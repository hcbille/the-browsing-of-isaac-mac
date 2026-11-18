import Foundation
import AppKit

/// Starts the game executable and tails its output.
@MainActor
final class GameRunner: ObservableObject {
    @Published var running = false
    @Published var logText = ""
    @Published var error: String?
    private var process: Process?
    private var logHandle: FileHandle?

    func launch(_ s: LauncherSettings, onExit: @escaping () -> Void) {
        guard !running else { return }
        error = nil
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

    private func start(_ s: LauncherSettings, exe: URL, onExit: @escaping () -> Void) {
        let fm = FileManager.default

        let iso = ISO8601DateFormatter().string(from: Date())
        let json: [String: Any] = [
            "version": 1, "launched_at": iso,
            "vanilla_launch": s.vanillaLaunch, "debug_console": s.debugConsole,
            "log_window": s.logWindow, "lua_debug": s.luaDebug,
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
        var env = ProcessInfo.processInfo.environment
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
                if self.logText.count > 400_000 { self.logText = String(self.logText.suffix(300_000)) }
            }
        }
        p.terminationHandler = { [weak self] proc in
            pipe.fileHandleForReading.readabilityHandler = nil
            DispatchQueue.main.async {
                guard let self else { return }
                self.running = false
                self.logText += "\n[launcher] game exited with status \(proc.terminationStatus)\n"
                try? self.logHandle?.close()
                onExit()
            }
        }
        do {
            try p.run()
            process = p
            running = true
        } catch {
            self.error = "Could not start the game: \(error.localizedDescription)"
            running = false
            for w in NSApp.windows where w.title == "Repentance+" { w.makeKeyAndOrderFront(nil) }
        }
    }
}
