import Foundation

/// Workshop sync: copies each Workshop item into the Mac Build's own mods folder, as the PC game
/// does (folder `<directory from metadata.xml>_<Workshop id>`). The Workshop folder is only read.
enum WorkshopSync {
    /// "[BETA] REPENTOGON: Isaac Script Extender": a checker mod, switched off on its first copy only.
    static let checkerModID = "3127536138"

    struct Item { var id: String; var src: URL; var folder: String; var version: String; var fingerprint: String; var bytes: Int }

    static func tag(_ xml: String, _ name: String) -> String? {
        guard let r = xml.range(of: "<\(name)>[^<]*</\(name)>", options: .regularExpression) else { return nil }
        let s = String(xml[r]).dropFirst(name.count + 2).dropLast(name.count + 3)
        let t = s.trimmingCharacters(in: .whitespacesAndNewlines)
        return t.isEmpty ? nil : t
    }

    static func sanitize(_ s: String) -> String {
        var t = s.replacingOccurrences(of: "/", with: "-").replacingOccurrences(of: ":", with: "-")
        while t.hasPrefix(".") { t.removeFirst() }
        return t.trimmingCharacters(in: .whitespaces)
    }

    /// The Workshop source to use: Mac Steam's folder if it has mods, else the first fallback that does.
    static func chooseSource(_ dirs: SyncDirs, log: SyncLog) -> URL? {
        for (i, u) in ([dirs.steamWorkshop] + dirs.workshopFallbacks).enumerated() {
            let n = scan(u).count
            log.add("workshop sync: \(i == 0 ? "Steam" : "fallback") folder \(u.path): \(n) mod(s)")
            if n > 0 { return u }
        }
        return nil
    }

    static func scan(_ root: URL) -> [Item] {
        let fm = FileManager.default
        guard let names = try? fm.contentsOfDirectory(atPath: root.path) else { return [] }
        var items: [Item] = []
        for n in names.sorted() where n.allSatisfy({ $0.isNumber }) {
            let src = root.appendingPathComponent(n)
            guard FS.isDir(src), let xml = try? String(contentsOf: src.appendingPathComponent("metadata.xml"), encoding: .utf8) else { continue }
            let dir = tag(xml, "directory").map(sanitize) ?? ""
            let base = dir.isEmpty ? sanitize(tag(xml, "name") ?? "mod") : dir
            let (fp, bytes) = fingerprint(src)
            items.append(Item(id: n, src: src, folder: "\(base)_\(n)", version: tag(xml, "version") ?? "", fingerprint: fp, bytes: bytes))
        }
        return items
    }

    /// Hash of every file's (or, with `include`, every selected file's) relative path, size and mtime (cheap; no file is read).
    static func fingerprint(_ dir: URL, include: (String) -> Bool = { _ in true }) -> (String, Int) {
        var lines: [String] = []
        var total = 0
        let keys: [URLResourceKey] = [.isRegularFileKey, .fileSizeKey, .contentModificationDateKey]
        if let e = FileManager.default.enumerator(at: dir, includingPropertiesForKeys: keys) {
            let base = dir.standardizedFileURL.path.count + 1
            for case let u as URL in e {
                guard let v = try? u.resourceValues(forKeys: Set(keys)), v.isRegularFile == true else { continue }
                let rel = String(u.standardizedFileURL.path.dropFirst(base))
                if rel.hasSuffix(".DS_Store") || !include(rel) { continue }
                let size = v.fileSize ?? 0
                total += size
                lines.append("\(rel)|\(size)|\(Int(v.contentModificationDate?.timeIntervalSince1970 ?? 0))")
            }
        }
        return (Hash.sha256(string: lines.sorted().joined(separator: "\n")), total)
    }

    static func freeBytes(_ url: URL) -> Int64? {
        var u = url
        while !FS.exists(u), u.path != "/" { u = u.deletingLastPathComponent() }
        return (try? u.resourceValues(forKeys: [.volumeAvailableCapacityForImportantUsageKey]))?.volumeAvailableCapacityForImportantUsage
    }

