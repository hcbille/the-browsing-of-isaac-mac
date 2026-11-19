import Foundation

/// What each enabled mod's Lua says about REPENTOGON, for the Layer's gate.
///
/// A faithful Swift port of `createModScan` / `stripLua` / `scanLua` in
/// `scripts/recomp/web/mods.mjs` (round 122). The output of `ModScan.lua(_:)` is byte-identical to the
/// JS `scan.lua()` for the same mods (Tests/run.sh checks it). The code works on UTF-16 code units, as
/// the JS regexes do, so `\s`, `\b` and the token rules mean what they mean there. Where the JS is odd
/// (an unterminated string swallows its newline into a quote), this is odd the same way, on purpose.
/// Contract: docs/mac/launcher.md "Mod scan".
enum ModScan {
    /// Bump when the scanning rules change: it invalidates every cached row.
    static let scannerVersion = 2
    static let namesMax = 20000
    static let layerFolder = "repentogon"

    typealias U = [UInt16]

    struct Row: Equatable {
        var says = 0, quits = 0, yields = 0, files = 0
        var names: Set<String> = []
    }
    struct FileFacts {
        var says = 0, quits = 0, yields = 0
        var names: Set<String> = [], defined: Set<String> = []
    }

    // MARK: character classes (JS semantics)

    @inline(__always) private static func c(_ s: Unicode.Scalar) -> UInt16 { UInt16(s.value) }
    @inline(__always) private static func isUpper(_ x: UInt16) -> Bool { x >= 65 && x <= 90 }
    @inline(__always) private static func isLower(_ x: UInt16) -> Bool { x >= 97 && x <= 122 }
    @inline(__always) private static func isDigit(_ x: UInt16) -> Bool { x >= 48 && x <= 57 }
    @inline(__always) private static func isIdStart(_ x: UInt16) -> Bool { isUpper(x) || isLower(x) || x == 95 }
    @inline(__always) private static func isWord(_ x: UInt16) -> Bool { isIdStart(x) || isDigit(x) }
    /// JS `\s`: WhiteSpace and LineTerminator.
    @inline(__always) private static func isSpace(_ x: UInt16) -> Bool {
        switch x {
        case 9, 10, 11, 12, 13, 32, 0xA0, 0x1680, 0x2028, 0x2029, 0x202F, 0x205F, 0x3000, 0xFEFF: return true
        default: return x >= 0x2000 && x <= 0x200A
        }
    }
    private static func str(_ a: U, _ from: Int, _ to: Int) -> String {
        String(decoding: a[from..<to], as: UTF16.self)
    }

    /// TextDecoder('utf-8') as the page uses it: invalid bytes become U+FFFD, a leading BOM is dropped.
    static func decode(_ d: Data) -> U {
        var bytes = d[...]
        if bytes.starts(with: [0xEF, 0xBB, 0xBF]) { bytes = bytes.dropFirst(3) }
        return Array(String(decoding: bytes, as: UTF8.self).utf16)
    }

    // MARK: stripLua

