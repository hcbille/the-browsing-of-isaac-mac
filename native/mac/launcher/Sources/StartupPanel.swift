import SwiftUI
import AppKit

/// Small floating window shown from Play until the game presents its first frame. A plain AppKit
/// panel (like LogWindow) so macOS never restores it.
@MainActor
enum StartupPanel {
    static var window: NSPanel?
    static func show(_ runner: GameRunner) {
        if window == nil {
            let p = NSPanel(contentViewController: NSHostingController(
                rootView: StartupView().environmentObject(runner)))
            p.title = "Repentance+"
            p.styleMask = [.titled, .closable]
            p.level = .floating
            p.isRestorable = false
            p.isReleasedWhenClosed = false
            p.hidesOnDeactivate = false
            p.setContentSize(NSSize(width: 440, height: 250))
            p.center()
            window = p
        }
        window?.makeKeyAndOrderFront(nil)
    }
    static func close() {
        window?.orderOut(nil)
        window = nil
    }
}

struct StartupView: View {
    @EnvironmentObject var runner: GameRunner

    var body: some View {
        TimelineView(.periodic(from: .now, by: 1)) { ctx in
            let secs = max(0, Int(ctx.date.timeIntervalSince(runner.bootStarted ?? ctx.date)))
            VStack(spacing: 12) {
                Text("The Binding of Isaac: Repentance+").font(.caption).foregroundStyle(.secondary)
                ProgressView().controlSize(.regular)
                Text(runner.bootStatus ?? "Starting the game")
                    .font(.title3.weight(.semibold)).multilineTextAlignment(.center)
                    .lineLimit(3).frame(maxWidth: .infinity, minHeight: 56)
                Text(String(format: "%d:%02d", secs / 60, secs % 60))
                    .font(.system(.body, design: .monospaced)).foregroundStyle(.secondary)
                Text("Large mod packs can take a few minutes the first time")
                    .font(.caption).foregroundStyle(.secondary)
                    .opacity(secs >= 20 ? 1 : 0)
                Button("Show log") { LogWindow.show(runner) }.controlSize(.small)
            }
            .padding(20)
            .frame(width: 440)
        }
    }
}