    static func run(_ dirs: SyncDirs, log: SyncLog) {
        let fm = FileManager.default
        guard let source = chooseSource(dirs, log: log) else {
            log.add("workshop sync: no Workshop mods found; mods folder and mod-sync.json left as they are")
            return
        }
        let items = scan(source)
        var state = FS.readJSON(dirs.modSync) ?? [:]
        var known = (state["items"] as? [String: [String: Any]]) ?? [:]
        try? fm.createDirectory(at: dirs.mods, withIntermediateDirectories: true)
        try? fm.removeItem(at: dirs.tmpDir)                    // leftovers of an interrupted sync (ours)
        try? fm.createDirectory(at: dirs.tmpDir, withIntermediateDirectories: true)

        // What needs copying?
        enum Why { case new, changed, adopted }
        var todo: [(Item, Why)] = []
        for it in items {
            let dest = dirs.mods.appendingPathComponent(it.folder)
            if let k = known[it.id], (k["folder"] as? String) == it.folder, FS.isDir(dest) {
                if (k["fingerprint"] as? String) != it.fingerprint || (k["version"] as? String) != it.version { todo.append((it, .changed)) }
            } else if FS.isDir(dest) {
                if let xml = try? String(contentsOf: dest.appendingPathComponent("metadata.xml"), encoding: .utf8), tag(xml, "id") == it.id {
                    todo.append((it, .adopted))            // a copy from before the manifest existed
                } else {
                    log.add("workshop sync: \(it.folder) exists in mods/ and is not this Workshop item; left alone")
                }
            } else {
                todo.append((it, .new))
            }
        }

        let need = Int64(todo.reduce(0) { $0 + $1.0.bytes })
        if need > 0, let free = freeBytes(dirs.mods), free < need + 1 << 30 {
            log.add("workshop sync: not enough free disk space (\(free / 1_000_000) MB free, \(need / 1_000_000) MB + 1 GB wanted); nothing copied")
            todo = []
        }

        var copied = 0
        for (it, why) in todo {
            let dest = dirs.mods.appendingPathComponent(it.folder)
            let tmp = dirs.tmpDir.appendingPathComponent(it.folder)
            do {
                try? fm.removeItem(at: tmp)
                try fm.copyItem(at: it.src, to: tmp)           // clonefile on APFS
                var disabled = false
                if why != .new {
                    disabled = FS.exists(dest.appendingPathComponent("disable.it"))
                    // the old copy is kept, one generation, outside mods/
                    try fm.createDirectory(at: dirs.replacedDir, withIntermediateDirectories: true)
                    let old = dirs.replacedDir.appendingPathComponent(it.folder)
                    try? fm.removeItem(at: old)
                    try fm.moveItem(at: dest, to: old)
                } else if let oldName = known[it.id]?["folder"] as? String, oldName != it.folder,
                          FS.isDir(dirs.mods.appendingPathComponent(oldName)) {
                    // the mod's <directory> changed: the old folder is replaced, its on/off state carried over
                    let oldDir = dirs.mods.appendingPathComponent(oldName)
                    disabled = FS.exists(oldDir.appendingPathComponent("disable.it"))
                    try fm.createDirectory(at: dirs.replacedDir, withIntermediateDirectories: true)
                    let old = dirs.replacedDir.appendingPathComponent(oldName)
                    try? fm.removeItem(at: old)
                    try fm.moveItem(at: oldDir, to: old)
                } else if it.id == checkerModID {
                    disabled = true                            // off by default, first copy only
                }
                try fm.moveItem(at: tmp, to: dest)
                let mark = dest.appendingPathComponent("disable.it")
                if disabled { if !FS.exists(mark) { fm.createFile(atPath: mark.path, contents: Data()) } }
                else { try? fm.removeItem(at: mark) }
                known[it.id] = ["folder": it.folder, "version": it.version, "fingerprint": it.fingerprint, "synced_at": FS.iso()]
                copied += 1
                log.add("workshop sync: \(why == .new ? "new" : why == .changed ? "updated" : "adopted") \(it.folder) (version \(it.version))\(disabled ? ", disable.it " + (why == .new ? "set" : "kept") : "")")
            } catch {
                try? fm.removeItem(at: tmp)
                log.add("workshop sync: FAILED \(it.folder): \(error.localizedDescription)")
            }
        }
        // gone from the Workshop: keep the folder, list it
        let present = Set(items.map { $0.id })
        var unsub: [String] = []
        for (id, k) in known where !present.contains(id) {
            guard let folder = k["folder"] as? String else { continue }
            if FS.isDir(dirs.mods.appendingPathComponent(folder)) { unsub.append(folder) } else { known[id] = nil }
        }
        unsub.sort { $0.lowercased() < $1.lowercased() }
        if !unsub.isEmpty { log.add("workshop sync: no longer subscribed (kept): \(unsub.joined(separator: ", "))") }

        state["version"] = 1
        state["unsubscribed"] = unsub
        state["items"] = known
        state["source"] = source.path
        state["synced_at"] = FS.iso()
        FS.writeJSON(state, to: dirs.modSync)
        try? fm.removeItem(at: dirs.tmpDir)
        log.add("workshop sync: \(items.count) Workshop mod(s), \(copied) copied, \(items.count - copied) unchanged or skipped, \(unsub.count) unsubscribed")
    }
}
