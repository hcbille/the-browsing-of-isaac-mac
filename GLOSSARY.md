# Mac Native Port

A native arm64 macOS build of The Binding of Isaac: Repentance+, made from the same recompiled game as the browser build, launched through Steam.

## The game and what runs it

**Guest**:
The original Windows game code, recompiled, together with the memory it believes it owns.
_Avoid_: the exe, the binary (when meaning the running game)

**Host**:
The native code that stands in for everything the Guest expected from Windows: files, windows, sound, input, Steam.
_Avoid_: shim layer, runtime, emulator

**Browser Build**:
The upstream build that runs the Guest as WebAssembly in a web page.
_Avoid_: web version, wasm build

**Mac Build**:
The build this project produces: the Guest compiled for Apple Silicon with a Host for macOS.
_Avoid_: native port, Mac version

## REPENTOGON

**REPENTOGON**:
The real Windows modding extension for the game. It never runs in the Mac Build.
_Avoid_: Repentogon support (ambiguous)

**Layer**:
A from-scratch rewrite of REPENTOGON's mod-facing functions and callbacks for this game version, built into the Mac Build rather than listed as a mod.
_Avoid_: rgon, REPENTOGON (when meaning the Layer), compat layer, mod loader

**Launcher**:
The Mac window that runs before the game, where the user turns the Layer and mods on or off, sees which mods are Shown, sets launch options and starts play.
_Avoid_: mod loader, mod manager, settings app

**Vanilla Launch**:
Starting the Mac Build with the Layer off, so no mod sees REPENTOGON.
_Avoid_: safe mode, unmodded

**Shown**:
A mod is Shown when the Layer decides that mod may see REPENTOGON as present, because the Layer covers what the mod uses.
_Avoid_: allowed, enabled, gated in

**Target Mod**:
Any mod in the user's Workshop collection; the Layer is done when every Target Mod that uses REPENTOGON gets through a full run.
_Avoid_: supported mod, priority mod

**Hard Dependent**:
A Target Mod that refuses to run at all without REPENTOGON, rather than falling back to its own code.
_Avoid_: REPENTOGON-only mod, required mod

## Saves

**Imported Save**:
A one-time copy of the user's existing Repentance+ progress into the Mac Build; it is never written back.
_Avoid_: synced save, cloud save (when meaning the Mac Build's copy)
