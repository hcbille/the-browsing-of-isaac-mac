import Foundation
import CryptoKit

// Golden-test CLI for the mod scan (run by Tests/run.sh; never shipped).
//   golden lua   <mods dir> <out file>   the modscan.lua the Launcher would write for that folder of mods
//   golden files <mods dir> <out file>   one line per .lua file: stripLua hash, scanLua facts (compared to golden_scan.mjs)
// Both print timing to stderr.

func sha(_ s: String) -> String { SHA256.hash(data: Data(s.utf8)).map { String(format: "%02x", $0) }.joined() }

let a = CommandLine.arguments
guard a.count == 4 else { FileHandle.standardError.write(Data("usage: golden lua|files <mods dir> <out>\n".utf8)); exit(2) }
let dir = URL(fileURLWithPath: a[2]), out = URL(fileURLWithPath: a[3])
let t0 = Date()
switch a[1] {
case "lua":
    var cache: [String: ModScan.CacheEntry] = [:]
    let (mods, st) = ModScan.scanDirectory(dir, cache: &cache)
    try ModScan.lua(mods).write(to: out, atomically: true, encoding: .utf8)
    // the same again with the cache the first pass made: nothing is read
    var again = cache
    let (_, st2) = ModScan.scanDirectory(dir, cache: &again)
    FileHandle.standardError.write(Data("swift lua: \(st.enabled) enabled, \(st.naming) name REPENTOGON, scan \(st.ms) ms; second pass with cache: \(st2.scanned) read, \(st2.cached) cached, \(st2.ms) ms\n".utf8))
case "files":
    var lines: [String] = []
    let fm = FileManager.default
    for name in ((try? fm.contentsOfDirectory(atPath: dir.path)) ?? []).sorted() where !name.hasPrefix(".") && name.lowercased() != "repentogon" {
        let md = dir.appendingPathComponent(name)
        guard FS.isDir(md), !FS.exists(md.appendingPathComponent("disable.it")) else { continue }
        for f in ModScan.files(in: md) where f.rel.lowercased().hasSuffix(".lua") {
            let units = ModScan.decode((try? Data(contentsOf: f.url)) ?? Data())
            let stripped = String(decoding: ModScan.stripLua(units), as: UTF16.self)
            let r = ModScan.scanLua(units)
            let names = r.names.sorted { Array($0.utf8).lexicographicallyPrecedes(Array($1.utf8)) }.joined(separator: "\n")
            let defd = r.defined.sorted { Array($0.utf8).lexicographicallyPrecedes(Array($1.utf8)) }.joined(separator: "\n")
            lines.append("\(name)/\(f.rel)\t\(sha(stripped))\t\(r.says) \(r.quits) \(r.yields)\t\(sha(names))\t\(sha(defd))")
        }
    }
    try (lines.sorted { Array($0.utf8).lexicographicallyPrecedes(Array($1.utf8)) }.joined(separator: "\n") + "\n").write(to: out, atomically: true, encoding: .utf8)
    FileHandle.standardError.write(Data("swift files: \(lines.count) .lua file(s), \(Int(Date().timeIntervalSince(t0) * 1000)) ms\n".utf8))
default: exit(2)
}
