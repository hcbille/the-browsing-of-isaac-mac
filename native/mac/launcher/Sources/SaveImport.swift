import Foundation

/// Save import (ADR 0002): the user's existing Repentance+ save is copied into the Mac Build's own
/// save folder once, never written back, never over an existing Mac save. Source folders are only read.
enum SaveImport {
    /// The game's tree under the save dir (the Host keys saves as `documents/my games/...`).
    static let treeRel = "documents/my games/binding of isaac repentance+"

    /// persistentgamedata1..3.dat, optionally with a "rep+" prefix as in some cloud listings.
    /// (The game also has gamestate*.dat and rep+rerunstate*.dat: a run in progress is not imported.)
    static func slot(of name: String) -> Int? {
        let n = name.lowercased()
        for s in 1...3 where n == "persistentgamedata\(s).dat" || n == "rep+persistentgamedata\(s).dat" { return s }
        return nil
    }
    static func targetName(_ slot: Int) -> String { "persistentgamedata\(slot).dat" }

    /// Any save file the game itself wrote (persistent data, run states) = the Mac save exists.
    static func macSaveExists(_ dirs: SyncDirs) -> Bool {
        let names = (try? FileManager.default.contentsOfDirectory(atPath: dirs.saveTree.path)) ?? []
        return names.contains { n in
            let l = n.lowercased()
            return l.hasSuffix(".dat") && (l.contains("persistentgamedata") || l.contains("gamestate") || l.contains("rerunstate"))
        }
    }

    struct Candidate { var dir: URL; var files: [Int: URL]; var newest: Date; var origin: String }

    static func candidate(in dir: URL, origin: String) -> Candidate? {
        let fm = FileManager.default
        guard let names = try? fm.contentsOfDirectory(atPath: dir.path) else { return nil }
        var files: [Int: URL] = [:]
        var newest = Date.distantPast
        for n in names.sorted() {
            guard let s = slot(of: n) else { continue }
            let u = dir.appendingPathComponent(n)
            let a = try? fm.attributesOfItem(atPath: u.path)
            guard (a?[.type] as? FileAttributeType) == .typeRegular, ((a?[.size] as? Int) ?? 0) > 0 else { continue }
            // a plain name beats the "rep+" form for the same slot
            if files[s] == nil || n.lowercased() == targetName(s) { files[s] = u }
            newest = max(newest, (a?[.modificationDate] as? Date) ?? .distantPast)
        }
        return files.isEmpty ? nil : Candidate(dir: dir, files: files, newest: newest, origin: origin)
    }

    /// Steam userdata/<id>/250900/remote folders first, then a bounded read-only walk of the search folders.
    static func findCandidates(_ dirs: SyncDirs) -> [Candidate] {
        let fm = FileManager.default
        var out: [Candidate] = []
        if let ud = dirs.steamUserdata,
           let users = try? fm.contentsOfDirectory(atPath: ud.path) {
            for u in users.sorted() {
                let remote = ud.appendingPathComponent("\(u)/250900/remote")
                if let c = candidate(in: remote, origin: "steam-cloud") { out.append(c) }
            }
        }
        for root in dirs.saveSearch {
            guard let e = fm.enumerator(at: root, includingPropertiesForKeys: [.isDirectoryKey],
                                        options: [.skipsPackageDescendants]) else { continue }
            for case let u as URL in e {
                if e.level > 7 { e.skipDescendants(); continue }
                let n = u.lastPathComponent
                if FS.isDir(u) {
                    if n == "workshop" || n == "node_modules" || n == ".git" { e.skipDescendants() }
                    continue
                }
                if slot(of: n) != nil {
                    let dir = u.deletingLastPathComponent()
                    if !out.contains(where: { $0.dir == dir }), let c = candidate(in: dir, origin: "search") { out.append(c) }
                }
            }
        }
        return out
    }

    @discardableResult
    static func run(_ dirs: SyncDirs, log: SyncLog) -> Bool {
        let fm = FileManager.default
        if let m = FS.readJSON(dirs.saveMarker) {
            log.add("save import: already decided (\(m["status"] as? String ?? "done") at \(m["imported_at"] as? String ?? "?")); nothing to do")
            return false
        }
        if macSaveExists(dirs) {
            // A Mac save the user did not import (a fresh game, or a copy by hand): never touched, never imported over.
            log.add("save import: a Mac save already exists in \(dirs.saveTree.path); leaving it untouched, import will not happen")
            FS.writeJSON(["version": 1, "status": "skipped-existing-mac-save", "imported_at": FS.iso(), "files": []] as [String: Any],
                         to: dirs.saveMarker)
            return false
        }
        let cands = findCandidates(dirs)
        guard !cands.isEmpty else {
            log.add("save import: no existing Repentance+ save found; the game will create a fresh save")
            return false
        }
        for c in cands { log.add("save import: candidate \(c.dir.path) [\(c.origin)] slots \(c.files.keys.sorted())") }
        // Steam Cloud before folder search, then the newest.
        let best = cands.sorted {
            if ($0.origin == "steam-cloud") != ($1.origin == "steam-cloud") { return $0.origin == "steam-cloud" }
            return $0.newest > $1.newest
        }[0]

        do {
            try fm.createDirectory(at: dirs.saveTree, withIntermediateDirectories: true)
            var recorded: [[String: Any]] = []
            for (s, src) in best.files.sorted(by: { $0.key < $1.key }) {
                let dst = dirs.saveTree.appendingPathComponent(targetName(s))
                if FS.exists(dst) { log.add("save import: \(dst.lastPathComponent) exists, skipped"); continue }
                let tmp = dirs.saveTree.appendingPathComponent(".import-\(s).tmp")
                try? fm.removeItem(at: tmp)
                try fm.copyItem(at: src, to: tmp)               // source is only read
                guard let h1 = Hash.sha256(file: src), let h2 = Hash.sha256(file: tmp), h1 == h2 else {
                    try? fm.removeItem(at: tmp)
                    log.add("save import: copy of \(src.lastPathComponent) did not verify; abandoned")
                    continue
                }
                try fm.moveItem(at: tmp, to: dst)               // throws rather than overwrite
                let size = (try? fm.attributesOfItem(atPath: dst.path)[.size] as? Int) ?? 0
                recorded.append(["name": targetName(s), "from": src.path, "sha256": h1, "bytes": size])
                log.add("save import: imported \(src.path) -> \(dst.path) (\(size) bytes, sha256 \(h1))")
            }
            guard !recorded.isEmpty else { return false }
            FS.writeJSON(["version": 1, "status": "imported", "imported_at": FS.iso(), "source": best.dir.path,
                          "origin": best.origin, "files": recorded] as [String: Any], to: dirs.saveMarker)
            return true
        } catch {
            log.add("save import: failed (\(error.localizedDescription)); no marker written, will retry next launch")
            return false
        }
    }
}
