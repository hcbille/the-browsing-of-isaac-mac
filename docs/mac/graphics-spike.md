# Graphics spike: SDL3 + ANGLE on Metal

`native/mac/spike/gl_spike.c`, measured 2026-11-18 on an M2, macOS 27, SDL 3.4.12,
ANGLE 2.1.28222 (taken from Playwright's Chromium 151 headless shell).

Result: `ANGLE (Apple, ANGLE Metal Renderer: Apple M2, ...)`, OpenGL ES 3.0,
correct pixel read back, vsync ~60 fps.

Gotchas the real Host must keep:

1. **ANGLE defaults to its OpenGL backend on macOS** (renderer string
   `OpenGL 4.1 Metal`, i.e. Apple's deprecated GL). Metal must be asked for.
2. **SDL3's Cocoa EGL path calls plain `eglGetDisplay`**, so
   `SDL_EGL_SetAttributeCallbacks` platform attributes are never used there.
   Set `ANGLE_DEFAULT_PLATFORM=metal` in the process environment before
   `SDL_Init`. An EGL-only probe with `EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE`
   confirms the dylibs have the Metal backend.
3. **Chromium's dylibs have install name `./libGLESv2.dylib`.** Rewrite to
   `@rpath/...` with `install_name_tool -id` and re-sign ad hoc
   (`codesign -f -s -`), or they only load from the working directory.
4. SDL needs `SDL_HINT_OPENGL_ES_DRIVER=1`, `SDL_HINT_VIDEO_FORCE_EGL=1`, and
   `SDL_HINT_EGL_LIBRARY` / `SDL_HINT_OPENGL_LIBRARY` pointing at ANGLE.
5. GL headers: the Khronos `GLES2`/`GLES3`/`KHR`/`EGL` folders only, not a whole
   sysroot (Emscripten's libc headers clash with the macOS SDK).
