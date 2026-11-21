import Foundation
import AppKit

// MARK: - Paths (contracts in docs/mac/launcher.md)

enum Paths {
    static let folderName = "Repentance+ Mac"
    static let env = ProcessInfo.processInfo.environment

    static var supportDir: URL {
        if let p = env["REPENTANCE_MAC_SUPPORT_DIR"], !p.isEmpty { return URL(fileURLWithPath: p) }
        return FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("Library/Application Support/\(folderName)")
    }
    static var modsDir: URL {
        if let p = env["REPENTANCE_MAC_MODS_DIR"], !p.isEmpty { return URL(fileURLWithPath: p) }
        return supportDir.appendingPathComponent("mods")
    }
    static var launcherSettings: URL { supportDir.appendingPathComponent("launcher-settings.json") }
    static var launchSettings: URL { supportDir.appendingPathComponent("launch-settings.json") }
    static var gateLog: URL {
        if let p = env["REPENTANCE_MAC_GATE_LOG"], !p.isEmpty { return URL(fileURLWithPath: p) }
        return supportDir.appendingPathComponent("logs/layer-gate.json")
    }
    static var modSync: URL { supportDir.appendingPathComponent("mod-sync.json") }
    static var gameLog: URL { supportDir.appendingPathComponent("logs/game.log") }
    static var layerInfo: URL {
        if let p = env["REPENTANCE_MAC_LAYER_INFO"], !p.isEmpty { return URL(fileURLWithPath: p) }
        return Bundle.main.bundleURL.appendingPathComponent("Contents/Resources/layer-info.json")
    }
    /// The build's own `Resources/sync-config.json` (scripts/mac/build.sh); lowest-precedence config.
    static var bundledSyncConfig: URL {
        if let p = env["REPENTANCE_MAC_BUNDLED_SYNC_CONFIG"], !p.isEmpty { return URL(fileURLWithPath: p) }
        return Bundle.main.bundleURL.appendingPathComponent("Contents/Resources/sync-config.json")
    }
    static var gameExecutable: URL {
        if let p = env["REPENTANCE_MAC_GAME_EXE"], !p.isEmpty { return URL(fileURLWithPath: p) }
        return Bundle.main.bundleURL.appendingPathComponent("Contents/MacOS/isaac-mac")
    }
}

// MARK: - NotchFill (optional, docs/mac/launcher.md)

enum NotchFill {
    static let libraryName = "libNotchFillHook.dylib"

    /// True when a built-in screen has a notch (macOS 12+: a non-empty `auxiliaryTopLeftArea`).
    /// `REPENTANCE_MAC_LAUNCHER_FORCE_NOTCH=1` shows the option on any Mac (UI tests).
    static var macHasNotch: Bool {
        if Paths.env["REPENTANCE_MAC_LAUNCHER_FORCE_NOTCH"] == "1" { return true }
        if #available(macOS 12.0, *) {
            return NSScreen.screens.contains { !($0.auxiliaryTopLeftArea ?? .zero).isEmpty }
        }
        return false
    }

    /// `<app>/Contents/Frameworks/libNotchFillHook.dylib`: next to the game program, else the app's own
    /// Frameworks folder. nil when the file is not there.
    static func libraryURL(gameExecutable: URL = Paths.gameExecutable,
                           privateFrameworks: String? = Bundle.main.privateFrameworksPath) -> URL? {
        var dirs = [gameExecutable.deletingLastPathComponent().deletingLastPathComponent()
                        .appendingPathComponent("Frameworks")]
        if let p = privateFrameworks { dirs.append(URL(fileURLWithPath: p)) }
        return dirs.map { $0.appendingPathComponent(libraryName) }
            .first { FileManager.default.fileExists(atPath: $0.path) }
    }

    /// The game's environment. Off: `DYLD_INSERT_LIBRARIES` is removed (never inherited). On: set to
    /// the library; when the file is missing `note` says so and the game starts without it.
    static func environment(base: [String: String], enabled: Bool, library: URL?, note: (String) -> Void)
        -> [String: String] {
        var env = base
        env["DYLD_INSERT_LIBRARIES"] = nil
        guard enabled else { return env }
        if let l = library { env["DYLD_INSERT_LIBRARIES"] = l.path }
        else { note("NotchFill is on but \(libraryName) was not found in the app; starting the game without it") }
        return env
    }
}

// MARK: - Settings

struct LauncherSettings: Codable, Equatable {
    var skipLauncher = false
    var vanillaLaunch = false
    var debugConsole = false
    var logWindow = false
    var luaDebug = false
    /// "adapt" = the game lays itself out at the window's pixel size (default); "integer" = whole-number
    /// scaling of the 960x540 picture with black bars; "fill" = stretch the picture to the window.
    var scaleMode = "adapt"
    /// Load the vendored NotchFill library into the game (full screen uses the strip beside the notch).
    var notchFill = false