    /// Lua source with comments and string contents blanked (newlines kept), so that what is read afterwards is code.
    static func stripLua(_ s: U) -> U {
        let n = s.count
        var out = U()
        out.reserveCapacity(n)
        var i = 0, from = 0
        @inline(__always) func at(_ p: Int) -> UInt16 { p >= 0 && p < n ? s[p] : 0 }
        func blank(_ a: Int, _ b: Int) {
            var k = a
            while k < b { out.append(s[k] == 10 ? 10 : 32); k += 1 }
        }
        func longAt(_ p: Int) -> Int {
            if at(p) != c("[") { return -1 }
            var q = p + 1
            while at(q) == c("=") { q += 1 }
            return at(q) == c("[") ? q - p - 1 : -1
        }
        func longEnd(_ p: Int, _ level: Int) -> Int {
            let len = level + 2
            var e = p + level + 2
            while e + len <= n {
                if s[e] == c("]") && s[e + len - 1] == c("]") {
                    var ok = true
                    var k = 1
                    while k < len - 1 { if s[e + k] != c("=") { ok = false; break }; k += 1 }
                    if ok { return e + len }
                }
                e += 1
            }
            return n
        }
        func cut(_ a: Int, _ b: Int, _ quote: Bool) {
            if from < a { out.append(contentsOf: s[from..<a]) }
            if quote {
                out.append(34)
                blank(a + 1, max(a + 1, b - 1))
                if b - 1 > a { out.append(34) }
            } else { blank(a, b) }
            from = b
        }
        while i < n {
            let ch = s[i]
            if ch == c("-") && at(i + 1) == c("-") {
                let level = longAt(i + 2)
                var e: Int
                if level >= 0 { e = longEnd(i + 2, level) } else {
                    e = i
                    while e < n && s[e] != 10 { e += 1 }
                }
                cut(i, e, false); i = e
            } else if ch == c("\"") || ch == c("'") {
                var e = i + 1
                while e < n && s[e] != ch && s[e] != 10 { e += s[e] == c("\\") ? 2 : 1 }
                e = min(n, e + 1)
                cut(i, e, true); i = e
            } else if ch == c("[") && longAt(i) >= 0 {
                let e = longEnd(i, longAt(i))
                cut(i, e, true); i = e
            } else { i += 1 }
        }
        if from < n { out.append(contentsOf: s[from..<n]) }
        return out
    }

    // MARK: scanLua

    private static let kFunction = Array("function".utf16)
    private static let kRepentogon = Array("REPENTOGON".utf16)

    private static func matches(_ a: U, _ p: Int, _ lit: U) -> Bool {
        if p + lit.count > a.count { return false }
        for k in 0..<lit.count where a[p + k] != lit[k] { return false }
        return true
    }

    /// What the code shows of a call's receiver (`receiverHint` in mods.mjs): ".id" for `id:Name(`, "!callee" for
    /// `callee(...):Name(`, "" for anything else. `at` is the index of the `.` or `:` before the name.
    static func receiverHint(_ code: U, _ at: Int) -> String {
        var k = at - 1
        while k >= 0 && isSpace(code[k]) { k -= 1 }
        if k < 0 { return "" }
        if isWord(code[k]) {
            var b = k
            while b >= 0 && isWord(code[b]) { b -= 1 }
            if !isIdStart(code[b + 1]) { return "" }
            var j = b
            while j >= 0 && isSpace(code[j]) { j -= 1 }
            if j >= 0 && (code[j] == c(".") || code[j] == c(":")) && !(j > 0 && code[j - 1] == code[j]) { return "" }
            return "." + str(code, b + 1, k + 1)
        }
        if code[k] == c(")") {
            var depth = 0
            var o = k
            while o >= 0 {
                if code[o] == c(")") { depth += 1 }
                else if code[o] == c("(") { depth -= 1; if depth == 0 { break } }
                o -= 1
            }
            if o < 0 { return "" }
            var e = o - 1
            while e >= 0 && isSpace(code[e]) { e -= 1 }
            if e < 0 || !isWord(code[e]) { return "" }
            var b = e
            while b >= 0 && isWord(code[b]) { b -= 1 }
            return isIdStart(code[b + 1]) ? "!" + str(code, b + 1, e + 1) : ""
        }
        return ""
    }

