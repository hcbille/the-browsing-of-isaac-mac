import Foundation

// Tests for save import and Workshop sync, on fixtures under a scratch folder.
// Run: native/mac/launcher/Tests/run.sh <scratch dir>   (exit status 0 = all passed)

var failures = 0
func check(_ ok: Bool, _ what: String) { print(ok ? "ok   " : "FAIL ", what); if !ok { failures += 1 } }

let fm = FileManager.default
let root = URL(fileURLWithPath: CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : NSTemporaryDirectory())
    .appendingPathComponent("sync-tests", isDirectory: true).standardizedFileURL
try? fm.removeItem(at: root)
try fm.createDirectory(at: root, withIntermediateDirectories: true)

func write(_ u: URL, _ s: String) {
    try? fm.createDirectory(at: u.deletingLastPathComponent(), withIntermediateDirectories: true)
    try? Data(s.utf8).write(to: u)
}
func read(_ u: URL) -> String? { try? String(contentsOf: u, encoding: .utf8) }
func exists(_ u: URL) -> Bool { fm.fileExists(atPath: u.path) }

func mod(_ ws: URL, id: String, dir: String, version: String, body: String = "print('x')") {
    let d = ws.appendingPathComponent(id)
    write(d.appendingPathComponent("metadata.xml"), "<metadata><name>\(dir)</name><directory>\(dir)</directory><id>\(id)</id><version>\(version)</version></metadata>")
    write(d.appendingPathComponent("main.lua"), body)
}

func fresh(_ name: String) -> (SyncDirs, URL) {
    let base = root.appendingPathComponent(name)
    let d = SyncDirs(support: base.appendingPathComponent("support"), mods: base.appendingPathComponent("support/mods"),
                     saves: base.appendingPathComponent("support/saves"),
                     steamUserdata: base.appendingPathComponent("steam/userdata"),
                     steamWorkshop: base.appendingPathComponent("steam/workshop"),
                     workshopFallbacks: [base.appendingPathComponent("fallback")], saveSearch: [base.appendingPathComponent("game")])
    return (d, base)
}
func runAll(_ d: SyncDirs) { PreLaunch.run(d) }
func logText(_ d: SyncDirs) -> String { read(d.log) ?? "" }
func unsub(_ d: SyncDirs) -> [String]? { FS.readJSON(d.modSync)?["unsubscribed"] as? [String] }

