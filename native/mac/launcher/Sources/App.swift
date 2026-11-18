import SwiftUI
import AppKit

@MainActor
final class AppModel: ObservableObject {
    @Published var settings = LauncherSettings.load() {
        didSet { if settings != oldValue { settings.save() } }
    }
    @Published var mods: [Mod] = []
    @Published var layer: LayerInfo? = LayerInfo.load()
    @Published var gate: GateLog? = GateLog.load()
    let runner = GameRunner()
    /// A built-in screen has a notch: only then is the NotchFill option offered (kept visible while it is on).
    var hasNotch: Bool { NotchFill.macHasNotch || settings.notchFill }
    /// Decided once at start: skip is on and Option is not held.
    let skipThisStart: Bool
    var didAutoStart = false

    init() {
        let optionHeld = NSEvent.modifierFlags.contains(.option)
        let forced = Debug.value("FORCE") != nil
        skipThisStart = LauncherSettings.load().skipLauncher && !optionHeld && !forced
        reload()
    }

    func reload() {
        mods = ModStore.load()
        layer = LayerInfo.load()
        gate = GateLog.load()
    }

    var epiphanyOnWithVanilla: Bool {
        settings.vanillaLaunch && mods.contains { $0.isEpiphany && $0.enabled }
    }

    func toggle(_ mod: Mod, _ on: Bool) {
        ModStore.setEnabled(mod, on)
        reload()
    }

    func play(afterLaunch: @escaping () -> Void, onReady: @escaping () -> Void,
              onExit: @escaping (Int32, Bool) -> Void) {
        runner.onPrepared = { [weak self] in self?.reload() }
        runner.onReady = onReady
        runner.launch(settings, onExit: onExit)
        if runner.running { afterLaunch() }
    }
}

final class AppDelegate: NSObject, NSApplicationDelegate {
    // The Launcher hides its window while the game runs; never restore that hidden state next start.
    func applicationWillFinishLaunching(_ notification: Notification) {
        UserDefaults.standard.set(false, forKey: "NSQuitAlwaysKeepsWindows")
    }
    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.regular)
        NSApp.activate(ignoringOtherApps: true)
    }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { false }
}

@main
struct LauncherApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var delegate
    @StateObject private var model = AppModel()

    init() {
        // never restore a window state in which the Launcher had hidden itself behind a running game
        UserDefaults.standard.set(false, forKey: "NSQuitAlwaysKeepsWindows")
    }

    var body: some Scene {
        WindowGroup("Repentance+") {
            RootView()
                .environmentObject(model)
                .frame(minWidth: 640, minHeight: 520)
        }
        .windowResizability(.contentMinSize)
        .defaultSize(width: 760, height: 600)
    }
}

/// The game log lives in a plain AppKit window (not a SwiftUI scene) so that macOS never
/// restores "only the log window" at the next start.
@MainActor
enum LogWindow {
    static var window: NSWindow?
    static func show(_ runner: GameRunner) {
        if window == nil {
            let w = NSWindow(contentViewController: NSHostingController(
                rootView: LogView().environmentObject(runner).frame(minWidth: 520, minHeight: 300)))
            w.title = "Game Log"
            w.isRestorable = false
            w.isReleasedWhenClosed = false
            w.setContentSize(NSSize(width: 720, height: 420))
            w.center()
            window = w
        }
        window?.makeKeyAndOrderFront(nil)
    }
}