    /// One file's facts. Same rules as the JS: see the comment above `scanLua` in mods.mjs.
    static func scanLua(_ src: U) -> FileFacts {
        let code = stripLua(src)
        let n = code.count
        var f = FileFacts()
        let dot = c("."), colon = c(":"), paren = c("(")

        // SCAN_CALLS  /[.:]\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(/g
        var p = 0
        while p < n {
            let ch = code[p]
            if ch == dot || ch == colon {
                var q = p + 1
                while q < n && isSpace(code[q]) { q += 1 }
                if q < n && isIdStart(code[q]) {
                    let st = q
                    q += 1
                    while q < n && isWord(code[q]) { q += 1 }
                    let en = q
                    while q < n && isSpace(code[q]) { q += 1 }
                    if q < n && code[q] == paren { f.names.insert(str(code, st, en) + receiverHint(code, p)) }
                }
            }
            p += 1
        }
        // SCAN_CALLBACKS  /\bMC_[A-Z0-9_]+\b/g
        p = 0
        while p + 3 < n {
            if code[p] == c("M") && code[p + 1] == c("C") && code[p + 2] == c("_") && (p == 0 || !isWord(code[p - 1])) {
                var q = p + 3
                while q < n && (isUpper(code[q]) || isDigit(code[q]) || code[q] == 95) { q += 1 }
                if q > p + 3 && (q == n || !isWord(code[q])) { f.names.insert(str(code, p, q)); p = q; continue }
            }
            p += 1
        }
        // SCAN_TABLES  /(?:^|[^.:\w])([A-Z][A-Za-z0-9_]*)\s*[.:(]/g, with lastIndex semantics
        func tableAt(_ s: Int) -> (String, Int)? {
            guard s < n, isUpper(code[s]) else { return nil }
            var q = s + 1
            while q < n && isWord(code[q]) { q += 1 }
            let en = q
            while q < n && isSpace(code[q]) { q += 1 }
            guard q < n, code[q] == dot || code[q] == colon || code[q] == paren else { return nil }
            return (str(code, s, en), q + 1)
        }
        p = 0
        while p <= n {
            var hit: (String, Int)? = p == 0 ? tableAt(0) : nil
            if hit == nil, p < n, code[p] != dot, code[p] != colon, !isWord(code[p]) { hit = tableAt(p + 1) }
            if let (name, end) = hit { f.names.insert(name); p = end } else { p += 1 }
        }
        // SCAN_DEFINED
        p = 0
        while p < n {
            let ch = code[p]
            if ch == c("f") && (p == 0 || !isWord(code[p - 1])) && matches(code, p, kFunction) {
                var q = p + 8
                let sp = q
                while q < n && isSpace(code[q]) { q += 1 }
                if q > sp {
                    var name: String?
                    var end = 0
                    while true {
                        guard q < n, isIdStart(code[q]) else { break }
                        let st = q
                        q += 1
                        while q < n && isWord(code[q]) { q += 1 }
                        let en = q
                        while q < n && isSpace(code[q]) { q += 1 }
                        if q < n && (code[q] == dot || code[q] == colon) {
                            q += 1
                            while q < n && isSpace(code[q]) { q += 1 }
                            continue
                        }
                        if q < n && code[q] == paren { name = str(code, st, en); end = q + 1 }
                        break
                    }
                    if let name { f.defined.insert(name); p = end; continue }
                }
            } else if ch == dot || ch == colon {
                var q = p + 1
                while q < n && isSpace(code[q]) { q += 1 }
                if q < n && isIdStart(code[q]) {
                    let st = q
                    q += 1
                    while q < n && isWord(code[q]) { q += 1 }
                    let en = q
                    while q < n && isSpace(code[q]) { q += 1 }
                    if q < n && code[q] == c("=") {
                        q += 1
                        while q < n && isSpace(code[q]) { q += 1 }
                        if matches(code, q, kFunction) && (q + 8 == n || !isWord(code[q + 8])) {
                            f.defined.insert(str(code, st, en)); p = q + 8; continue
                        }
                    }
                }
            }
            p += 1
        }
        f.names.remove("REPENTOGON")
        // says: /\bREPENTOGON\b/g
        p = 0
        var says = 0
        while p < n {
            if code[p] == c("R") && (p == 0 || !isWord(code[p - 1])) && matches(code, p, kRepentogon)
                && (p + 10 == n || !isWord(code[p + 10])) { says += 1; p += 10 } else { p += 1 }
        }
        f.says = says
        if says == 0 { return f }

        // tokens: /[A-Za-z_][A-Za-z0-9_]*|==|~=|[()]|[^\sA-Za-z_0-9()]/g
        var tok: [String] = []
        p = 0
        while p < n {
            let ch = code[p]
            if isIdStart(ch) {
                var q = p + 1
                while q < n && isWord(code[q]) { q += 1 }
                tok.append(str(code, p, q)); p = q
            } else if (ch == c("=") || ch == c("~")) && p + 1 < n && code[p + 1] == c("=") {
                tok.append(ch == c("=") ? "==" : "~="); p += 2
            } else if isSpace(ch) || isDigit(ch) { p += 1 } else {
                tok.append(str(code, p, p + 1)); p += 1
            }
        }
        let T = tok.count
        @inline(__always) func t(_ i: Int) -> String { i >= 0 && i < T ? tok[i] : "" }

        var quits = 0, yields = 0, fdepth = 0
        var stack: [String] = []
        struct Guard { var depth: Int; var live: Bool; var hit: Bool }
        var guards: [Guard] = []
        var i = 0
        while i < T {
            let tk = tok[i]
            defer { i += 1 }
            if tk == "assert" && fdepth == 0 && t(i + 1) == "(" && t(i + 2) == "REPENTOGON" { quits += 1 }
            if tk == "if" {
                var cond: [String] = []
                var j = i + 1
                while j < T && tok[j] != "then" && cond.count < 40 {
                    if tok[j] != "(" && tok[j] != ")" { cond.append(tok[j]) }
                    j += 1
                }
                @inline(__always) func cd(_ k: Int) -> String { k < cond.count ? cond[k] : "" }
                let present = cond.count == 1 && cond[0] == "REPENTOGON"
                if present && t(j + 1) == "return" {
                    let v = t(j + 2)
                    let k = (v == "true" || v == "false" || v == "nil") ? j + 3 : j + 2
                    if t(k) == "end" { yields += 1 }
                }
                stack.append("if")
                if fdepth == 0 {
                    let absent = (cd(0) == "not" && cd(1) == "REPENTOGON" && (cond.count == 2 || cd(2) == "or"))
                        || (cd(0) == "REPENTOGON" && cd(1) == "==" && cd(2) == "nil" && (cond.count == 3 || cd(3) == "or"))
                        || (cd(0) == "nil" && cd(1) == "==" && cd(2) == "REPENTOGON" && (cond.count == 3 || cd(3) == "or"))
                    if absent || present { guards.append(Guard(depth: stack.count, live: absent, hit: false)) }
                }
                continue
            }
            if tk == "function" || tk == "do" || tk == "repeat" {
                stack.append(tk)
                if tk == "function" { fdepth += 1 }
                continue
            }
            if tk == "end" || tk == "until" {
                if let g = guards.last, g.depth == stack.count {
                    if g.hit { quits += 1 }
                    guards.removeLast()
                }
                if stack.popLast() == "function" { fdepth -= 1 }
                continue
            }
            guard var g = guards.last else { continue }
            if (tk == "else" || tk == "elseif") && g.depth == stack.count {
                if g.live {
                    if g.hit { quits += 1 }
                    guards.removeLast()
                } else if tk == "else" {
                    g.live = true; guards[guards.count - 1] = g
                } else { guards.removeLast() }
                continue
            }
            if g.live && fdepth == 0 && (tk == "return" || (tk == "error" && (t(i + 1) == "(" || t(i + 1) == "\""))) {
                g.hit = true; guards[guards.count - 1] = g
            }
        }
        f.quits = quits; f.yields = yields
        return f
    }