// ---- Save import ----
do {
    let (d, base) = fresh("save-present")
    let remote = base.appendingPathComponent("steam/userdata/123/250900/remote")
    write(remote.appendingPathComponent("persistentgamedata1.dat"), "CLOUD-SAVE-ONE")
    write(remote.appendingPathComponent("persistentgamedata2.dat"), "CLOUD-SAVE-TWO")
    write(remote.appendingPathComponent("gamestate1.dat"), "RUN")
    let before = try fm.contentsOfDirectory(atPath: remote.path).sorted()
    runAll(d)
    let t = d.saveTree
    check(read(t.appendingPathComponent("persistentgamedata1.dat")) == "CLOUD-SAVE-ONE", "save present + no Mac save: slot 1 imported")
    check(read(t.appendingPathComponent("persistentgamedata2.dat")) == "CLOUD-SAVE-TWO", "slot 2 imported")
    check(!exists(t.appendingPathComponent("gamestate1.dat")), "a run in progress (gamestate) is not imported")
    check(exists(d.saveMarker), "marker written")
    let m = FS.readJSON(d.saveMarker)
    check((m?["source"] as? String) == remote.path && ((m?["files"] as? [[String: Any]])?.count == 2), "marker records source and two files")
    check(((m?["files"] as? [[String: Any]])?.first?["sha256"] as? String)?.count == 64, "marker has sha256")
    check((try fm.contentsOfDirectory(atPath: remote.path).sorted()) == before
          && read(remote.appendingPathComponent("persistentgamedata1.dat")) == "CLOUD-SAVE-ONE", "source untouched")
    check(logText(d).contains("save import: imported"), "decision logged to launcher.log")
    write(t.appendingPathComponent("persistentgamedata1.dat"), "MAC-PROGRESS")
    write(remote.appendingPathComponent("persistentgamedata1.dat"), "CLOUD-CHANGED")
    runAll(d)
    check(read(t.appendingPathComponent("persistentgamedata1.dat")) == "MAC-PROGRESS", "second launch: no re-import, Mac progress kept")
    try fm.removeItem(at: t.appendingPathComponent("persistentgamedata2.dat"))
    runAll(d)
    check(!exists(t.appendingPathComponent("persistentgamedata2.dat")), "marker prevents a later re-import even after a slot is deleted")
}
do {
    let (d, base) = fresh("save-absent")
    try fm.createDirectory(at: base.appendingPathComponent("steam/userdata/123/250900/remote"), withIntermediateDirectories: true)
    runAll(d)
    check(!exists(d.saveTree) && !exists(d.saveMarker), "save absent: nothing created, no marker")
    check(logText(d).contains("no existing Repentance+ save found"), "absence logged")
    write(base.appendingPathComponent("game/Documents/My Games/x/rep+persistentgamedata3.dat"), "FOUND-BY-SEARCH")
    runAll(d)
    check(read(d.saveTree.appendingPathComponent("persistentgamedata3.dat")) == "FOUND-BY-SEARCH",
          "later-appearing save in a search folder is imported (rep+ name mapped)")
}
do {
    let (d, base) = fresh("save-mac-exists")
    let remote = base.appendingPathComponent("steam/userdata/123/250900/remote")
    write(remote.appendingPathComponent("persistentgamedata1.dat"), "CLOUD")
    let mine = d.saveTree.appendingPathComponent("persistentgamedata1.dat")
    write(mine, "MAC-BYTES-\u{1}\u{2}")
    let bytes = try Data(contentsOf: mine)
    runAll(d)
    check((try Data(contentsOf: mine)) == bytes, "Mac save already present: bytes unchanged")
    check(!exists(d.saveTree.appendingPathComponent("persistentgamedata2.dat")), "no other file added")
    check((FS.readJSON(d.saveMarker)?["status"] as? String) == "skipped-existing-mac-save", "marker records the skip")
    runAll(d)
    check((try Data(contentsOf: mine)) == bytes, "still unchanged on the next launch")
}