    enum CodingKeys: String, CodingKey {
        case skipLauncher = "skip_launcher", vanillaLaunch = "vanilla_launch"
        case debugConsole = "debug_console", logWindow = "log_window", luaDebug = "lua_debug"
        case scaleMode = "scale_mode", notchFill = "notch_fill"
    }
    init() {}
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: CodingKeys.self)
        skipLauncher = try c.decodeIfPresent(Bool.self, forKey: .skipLauncher) ?? false
        vanillaLaunch = try c.decodeIfPresent(Bool.self, forKey: .vanillaLaunch) ?? false
        debugConsole = try c.decodeIfPresent(Bool.self, forKey: .debugConsole) ?? false
        logWindow = try c.decodeIfPresent(Bool.self, forKey: .logWindow) ?? false
        luaDebug = try c.decodeIfPresent(Bool.self, forKey: .luaDebug) ?? false
        let sm = try c.decodeIfPresent(String.self, forKey: .scaleMode) ?? "adapt"
        scaleMode = (sm == "fill" || sm == "integer") ? sm : "adapt"
        notchFill = try c.decodeIfPresent(Bool.self, forKey: .notchFill) ?? false
    }

    static func load() -> LauncherSettings {
        guard let d = try? Data(contentsOf: Paths.launcherSettings),
              let s = try? JSONDecoder().decode(LauncherSettings.self, from: d) else { return LauncherSettings() }
        return s
    }
    func save() {
        try? FileManager.default.createDirectory(at: Paths.supportDir, withIntermediateDirectories: true)
        let e = JSONEncoder(); e.outputFormatting = [.prettyPrinted, .sortedKeys]
        if let d = try? e.encode(self) { try? d.write(to: Paths.launcherSettings, options: .atomic) }
    }
}

// MARK: - Layer info and gate log

struct LayerInfo: Decodable {
    var version: Int?
    var layerVersion: String?
    var gameVersion: String?
    var implementedNames: Int?
    var unsupportedNames: Int?
    var knownNames: Int?
    var implementedCallbacks: Int?
    var knownCallbacks: Int?
    var builtAt: String?
    enum CodingKeys: String, CodingKey {
        case version, builtAt = "built_at"
        case layerVersion = "layer_version", gameVersion = "game_version"
        case implementedNames = "implemented_names", unsupportedNames = "unsupported_names"
        case knownNames = "known_names", implementedCallbacks = "implemented_callbacks"
        case knownCallbacks = "known_callbacks"
    }
    static func load() -> LayerInfo? {
        guard let d = try? Data(contentsOf: Paths.layerInfo) else { return nil }
        return try? JSONDecoder().decode(LayerInfo.self, from: d)
    }
    var coverageText: String? {
        guard let i = implementedNames, let k = knownNames, k > 0 else { return nil }
        let pct = Int((Double(i) / Double(k) * 100).rounded())
        var s = "\(i) of \(k) REPENTOGON names (\(pct)%)"
        if let ci = implementedCallbacks, let ck = knownCallbacks, ck > 0 { s += ", \(ci) of \(ck) callbacks" }
        return s
    }
}

struct GateRow: Decodable {
    var shown: Bool
    var why: String?
    var used: Int?
    var missing: [String]?
}
struct GateLog: Decodable {
    var version: Int?
    var layerOn: Bool?
    var writtenAt: String?
    var mods: [String: GateRow]
    enum CodingKeys: String, CodingKey { case version, mods, layerOn = "layer_on", writtenAt = "written_at" }
    static func load() -> GateLog? {
        guard let d = try? Data(contentsOf: Paths.gateLog) else { return nil }
        return try? JSONDecoder().decode(GateLog.self, from: d)
    }
}
struct ModSync: Decodable {
    var unsubscribed: [String]?
    static func load() -> Set<String> {
        guard let d = try? Data(contentsOf: Paths.modSync),
              let s = try? JSONDecoder().decode(ModSync.self, from: d) else { return [] }
        return Set((s.unsubscribed ?? []).map { $0.lowercased() })
    }
}

// MARK: - Mods

struct Mod: Identifiable {
    var id: String            // folder name
    var name: String
    var version: String
    var enabled: Bool
    var order: Int            // 1-based position in the game's load order (the order it logs `LOADED MOD` in)
    var unsubscribed: Bool
    var gate: GateRow?
    var isEpiphany: Bool { (id + " " + name).lowercased().contains("epiphany") }
    var folder: URL { Paths.modsDir.appendingPathComponent(id) }
}

/// The Mods tab's enabled/disabled filter. Enabled = no `disable.it` in the mod's folder (`Mod.enabled`).
enum ModStateFilter: String, CaseIterable {
    case all = "All", enabled = "Enabled", disabled = "Disabled"
}