    // MARK: one mod

    /// `add(id, files)` of the JS: `files` are (relative path, bytes). nil = the mod's code never names REPENTOGON.
    static func scanMod(_ files: [(rel: String, bytes: Data)]) -> Row? {
        let lua = files.filter { $0.rel.lowercased().hasSuffix(".lua") }
        // fast path of `texts.some(t.includes('REPENTOGON'))`: a byte search (decoding cannot make or break ASCII)
        let needle = Data("REPENTOGON".utf8)
        guard lua.contains(where: { $0.bytes.range(of: needle) != nil }) else { return nil }
        var row = Row()
        row.files = lua.count
        var defined = Set<String>()
        for f in lua {
            let rel = f.rel.replacingOccurrences(of: "\\", with: "/").lowercased()
            let r = scanLua(decode(f.bytes))
            row.says += r.says; row.yields += r.yields
            if rel == "main.lua" { row.quits += r.quits }
            row.names.formUnion(r.names)
            defined.formUnion(r.defined)
        }
        if row.says == 0 { return nil }
        row.names = row.names.filter { t in
            let base = t.split(whereSeparator: { $0 == "." || $0 == "!" }).first.map(String.init) ?? t
            return !defined.contains(base)
        }
        return row
    }

    // MARK: the file

    /// `lua()` of the JS scan: the file the Layer loads (`B.load("modscan.lua")`).
    static func lua(_ mods: [String: Row]) -> String {
        func q(_ s: String) -> String {
            var out: U = [34]
            for u in s.utf16 {
                if isWord(u) || u == 32 || u == 46 || u == 33 || u == 43 || u == 45 { out.append(u) } else { out.append(95) }
            }
            out.append(34)
            return String(decoding: out, as: UTF16.self)
        }
        func validName(_ s: String) -> Bool {
            // Name, or Name.hint / Name!hint
            let parts = s.utf16.split(maxSplits: 1, omittingEmptySubsequences: false) { $0 == 46 || $0 == 33 }.map(Array.init)
            return parts.count >= 1 && parts.allSatisfy { !$0.isEmpty && isIdStart($0[0]) && $0.allSatisfy(isWord) }
        }
        let keyed = mods.map { (key: $0.key.lowercased(), row: $0.value) }
            .sorted { Array($0.key.utf16).lexicographicallyPrecedes(Array($1.key.utf16)) }
        var rows: [String] = []
        for (id, r) in keyed {
            let names = r.names.filter(validName).sorted { Array($0.utf8).lexicographicallyPrecedes(Array($1.utf8)) }.prefix(namesMax)
            rows.append("  [\(q(id))] = { says = \(r.says), quits = \(r.quits), yields = \(r.yields), files = \(r.files), names = \(q(names.joined(separator: " "))) },")
        }
        return "-- generated by the page at this start (mods.mjs createModScan): what the Lua of each installed mod says about REPENTOGON\n"
            + "return { version = 1, mods = {\n\(rows.joined(separator: "\n"))\n} }\n"
    }

