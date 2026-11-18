import Foundation
import CryptoKit

// Shared pieces of the pre-launch work (save import, Workshop sync). No UI.
// Contracts: docs/mac/launcher.md ("Save import", "Workshop sync").

/// Every folder the pre-launch work touches, explicit so tests can point it at fixtures.
struct SyncDirs {
    var support: URL
    var mods: URL
    /// Where the game keeps its saves: <support>/saves. The game's own tree lives below it
    /// (`documents/my games/binding of isaac repentance+/`).
    var saves: URL
    /// Steam's own userdata folder (read only). nil = none.
    var steamUserdata: URL?
    /// Mac Steam's Workshop content folder for app 250900 (read only).
    var steamWorkshop: URL
    /// Extra Workshop folders tried, in order, when the Steam one has no mods (read only).
    var workshopFallbacks: [URL]
    /// Folders searched (read only) for an existing Repentance+ save, e.g. a Windows game folder.
    var saveSearch: [URL]

    var saveTree: URL { saves.appendingPathComponent(SaveImport.treeRel, isDirectory: true) }
    var log: URL { support.appendingPathComponent("logs/launcher.log") }
    var modSync: URL { support.appendingPathComponent("mod-sync.json") }
    var saveMarker: URL { support.appendingPathComponent("save-import.json") }
    /// What the game reads for the Layer's gate: the mod scan (ModScan.swift). The Host serves it at the
    /// virtual path `mods/repentogon/modscan.lua` (docs/mac/launcher.md "Mod scan").
    var modScanFile: URL { support.appendingPathComponent("modscan.lua") }
    var modScanCache: URL { support.appendingPathComponent("modscan-cache.json") }
    var replacedDir: URL { support.appendingPathComponent("mods-replaced") }
    var tmpDir: URL { support.appendingPathComponent(".sync-tmp") }

    /// The real locations, with environment overrides (see docs/mac/launcher.md "Files and paths").
    static func fromEnvironment() -> SyncDirs {
        let env = ProcessInfo.processInfo.environment
        let home = FileManager.default.homeDirectoryForCurrentUser
        let steam = home.appendingPathComponent("Library/Application Support/Steam")
        let support = Paths.supportDir
        func url(_ k: String) -> URL? { (env[k]).flatMap { $0.isEmpty ? nil : URL(fileURLWithPath: $0) } }
        func list(_ k: String) -> [URL] {
            (env[k] ?? "").split(separator: ":").map { URL(fileURLWithPath: String($0)) }
        }
        let cfg = SyncConfig.merged([SyncConfig.load(support.appendingPathComponent("sync-config.json")),
                                     SyncConfig.load(Paths.bundledSyncConfig)])
        return SyncDirs(
            support: support,
            mods: Paths.modsDir,
            saves: url("REPENTANCE_MAC_SAVE_DIR") ?? support.appendingPathComponent("saves"),
            steamUserdata: url("REPENTANCE_MAC_STEAM_USERDATA") ?? steam.appendingPathComponent("userdata"),
            steamWorkshop: url("REPENTANCE_MAC_WORKSHOP_DIR")
                ?? steam.appendingPathComponent("steamapps/workshop/content/250900"),
            workshopFallbacks: SyncConfig.unique(list("REPENTANCE_MAC_WORKSHOP_FALLBACK") + cfg.workshopFallback.map { URL(fileURLWithPath: $0) }),
            saveSearch: SyncConfig.unique(list("REPENTANCE_MAC_SAVE_SEARCH") + cfg.saveSearch.map { URL(fileURLWithPath: $0) }))
    }
}