enum ModFilter {
    /// Mods matching the search text and the state filter, in the given order. The search is a
    /// case-insensitive substring match on the display name or the folder name (the id); empty or
    /// whitespace-only text matches everything.
    static func apply(_ mods: [Mod], search: String, state: ModStateFilter) -> [Mod] {
        let q = search.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        return mods.filter { m in
            switch state {
            case .all: break
            case .enabled: if !m.enabled { return false }
            case .disabled: if m.enabled { return false }
            }
            return q.isEmpty || m.name.lowercased().contains(q) || m.id.lowercased().contains(q)
        }
    }

    /// (enabled, disabled) over all mods, ignoring the search.
    static func counts(_ mods: [Mod]) -> (enabled: Int, disabled: Int) {
        let e = mods.reduce(0) { $0 + ($1.enabled ? 1 : 0) }
        return (e, mods.count - e)
    }
}

enum ModStore {
    private static func tag(_ xml: String, _ name: String) -> String? {
        guard let r = xml.range(of: "<\(name)>[^<]*</\(name)>", options: .regularExpression) else { return nil }
        let s = String(xml[r]).dropFirst(name.count + 2).dropLast(name.count + 3)
        let t = s.trimmingCharacters(in: .whitespacesAndNewlines)
        return t.isEmpty ? nil : t
    }

    /// Folder names in the order the game loads them. The Host seeds the mods folder with
    /// `readdir` (mac_fs.c `mac_seed_mods`) and the game loads in that order, so this is the raw,
    /// unsorted directory order of the volume (APFS: by a hash of the name, not by name).
    /// Mirrors the Host's filter: dot-folders, non-directories and a folder named `repentogon` are not mods.
    static func gameLoadOrder(in dir: URL) -> [String] {
        guard let d = opendir(dir.path) else { return [] }
        defer { closedir(d) }
        var out: [String] = []
        while let e = readdir(d) {
            let name = withUnsafePointer(to: &e.pointee.d_name) {
                $0.withMemoryRebound(to: CChar.self, capacity: Int(NAME_MAX) + 1) { String(cString: $0) }
            }
            if name.hasPrefix(".") { continue }
            var st = stat()
            guard stat(dir.appendingPathComponent(name).path, &st) == 0, (st.st_mode & S_IFMT) == S_IFDIR else { continue }
            if name.lowercased() == "repentogon" { continue }
            out.append(name)
        }
        return out
    }

    /// The list is sorted like the in-game MODS menu (display name, case-insensitive); each row
    /// carries its position in the game's real load order (`gameLoadOrder`).
    static func load() -> [Mod] {
        let fm = FileManager.default
        let loadIndex = Dictionary(gameLoadOrder(in: Paths.modsDir).enumerated().map { ($1, $0 + 1) },
                                   uniquingKeysWith: { a, _ in a })
        let items = (try? fm.contentsOfDirectory(at: Paths.modsDir, includingPropertiesForKeys: [.isDirectoryKey])) ?? []
        let dirs = items.filter { (try? $0.resourceValues(forKeys: [.isDirectoryKey]).isDirectory) == true
            && !$0.lastPathComponent.hasPrefix(".") }
        let gate = GateLog.load()
        let unsub = ModSync.load()
        var mods: [Mod] = dirs.map { $0.lastPathComponent }.map { folder in
            let dir = Paths.modsDir.appendingPathComponent(folder)
            let xml = (try? String(contentsOf: dir.appendingPathComponent("metadata.xml"), encoding: .utf8)) ?? ""
            return Mod(id: folder, name: tag(xml, "name") ?? folder, version: tag(xml, "version") ?? "",
                       enabled: !fm.fileExists(atPath: dir.appendingPathComponent("disable.it").path),
                       order: loadIndex[folder] ?? 0, unsubscribed: unsub.contains(folder.lowercased()),
                       gate: gate?.mods[folder.lowercased()])
        }
        mods.sort {
            let c = $0.name.compare($1.name, options: [.caseInsensitive])
            return c == .orderedSame ? $0.id.compare($1.id, options: [.caseInsensitive]) == .orderedAscending
                                     : c == .orderedAscending
        }
        return mods
    }

    static func setEnabled(_ mod: Mod, _ on: Bool) {
        let mark = mod.folder.appendingPathComponent("disable.it")
        if on { try? FileManager.default.removeItem(at: mark) }
        else { FileManager.default.createFile(atPath: mark.path, contents: Data()) }
    }

    /// Moves the folder to the Trash. Never hard-deletes.
    static func moveToTrash(_ mod: Mod) throws {
        try FileManager.default.trashItem(at: mod.folder, resultingItemURL: nil)
    }
}