    // MARK: directory scan with a per-mod cache

    struct Stats { var enabled = 0, naming = 0, scanned = 0, cached = 0, ms = 0 }

    /// Every file under `dir` (relative path with `/`), not following symlinks.
    static func files(in dir: URL) -> [(rel: String, url: URL)] {
        var out: [(String, URL)] = []
        let keys: [URLResourceKey] = [.isRegularFileKey]
        guard let e = FileManager.default.enumerator(at: dir, includingPropertiesForKeys: keys) else { return [] }
        let base = dir.standardizedFileURL.path.count + 1
        for case let u as URL in e {
            guard (try? u.resourceValues(forKeys: Set(keys)))?.isRegularFile == true else { continue }
            out.append((String(u.standardizedFileURL.path.dropFirst(base)), u))
        }
        return out
    }

    /// The enabled mods of `modsDir` (a folder without `disable.it`; the Layer's own folder and dot-folders left out) -> rows.
    /// `cache`: folder -> (fingerprint, row?) from the last run; updated in place.
    static func scanDirectory(_ modsDir: URL, cache: inout [String: CacheEntry], useCache: Bool = true) -> (mods: [String: Row], stats: Stats) {
        let t0 = Date()
        var stats = Stats()
        var result: [String: Row] = [:]
        var newCache: [String: CacheEntry] = [:]
        let names = ((try? FileManager.default.contentsOfDirectory(atPath: modsDir.path)) ?? []).sorted()
        for name in names where !name.hasPrefix(".") && name.lowercased() != layerFolder {
            let dir = modsDir.appendingPathComponent(name)
            guard FS.isDir(dir), !FS.exists(dir.appendingPathComponent("disable.it")) else { continue }
            stats.enabled += 1
            let fp = WorkshopSync.fingerprint(dir, include: { $0.lowercased().hasSuffix(".lua") }).0
            if useCache, let hit = cache[name], hit.fingerprint == fp {
                stats.cached += 1
                newCache[name] = hit
                if let r = hit.row { result[name] = r }
                continue
            }
            let fl = files(in: dir).filter { $0.rel.lowercased().hasSuffix(".lua") }
                .map { (rel: $0.rel, bytes: (try? Data(contentsOf: $0.url)) ?? Data()) }
            let row = scanMod(fl)
            stats.scanned += 1
            newCache[name] = CacheEntry(fingerprint: fp, row: row)
            if let row { result[name] = row }
        }
        cache = newCache
        stats.naming = result.count
        stats.ms = Int(Date().timeIntervalSince(t0) * 1000)
        return (result, stats)
    }