/// `<support>/sync-config.json`: this machine's extra source folders. User-local, never committed:
/// `{ "workshop_fallback": ["/path/to/steamapps/workshop/content/250900"], "save_search": ["/path/to/game"] }`
struct SyncConfig: Decodable {
    var workshopFallback: [String] = []
    var saveSearch: [String] = []
    enum CodingKeys: String, CodingKey { case workshopFallback = "workshop_fallback", saveSearch = "save_search" }
    init() {}
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: CodingKeys.self)
        workshopFallback = try c.decodeIfPresent([String].self, forKey: .workshopFallback) ?? []
        saveSearch = try c.decodeIfPresent([String].self, forKey: .saveSearch) ?? []
    }
    /// Precedence order of the arguments: earlier wins (it comes first in each list; lists are searched in order).
    /// Used for `<support>/sync-config.json` first, then the bundle's `Resources/sync-config.json`.
    static func merged(_ cfgs: [SyncConfig]) -> SyncConfig {
        var m = SyncConfig()
        m.workshopFallback = unique(cfgs.flatMap { $0.workshopFallback })
        m.saveSearch = unique(cfgs.flatMap { $0.saveSearch })
        return m
    }
    static func unique<T: Hashable>(_ a: [T]) -> [T] { var seen = Set<T>(); return a.filter { seen.insert($0).inserted } }
    static func load(_ url: URL) -> SyncConfig {
        guard let d = try? Data(contentsOf: url), let c = try? JSONDecoder().decode(SyncConfig.self, from: d) else { return SyncConfig() }
        return c
    }
}

/// Appends timestamped lines to <support>/logs/launcher.log.
final class SyncLog {
    let url: URL
    private(set) var lines: [String] = []
    init(_ url: URL) { self.url = url }
    func add(_ s: String) {
        let line = "\(ISO8601DateFormatter().string(from: Date())) \(s)"
        lines.append(s)
        let fm = FileManager.default
        try? fm.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        if !fm.fileExists(atPath: url.path) { fm.createFile(atPath: url.path, contents: nil) }
        if let h = try? FileHandle(forWritingTo: url) {
            _ = try? h.seekToEnd()
            try? h.write(contentsOf: Data((line + "\n").utf8))
            try? h.close()
        }
    }
}

enum Hash {
    static func sha256(file url: URL) -> String? {
        guard let h = try? FileHandle(forReadingFrom: url) else { return nil }
        defer { try? h.close() }
        var sha = SHA256()
        while let d = try? h.read(upToCount: 1 << 20), !d.isEmpty { sha.update(data: d) }
        return sha.finalize().map { String(format: "%02x", $0) }.joined()
    }
    static func sha256(string s: String) -> String {
        SHA256.hash(data: Data(s.utf8)).map { String(format: "%02x", $0) }.joined()
    }
}

enum FS {
    static func isDir(_ u: URL) -> Bool {
        var d: ObjCBool = false
        return FileManager.default.fileExists(atPath: u.path, isDirectory: &d) && d.boolValue
    }
    static func exists(_ u: URL) -> Bool { FileManager.default.fileExists(atPath: u.path) }
    static func iso(_ d: Date = Date()) -> String { ISO8601DateFormatter().string(from: d) }
    static func writeJSON(_ obj: Any, to url: URL) {
        try? FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        if let d = try? JSONSerialization.data(withJSONObject: obj, options: [.prettyPrinted, .sortedKeys]) {
            try? d.write(to: url, options: .atomic)
        }
    }
    static func readJSON(_ url: URL) -> [String: Any]? {
        guard let d = try? Data(contentsOf: url) else { return nil }
        return (try? JSONSerialization.jsonObject(with: d)) as? [String: Any]
    }
}

/// Everything that runs before the game starts.
enum PreLaunch {
    /// Runs the save import, the Workshop sync, then the mod scan. Never throws; every decision goes to launcher.log.
    @discardableResult
    static func run(_ dirs: SyncDirs = .fromEnvironment()) -> [String] {
        let log = SyncLog(dirs.log)
        log.add("pre-launch: support=\(dirs.support.path)")
        SaveImport.run(dirs, log: log)
        WorkshopSync.run(dirs, log: log)
        ModScan.run(dirs, log: log)
        return log.lines
    }
}
