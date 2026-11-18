import Foundation
import AppKit

/// Starts the game executable and tails its output.
@MainActor
final class GameRunner: ObservableObject {
    @Published var running = false
    @Published var logText = ""
    @Published var error: String?
    /// Startup feedback, from the game's `[mac][boot]` stdout lines (docs/mac/launcher.md).
    @Published var bootStatus: String?
    @Published var bootStarted: Date?
    @Published var ready = false
    private var sawBootLine = false
    private var lineBuffer = ""
    private var fallbackTask: Task<Void, Never>?
    /// Seconds without any `[mac][boot]` line after which an older game binary is treated as started.
    static let noBootLineFallback: UInt64 = 240
    /// Called once when the first frame is presented (or the fallback fires).
    var onReady: (() -> Void)?
    private var process: Process?
    private var logHandle: FileHandle?

    /// `onExit(status, wasReady)`: wasReady is false when the game ended during startup.
    func launch(_ s: LauncherSettings, onExit: @escaping (Int32, Bool) -> Void) {
        guard !running else { return }
        error = nil
        ready = false; sawBootLine = false; lineBuffer = ""; fallbackTask?.cancel()
        bootStarted = Date()
        bootStatus = "Preparing (save import, Workshop sync)"
        let fm = FileManager.default
        let exe = Paths.gameExecutable
        guard fm.isExecutableFile(atPath: exe.path) else {
            error = "The game program was not found at \(exe.path)."
            return
        }
        try? fm.createDirectory(at: Paths.supportDir, withIntermediateDirectories: true)
        try? fm.createDirectory(at: Paths.gameLog.deletingLastPathComponent(), withIntermediateDirectories: true)
        // Save import and Workshop sync run before every game start (docs/mac/launcher.md), off the main thread.
        running = true
        logText = "[launcher] preparing: save import, Workshop sync\n"
        Task { @MainActor in
            let lines = await Task.detached(priority: .userInitiated) { PreLaunch.run() }.value
            self.logText += lines.map { "[launcher] \($0)\n" }.joined()
            self.onPrepared?()
            self.start(s, exe: exe, onExit: onExit)
        }
    }

    var onPrepared: (() -> Void)?

    private func start(_ s: LauncherSettings, exe: URL, onExit: @escaping (Int32, Bool) -> Void) {
        let fm = FileManager.default

        let iso = ISO8601DateFormatter().string(from: Date())
        let json: [String: Any] = [
            "version": 1, "launched_at": iso,
            "vanilla_launch": s.vanillaLaunch, "debug_console": s.debugConsole,
            "log_window": s.logWindow, "lua_debug": s.luaDebug,
            "scale_mode": s.scaleMode, "notch_fill": s.notchFill,
            "support_dir": Paths.supportDir.path, "mods_dir": Paths.modsDir.path,
            "save_dir": SyncDirs.fromEnvironment().saves.path,
            "gate_log_path": Paths.gateLog.path, "game_log_path": Paths.gameLog.path,
            "modscan_path": SyncDirs.fromEnvironment().modScanFile.path,
        ]
        if let d = try? JSONSerialization.data(withJSONObject: json, options: [.prettyPrinted, .sortedKeys]) {
            try? d.write(to: Paths.launchSettings, options: .atomic)
        }

        var args = ["--launch-settings", Paths.launchSettings.path]
        if s.vanillaLaunch { args.append("--vanilla") }
        if s.debugConsole { args.append("--debug-console") }
        if s.luaDebug { args.append("--lua-debug") }

        fm.createFile(atPath: Paths.gameLog.path, contents: nil)
        logHandle = try? FileHandle(forWritingTo: Paths.gameLog)
        let lh = logHandle

        let p = Process()
        p.executableURL = exe
        p.arguments = args
        let lib = s.notchFill ? NotchFill.libraryURL(gameExecutable: exe) : nil
        let launcherLog = SyncLog(SyncDirs.fromEnvironment().log)
        var env = NotchFill.environment(base: ProcessInfo.processInfo.environment, enabled: s.notchFill, library: lib) { msg in
            launcherLog.add(msg)
            self.logText += "[launcher] \(msg)\n"
        }
        if let l = lib {
            launcherLog.add("NotchFill on: DYLD_INSERT_LIBRARIES=\(l.path)")
            logText += "[launcher] NotchFill on: DYLD_INSERT_LIBRARIES=\(l.path)\n"
        }
        env["REPENTANCE_MAC_LAUNCH_SETTINGS"] = Paths.launchSettings.path
        p.environment = env
        let pipe = Pipe()
        p.standardOutput = pipe
        p.standardError = pipe
        pipe.fileHandleForReading.readabilityHandler = { [weak self] h in
            let d = h.availableData
            guard !d.isEmpty else { return }
            try? lh?.write(contentsOf: d)
            let t = String(decoding: d, as: UTF8.self)
            DispatchQueue.main.async {
                guard let self else { return }
                self.logText += t
                self.consumeBoot(t)
                if self.logText.count > 400_000 { self.logText = String(self.logText.suffix(300_000)) }
            }
        }
        p.terminationHandler = { [weak self] proc in
            pipe.fileHandleForReading.readabilityHandler = nil
            DispatchQueue.main.async {
                guard let self else { return }
                self.running = false
                self.fallbackTask?.cancel()
                self.flushBootBuffer()
                self.logText += "\n[launcher] game exited with status \(proc.terminationStatus)\n"
                try? self.logHandle?.close()
                onExit(proc.terminationStatus, self.ready)
            }
        }
        do {
            try p.run()
            process = p
            running = true
            if !sawBootLine { bootStatus = "Starting the game" }
            fallbackTask = Task { @MainActor [weak self] in
                try? await Task.sleep(nanoseconds: Self.noBootLineFallback * 1_000_000_000)
                guard let self, !Task.isCancelled, self.running, !self.sawBootLine else { return }
                self.markReady()
            }
        } catch {
            self.error = "Could not start the game: \(error.localizedDescription)"
            running = false
            bootStatus = nil
            for w in NSApp.windows where w.title == "Repentance+" { w.makeKeyAndOrderFront(nil) }
        }
    }

    // MARK: boot protocol

    /// Buffers partial chunks and handles whole lines.
    private func consumeBoot(_ chunk: String) {
        lineBuffer += chunk
        while let nl = lineBuffer.firstIndex(of: "\n") {
            let line = String(lineBuffer[..<nl])
            lineBuffer.removeSubrange(...nl)
            handleBootLine(line)
        }
        if lineBuffer.count > 20_000 { lineBuffer = "" }
    }

    private func flushBootBuffer() {
        if !lineBuffer.isEmpty { handleBootLine(lineBuffer); lineBuffer = "" }
    }

    private func handleBootLine(_ line: String) {
        if let r = line.range(of: "[mac][boot] status:") {
            sawBootLine = true
            let t = line[r.upperBound...].trimmingCharacters(in: .whitespaces)
            if !ready, !t.isEmpty { bootStatus = t }
        } else if line.contains("[mac][boot] ready") {
            sawBootLine = true
            markReady()
        }
    }

    private func markReady() {
        guard !ready else { return }
        ready = true
        fallbackTask?.cancel()
        onReady?()
    }
}