// ---- Workshop sync ----
do {
    let (d, base) = fresh("mods")
    let fb = base.appendingPathComponent("fallback")
    let checker = d.mods.appendingPathComponent("repentogon_3127536138")
    mod(fb, id: "111", dir: "alpha", version: "1.0")
    mod(fb, id: "222", dir: "beta", version: "1.0")
    mod(fb, id: WorkshopSync.checkerModID, dir: "repentogon", version: "1.10")
    write(fb.appendingPathComponent("111/resources/a.txt"), "A")
    runAll(d)
    check(read(d.mods.appendingPathComponent("alpha_111/main.lua")) == "print('x')"
          && exists(d.mods.appendingPathComponent("alpha_111/resources/a.txt")), "new mod copied as <directory>_<id>")
    check(exists(d.mods.appendingPathComponent("beta_222")), "second new mod copied")
    check(exists(checker.appendingPathComponent("disable.it")), "checker mod off after first copy")
    check(!exists(d.mods.appendingPathComponent("alpha_111/disable.it")), "ordinary mod on by default")
    check(!exists(fb.appendingPathComponent("111/disable.it")), "Workshop source untouched")
    check(unsub(d) == [], "mod-sync.json written with empty unsubscribed")

    write(d.mods.appendingPathComponent("alpha_111/disable.it"), "")
    try fm.removeItem(at: checker.appendingPathComponent("disable.it"))
    runAll(d)
    check(!exists(checker.appendingPathComponent("disable.it")), "checker mod stays on once the user turned it on (unchanged mod)")
    mod(fb, id: WorkshopSync.checkerModID, dir: "repentogon", version: "1.11")
    runAll(d)
    check(read(checker.appendingPathComponent("metadata.xml"))?.contains("1.11") == true
          && !exists(checker.appendingPathComponent("disable.it")), "checker mod updated, still on")

    mod(fb, id: "111", dir: "alpha", version: "1.1", body: "print('new')")
    write(d.mods.appendingPathComponent("alpha_111/user-note.txt"), "user file")
    runAll(d)
    check(read(d.mods.appendingPathComponent("alpha_111/main.lua")) == "print('new')", "updated mod re-copied")
    check(exists(d.mods.appendingPathComponent("alpha_111/disable.it")), "update keeps disable.it (user had it off)")
    check(exists(d.replacedDir.appendingPathComponent("alpha_111/user-note.txt")), "previous copy kept outside mods/")
    write(fb.appendingPathComponent("222/extra.lua"), "more")
    runAll(d)
    check(exists(d.mods.appendingPathComponent("beta_222/extra.lua"))
          && !exists(d.mods.appendingPathComponent("beta_222/disable.it")), "content change at same version re-copied; stays on")

    try fm.removeItem(at: fb.appendingPathComponent("222"))
    runAll(d)
    check(exists(d.mods.appendingPathComponent("beta_222/main.lua")), "unsubscribed mod folder kept")
    check(unsub(d) == ["beta_222"], "unsubscribed mod listed in mod-sync.json")
    mod(fb, id: "222", dir: "beta", version: "2.0")
    runAll(d)
    check(unsub(d) == [], "resubscribed mod leaves the list")

    try fm.removeItem(at: fb)
    runAll(d)
    check(unsub(d) == [] && exists(d.mods.appendingPathComponent("alpha_111")), "no Workshop source at all: nothing marked, mods untouched")
    write(d.mods.appendingPathComponent("my-own-mod/metadata.xml"), "<metadata><name>mine</name></metadata>")
    mod(base.appendingPathComponent("steam/workshop"), id: "333", dir: "gamma", version: "1")
    runAll(d)
    check(exists(d.mods.appendingPathComponent("gamma_333")) && exists(d.mods.appendingPathComponent("my-own-mod/metadata.xml")),
          "Steam folder used when it has mods; user's own mod untouched")
    check((unsub(d) ?? []).sorted() == ["alpha_111", "beta_222", "repentogon_3127536138"],
          "mods from the old source listed as unsubscribed, own mod never")
}