    struct CacheEntry: Equatable { var fingerprint: String; var row: Row? }

    static func loadCache(_ url: URL) -> [String: CacheEntry] {
        guard let j = FS.readJSON(url), (j["scanner"] as? Int) == scannerVersion, let mods = j["mods"] as? [String: [String: Any]] else { return [:] }
        var out: [String: CacheEntry] = [:]
        for (k, v) in mods {
            guard let fp = v["fingerprint"] as? String else { continue }
            if let r = v["row"] as? [String: Any] {
                guard let s = r["says"] as? Int, let qn = r["quits"] as? Int, let y = r["yields"] as? Int, let fc = r["files"] as? Int,
                      let nm = r["names"] as? [String] else { continue }
                out[k] = CacheEntry(fingerprint: fp, row: Row(says: s, quits: qn, yields: y, files: fc, names: Set(nm)))
            } else { out[k] = CacheEntry(fingerprint: fp, row: nil) }
        }
        return out
    }
    static func saveCache(_ cache: [String: CacheEntry], to url: URL) {
        var mods: [String: Any] = [:]
        for (k, e) in cache {
            var m: [String: Any] = ["fingerprint": e.fingerprint]
            if let r = e.row {
                m["row"] = ["says": r.says, "quits": r.quits, "yields": r.yields, "files": r.files, "names": r.names.sorted()] as [String: Any]
            }
            mods[k] = m
        }
        FS.writeJSON(["version": 1, "scanner": scannerVersion, "mods": mods] as [String: Any], to: url)
    }

    /// Pre-launch step, after the Workshop sync: scans the enabled mods of `<support>/mods`, writes `<support>/modscan.lua`.
    static func run(_ dirs: SyncDirs, log: SyncLog) {
        var cache = loadCache(dirs.modScanCache)
        let (mods, st) = scanDirectory(dirs.mods, cache: &cache)
        let text = lua(mods)
        let fm = FileManager.default
        try? fm.createDirectory(at: dirs.modScanFile.deletingLastPathComponent(), withIntermediateDirectories: true)
        if let old = try? String(contentsOf: dirs.modScanFile, encoding: .utf8), old == text {
            // unchanged: leave the file (and its mtime) alone
        } else {
            do { try Data(text.utf8).write(to: dirs.modScanFile, options: .atomic) } catch {
                log.add("mod scan: FAILED to write \(dirs.modScanFile.path): \(error.localizedDescription)")
                return
            }
        }
        saveCache(cache, to: dirs.modScanCache)
        log.add("mod scan: \(st.enabled) enabled mod(s), \(st.naming) name REPENTOGON in their code, \(st.scanned) read, \(st.cached) unchanged (cached), \(st.ms) ms; wrote \(dirs.modScanFile.path)")
    }
}
