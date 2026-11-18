import SwiftUI
import AppKit

struct RootView: View {
    @EnvironmentObject var model: AppModel
    @State private var tab = Tab(rawValue: Debug.value("TAB")?.capitalized ?? "") ?? Tab.play
    enum Tab: String, CaseIterable { case play = "Play", mods = "Mods", options = "Options" }

    var body: some View {
        VStack(spacing: 0) {
            Picker("", selection: $tab) {
                ForEach(Tab.allCases, id: \.self) { Text($0.rawValue).tag($0) }
            }
            .pickerStyle(.segmented).labelsHidden()
            .padding(12)
            Divider()
            Group {
                switch tab {
                case .play: PlayView(play: play)
                case .mods: ModsView()
                case .options: OptionsView()
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
        .onAppear {
            Debug.snapshotIfAsked()
            guard model.skipThisStart, !model.didAutoStart else { return }
            model.didAutoStart = true
            play()
        }
    }

    func play() {
        model.reload()
        model.play(afterLaunch: {
            if model.settings.logWindow { LogWindow.show(model.runner) }
            // The startup panel stands in for the Launcher until the game's first frame. The Launcher
            // stays alive while the game runs (Steam tracks this process) but gets out of the way.
            StartupPanel.show(model.runner)
            for w in NSApp.windows where w.title == "Repentance+" { w.orderOut(nil) }
        }, onReady: {
            StartupPanel.close()
            for w in NSApp.windows where w.title == "Repentance+" { w.orderOut(nil) }
        }, onExit: { status, wasReady in
            StartupPanel.close()
            if wasReady {
                NSApp.terminate(nil)
            } else {
                model.runner.error = "The game exited during startup with status \(status)."
                NSApp.activate(ignoringOtherApps: true)
                for w in NSApp.windows where w.title == "Repentance+" { w.makeKeyAndOrderFront(nil) }
            }
        })
        if model.runner.error != nil {
            for w in NSApp.windows where w.title == "Repentance+" { w.makeKeyAndOrderFront(nil) }
        }
    }
}

struct PlayView: View {
    @EnvironmentObject var model: AppModel
    let play: () -> Void

    var body: some View {
        VStack(spacing: 18) {
            Spacer()
            Text("The Binding of Isaac: Repentance+").font(.title2.weight(.semibold))
            Text("Mac Build").foregroundStyle(.secondary)
            summary
            if model.epiphanyOnWithVanilla { VanillaEpiphanyWarning() }
            if let e = model.runner.error {
                Label(e, systemImage: "xmark.octagon.fill").foregroundStyle(.red)
                    .font(.callout).multilineTextAlignment(.center)
                Button("Show log") { LogWindow.show(model.runner) }.controlSize(.small)
            }
            Button(action: play) {
                Text(model.runner.running ? "Running…" : "Play").font(.title3.weight(.semibold))
                    .frame(width: 180, height: 30)
            }
            .buttonStyle(.borderedProminent).controlSize(.large)
            .keyboardShortcut(.defaultAction)
            .disabled(model.runner.running)
            Toggle("Skip the Launcher next time", isOn: $model.settings.skipLauncher)
            Text("Hold Option while starting to bring the Launcher back.")
                .font(.caption).foregroundStyle(.secondary)
            Spacer()
        }
        .padding(24)
    }

    var summary: some View {
        let on = model.mods.filter { $0.enabled }.count
        return VStack(spacing: 4) {
            Text(model.settings.vanillaLaunch ? "Vanilla Launch: the Layer is off" : "Layer on")
            Text("\(on) of \(model.mods.count) mods on")
        }
        .font(.callout).foregroundStyle(.secondary)
    }
}

struct VanillaEpiphanyWarning: View {
    var body: some View {
        Label("Epiphany needs REPENTOGON and will not run on a Vanilla Launch. Turn Vanilla Launch off in Options, or turn Epiphany off.",
              systemImage: "exclamationmark.triangle.fill")
            .foregroundStyle(.orange).font(.callout)
            .padding(10)
            .background(Color.orange.opacity(0.12), in: RoundedRectangle(cornerRadius: 8))
            .frame(maxWidth: 480)
    }
}

struct ModsView: View {
    @EnvironmentObject var model: AppModel
    @State private var pendingRemove: Mod?
    @State private var byLoadOrder = Debug.value("SORT") == "load"

    var body: some View {
        VStack(spacing: 0) {
            HStack {
                Text(Paths.modsDir.path).font(.caption).foregroundStyle(.secondary).lineLimit(1).truncationMode(.middle)
                Spacer()
                Picker("", selection: $byLoadOrder) {
                    Text("By name").tag(false); Text("By load order").tag(true)
                }.pickerStyle(.segmented).labelsHidden().frame(width: 200)
                Button("Reload") { model.reload() }
            }
            .padding(.horizontal, 12).padding(.vertical, 8)
            if model.epiphanyOnWithVanilla { VanillaEpiphanyWarning().padding(.bottom, 8) }
            Divider()
            if model.mods.isEmpty {
                Text("No mods in this folder yet.\nMods are copied here from Steam's Workshop folder when the game starts.")
                    .multilineTextAlignment(.center).foregroundStyle(.secondary).frame(maxHeight: .infinity)
            } else {
                List(byLoadOrder ? model.mods.sorted { $0.order < $1.order } : model.mods) { mod in
                    ModRow(mod: mod, remove: { pendingRemove = mod })
                }
            }
            Divider()
            Text("The list is sorted by name, as in the game's MODS menu. The number is the mod's place in the game's load order, which is the order the volume lists the folders in (not by name) and cannot be changed here.")
                .font(.caption).foregroundStyle(.secondary).padding(8)
        }
        .confirmationDialog(
            "Remove \(pendingRemove?.name ?? "this mod")?",
            isPresented: Binding(get: { pendingRemove != nil }, set: { if !$0 { pendingRemove = nil } }),
            titleVisibility: .visible, presenting: pendingRemove
        ) { mod in
            Button("Move to Trash", role: .destructive) {
                try? ModStore.moveToTrash(mod)
                model.reload()
            }
            Button("Cancel", role: .cancel) {}
        } message: { _ in
            Text("The mod's folder is moved to the Trash. You can put it back from there.")
        }
    }
}

struct ModRow: View {
    @EnvironmentObject var model: AppModel
    let mod: Mod
    let remove: () -> Void

    var body: some View {
        HStack(alignment: .top, spacing: 10) {
            Text(mod.order > 0 ? "\(mod.order)" : "–").font(.caption.monospacedDigit()).foregroundStyle(.secondary)
                .frame(width: 28, alignment: .trailing).padding(.top, 3)
            Toggle("", isOn: Binding(get: { mod.enabled }, set: { model.toggle(mod, $0) }))
                .labelsHidden().toggleStyle(.switch).controlSize(.small)
            VStack(alignment: .leading, spacing: 3) {
                HStack(spacing: 6) {
                    Text(mod.name).fontWeight(.medium).foregroundStyle(mod.enabled ? .primary : .secondary)
                    if !mod.version.isEmpty { Text("v\(mod.version)").font(.caption).foregroundStyle(.secondary) }
                    if mod.unsubscribed { Badge(text: "No longer subscribed", color: .gray) }
                }
                if mod.name != mod.id { Text(mod.id).font(.caption2).foregroundStyle(.tertiary) }
                gateInfo
            }
            Spacer()
            if mod.unsubscribed {
                Button("Remove…", role: .destructive, action: remove).controlSize(.small)
            }
        }
        .padding(.vertical, 3)
    }

    @ViewBuilder var gateInfo: some View {
        if let g = mod.gate {
            if g.shown {
                Badge(text: "Shown REPENTOGON", color: .green)
                if let m = g.missing, !m.isEmpty {
                    Text("Missing in the Layer (stand-ins in use): " + m.prefix(8).joined(separator: ", ") + (m.count > 8 ? " and \(m.count - 8) more" : ""))
                        .font(.caption).foregroundStyle(.secondary)
                }
            } else {
                Badge(text: "Hidden from REPENTOGON", color: .orange)
                if let m = g.missing, !m.isEmpty {
                    Text("Missing: " + m.prefix(10).joined(separator: ", ") + (m.count > 10 ? " and \(m.count - 10) more" : ""))
                        .font(.caption).foregroundStyle(.secondary)
                } else if let w = g.why {
                    Text(w).font(.caption).foregroundStyle(.secondary)
                }
            }
        } else if model.gate == nil {
            Text("Not scanned yet (the game scans mods at launch)").font(.caption2).foregroundStyle(.tertiary)
        } else {
            Text("Does not use REPENTOGON").font(.caption2).foregroundStyle(.tertiary)
        }
    }
}

struct Badge: View {
    let text: String; let color: Color
    var body: some View {
        Text(text).font(.caption2.weight(.semibold)).foregroundStyle(color)
            .padding(.horizontal, 6).padding(.vertical, 1)
            .background(color.opacity(0.15), in: Capsule())
    }
}

struct OptionsView: View {
    @EnvironmentObject var model: AppModel

    var body: some View {
        Form {
            Section("Layer") {
                if let l = model.layer {
                    LabeledContent("Layer version", value: l.layerVersion ?? "unknown")
                    LabeledContent("Game version", value: l.gameVersion ?? "unknown")
                    if let c = l.coverageText { LabeledContent("REPENTOGON coverage", value: c) }
                    if let u = l.unsupportedNames, u > 0 { LabeledContent("Accepted, not supported", value: "\(u) names") }
                } else {
                    Text("Layer information is not available (no layer-info.json in the app).").foregroundStyle(.secondary)
                }
                Text("The Layer is updated only by installing a new build; there is no auto-update.")
                    .font(.caption).foregroundStyle(.secondary)
            }
            Section("Window") {
                Picker("Scaling", selection: $model.settings.scaleMode) {
                    Text("Whole-number scaling (black bars)").tag("integer")
                    Text("Fill the window").tag("fill")
                }
                .pickerStyle(.radioGroup)
                Text(model.settings.scaleMode == "fill"
                     ? "The picture is stretched to fill the window; pixels may be uneven."
                     : "The picture is scaled by whole numbers and centred, with black bars around it.")
                    .font(.caption).foregroundStyle(.secondary)
            }
            if model.hasNotch {
                Section("Full screen") {
                    Toggle("Fill the notch area in full screen (NotchFill)", isOn: $model.settings.notchFill)
                    Text("Uses the strip beside the notch in full screen. Loads the NotchFill library into the game.")
                        .font(.caption).foregroundStyle(.secondary)
                }
            }
            Section("Launch") {
                Toggle("Vanilla Launch (Layer off)", isOn: $model.settings.vanillaLaunch)
                Text("No mod sees REPENTOGON. Mods that need it will fall back to their own code or refuse to run.")
                    .font(.caption).foregroundStyle(.secondary)
                if model.epiphanyOnWithVanilla { VanillaEpiphanyWarning() }
                Toggle("Debug console", isOn: $model.settings.debugConsole)
                Toggle("Log window", isOn: $model.settings.logWindow)
                Toggle("Lua debug", isOn: $model.settings.luaDebug)
                if model.settings.luaDebug {
                    Label("Lua debug gives mods access to files and programs on your Mac. Leave it off unless you are developing a mod you trust.",
                          systemImage: "exclamationmark.triangle.fill")
                        .foregroundStyle(.orange).font(.callout)
                } else {
                    Text("Off by default: Lua debug gives mods access to files and programs.")
                        .font(.caption).foregroundStyle(.secondary)
                }
            }
        }
        .formStyle(.grouped)
    }
}

struct LogView: View {
    @EnvironmentObject var runner: GameRunner
    var body: some View {
        ScrollViewReader { proxy in
            ScrollView {
                Text(runner.logText.isEmpty ? "Waiting for the game's output…" : runner.logText)
                    .font(.system(.caption, design: .monospaced))
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .textSelection(.enabled).padding(8)
                Color.clear.frame(height: 1).id("end")
            }
            .onChange(of: runner.logText) { _ in proxy.scrollTo("end") }
        }
    }
}

/// Test helpers, driven by environment variables (command-line arguments confuse AppKit's own parsing):
///   REPENTANCE_MAC_LAUNCHER_TAB=play|mods|options   initial tab
///   REPENTANCE_MAC_LAUNCHER_SORT=load               Mods tab starts sorted by load order
///   REPENTANCE_MAC_LAUNCHER_SNAPSHOT=<png path>     render every visible window of this app from inside it
///   REPENTANCE_MAC_LAUNCHER_SNAPSHOT_DELAY=<secs>, REPENTANCE_MAC_LAUNCHER_EXIT=1
enum Debug {
    static func value(_ name: String) -> String? {
        let v = ProcessInfo.processInfo.environment["REPENTANCE_MAC_LAUNCHER_" + name]
        return (v?.isEmpty ?? true) ? nil : v
    }
    static func snapshotIfAsked() {
        guard let path = value("SNAPSHOT") else { return }
        let delay = Double(value("SNAPSHOT_DELAY") ?? "1.5") ?? 1.5
        DispatchQueue.main.asyncAfter(deadline: .now() + delay) {
            for w in NSApp.windows where w.isVisible || w.title == "Repentance+" {
                guard let v = w.contentView?.superview ?? w.contentView,
                      let rep = v.bitmapImageRepForCachingDisplay(in: v.bounds) else { continue }
                v.cacheDisplay(in: v.bounds, to: rep)
                let suffix = w.title == "Repentance+" ? "" : "-" + w.title.replacingOccurrences(of: " ", with: "")
                let out = path.replacingOccurrences(of: ".png", with: suffix + ".png")
                if let d = rep.representation(using: .png, properties: [:]) { try? d.write(to: URL(fileURLWithPath: out)) }
            }
            if value("EXIT") != nil { NSApp.terminate(nil) }
        }
    }
}