// ---- Mod scan ----
do {
    let fixtures = URL(fileURLWithPath: #filePath).deletingLastPathComponent().appendingPathComponent("fixtures/modscan/mods")
    func units(_ s: String) -> [UInt16] { Array(s.utf16) }
    let strip = { (s: String) in String(decoding: ModScan.stripLua(units(s)), as: UTF16.self) }
    check(strip("a --c\nb") == "a    \nb", "stripLua: line comment blanked, newline kept")
    check(strip(#"x = 'it\'s' y"#) == "x = \"     \" y", "stripLua: string with an escaped quote")
    check(strip("--[==[ ]] ]==]z") == String(repeating: " ", count: 14) + "z", "stripLua: level-2 block comment holds a ]]")
    check(strip("[=[ ]] ]=]z") == "\"" + String(repeating: " ", count: 8) + "\"z", "stripLua: long string")
    check(ModScan.scanLua(units("-- REPENTOGON\nlocal s = \"REPENTOGON\" local t = [[REPENTOGON]]")).says == 0, "scanLua: REPENTOGON in comments and strings is not named")
    let q = ModScan.scanLua(units("if not REPENTOGON then\n return\nend\nIsaac.Foo()\nif REPENTOGON then return end"))
    check(q.says == 2 && q.quits == 1 && q.yields == 1 && q.names == ["Isaac", "Foo"], "scanLua: quit guard, yield guard, names")

    // a mods folder of edge cases: what the scan makes of it, against the values the JS gives (see run.sh for the byte comparison)
    var cache: [String: ModScan.CacheEntry] = [:]
    let (rows, st) = ModScan.scanDirectory(fixtures, cache: &cache)
    check(st.enabled == 15 && rows.count == 12 && st.scanned == 15, "fixtures: 15 enabled mods read, 12 name REPENTOGON")
    check(rows["comments-only"] == nil && rows["no-repentogon"] == nil && rows["disabled-mod"] == nil && rows["repentogon"] == nil,
          "fixtures: comment-only, plain, disabled and the Layer's own folder are not listed")
    check(rows["quits-top-level"]?.quits == 2 && rows["guards"]?.quits == 4 && rows["bom-crlf"]?.quits == 1, "fixtures: quits counted as the JS counts them")
    check(rows["quits-in-subfile"]?.quits == 0, "fixtures: a quit guard outside the top-level main.lua is not a quit")
    check(rows["comments-then-code"]?.yields == 4, "fixtures: yields")
    check(rows["defined"]?.names.contains("GetPlayerIndex") == false && rows["defined"]?.names.contains("Thing") == true, "fixtures: names the mod defines itself are dropped")
    let text = ModScan.lua(rows)
    check(text.contains("[\"weird name _1_! +x__\"]"), "fixtures: ids lower-cased and sanitised for the file")
    var again = cache
    let (_, st2) = ModScan.scanDirectory(fixtures, cache: &again)
    check(st2.scanned == 0 && st2.cached == 15, "fixtures: second pass reads nothing")

    // the pre-launch step: file location, cache, enable/disable
    let (d, base) = fresh("modscan")
    mod(base.appendingPathComponent("steam/workshop"), id: "444", dir: "delta", version: "1", body: "if not REPENTOGON then return end\nIsaac.Delta()\n")
    mod(base.appendingPathComponent("steam/workshop"), id: "555", dir: "eps", version: "1", body: "print(1)")
    write(d.mods.appendingPathComponent("repentogon/main.lua"), "REPENTOGON.X()")
    runAll(d)
    let scan1 = read(d.modScanFile) ?? ""
    check(d.modScanFile.path == d.support.appendingPathComponent("modscan.lua").path, "scan file is <support>/modscan.lua")
    check(scan1.contains("[\"delta_444\"] = { says = 1, quits = 1, yields = 0, files = 1, names = \"Delta Isaac\" },") && !scan1.contains("eps_555")
          && !scan1.contains("\"repentogon\""), "pre-launch: modscan.lua lists the mod that names REPENTOGON, not the others or the Layer folder")
    check(logText(d).contains("mod scan: 3 enabled mod(s)") || logText(d).contains("mod scan: 2 enabled mod(s)"), "pre-launch: scan logged")
    let m1 = (try? fm.attributesOfItem(atPath: d.modScanFile.path))?[.modificationDate] as? Date
    runAll(d)
    check(logText(d).contains("0 read, 2 unchanged (cached)"), "second launch: every mod from the cache")
    check((try? fm.attributesOfItem(atPath: d.modScanFile.path))?[.modificationDate] as? Date == m1, "unchanged scan leaves modscan.lua alone")
    write(d.mods.appendingPathComponent("delta_444/disable.it"), "")
    runAll(d)
    check(!(read(d.modScanFile) ?? "").contains("delta_444"), "a mod switched off leaves the scan")
    try fm.removeItem(at: d.mods.appendingPathComponent("delta_444/disable.it"))
    runAll(d)
    check((read(d.modScanFile) ?? "") == scan1 && logText(d).contains("0 read, 2 unchanged (cached)"), "switched on again: same file, from the cache (disable.it is not in the fingerprint)")
    write(d.mods.appendingPathComponent("delta_444/extra.lua"), "REPENTOGON.More()")
    runAll(d)
    check((read(d.modScanFile) ?? "").contains("files = 2") && logText(d).contains("1 read, 1 unchanged (cached)"), "a changed mod is read again, the rest stays cached")
    try fm.removeItem(at: d.modScanCache)
    runAll(d)
    check(logText(d).contains("2 read, 0 unchanged (cached)"), "no cache file: everything read")
}

// ---- Load order and window setting ----
do {
    let dir = root.appendingPathComponent("loadorder/mods")
    for n in ["Zeta_1", "alpha_2", "Beta_3", ".hidden", "repentogon", "! bang_4"] { try fm.createDirectory(at: dir.appendingPathComponent(n), withIntermediateDirectories: true) }
    write(dir.appendingPathComponent("a-file.txt"), "x")
    let lo = ModStore.gameLoadOrder(in: dir)
    check(Set(lo) == ["Zeta_1", "alpha_2", "Beta_3", "! bang_4"], "load order: dot-folders, files and the Layer folder are not mods")
    // the Host's readdir order, as the game logs it with LOADED MOD
    var raw: [String] = []
    if let d = opendir(dir.path) { while let e = readdir(d) {
        let n = withUnsafePointer(to: &e.pointee.d_name) { $0.withMemoryRebound(to: CChar.self, capacity: 1024) { String(cString: $0) } }
        if Set(["Zeta_1", "alpha_2", "Beta_3", "! bang_4"]).contains(n) { raw.append(n) } }; closedir(d) }
    check(lo == raw, "load order equals the raw directory order (not sorted by name)")
    if let log = ProcessInfo.processInfo.environment["REPENTANCE_MAC_LOADED_MOD_LOG"],
       let mods = ProcessInfo.processInfo.environment["REPENTANCE_MAC_LOADED_MOD_DIR"],
       let text = try? String(contentsOfFile: log, encoding: .utf8) {
        let logged = text.split(separator: "\n").compactMap { l -> String? in
            guard let r = l.range(of: "LOADED MOD //mods/") else { return nil }
            return String(l[r.upperBound...].split(separator: "/")[0]) }
        let mine = ModStore.gameLoadOrder(in: URL(fileURLWithPath: mods)).map { $0.lowercased() }
        check(!logged.isEmpty && logged == mine, "real run: LOADED MOD order of \(logged.count) mods equals the Launcher's load order")
    } else { print("SKIP  real LOADED MOD comparison (REPENTANCE_MAC_LOADED_MOD_LOG / _DIR not set)") }
    let s0 = try JSONDecoder().decode(LauncherSettings.self, from: Data("{}".utf8))
    check(s0.scaleMode == "integer", "scale_mode defaults to integer")
    var s1 = LauncherSettings(); s1.scaleMode = "fill"
    let enc = try JSONEncoder().encode(s1)
    check((try? JSONDecoder().decode(LauncherSettings.self, from: enc))?.scaleMode == "fill"
          && String(decoding: enc, as: UTF8.self).contains("\"scale_mode\""), "scale_mode fill round-trips under the key scale_mode")
    check((try JSONDecoder().decode(LauncherSettings.self, from: Data("{\"scale_mode\":\"weird\"}".utf8))).scaleMode == "integer", "unknown scale_mode falls back to integer")
}

// ---- sync-config merge: env > support > bundled > defaults ----
do {
    func cfg(_ w: [String], _ v: [String]) -> SyncConfig { var c = SyncConfig(); c.workshopFallback = w; c.saveSearch = v; return c }
    let m = SyncConfig.merged([cfg(["/s/w"], ["/s/g"]), cfg(["/b/w", "/s/w"], ["/b/g"])])
    check(m.workshopFallback == ["/s/w", "/b/w"] && m.saveSearch == ["/s/g", "/b/g"], "sync-config merge: support first, then bundled, duplicates dropped")
    check(SyncConfig.merged([SyncConfig(), cfg(["/b/w"], [])]).workshopFallback == ["/b/w"], "sync-config merge: bundled alone is used")
    check(SyncConfig.merged([]).workshopFallback.isEmpty, "sync-config merge: nothing configured = built-in defaults only")
    let base = root.appendingPathComponent("cfgmerge")
    let sup = base.appendingPathComponent("support"), bundled = base.appendingPathComponent("bundled.json")
    write(sup.appendingPathComponent("sync-config.json"), "{\"workshop_fallback\":[\"/s/w\"]}")
    write(bundled, "{\"workshop_fallback\":[\"/b/w\"],\"save_search\":[\"/b/g\"]}")
    let a = SyncConfig.merged([SyncConfig.load(sup.appendingPathComponent("sync-config.json")), SyncConfig.load(bundled)])
    check(a.workshopFallback == ["/s/w", "/b/w"] && a.saveSearch == ["/b/g"], "sync-config merge from files: bundled fills what support lacks")
    check(SyncConfig.load(base.appendingPathComponent("missing.json")).workshopFallback.isEmpty, "missing config file = empty")
}

print(failures == 0 ? "ALL PASSED" : "\(failures) FAILED")
exit(failures == 0 ? 0 : 1)
