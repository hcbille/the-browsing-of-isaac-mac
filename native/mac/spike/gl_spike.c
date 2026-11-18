/* Graphics spike: SDL3 window + OpenGL ES 3.0 through ANGLE's Metal backend.
 *
 * Proves the Mac Build's graphics route (Q4: ANGLE on Metal) before any Guest
 * code exists: open a window, get an ES 3 context from ANGLE's EGL, draw a
 * triangle, read a pixel back, print the renderer string.
 *
 * Build (ANGLE dylibs in $ANGLE_DIR):
 *   clang -O1 gl_spike.c -o gl_spike $(pkg-config --cflags --libs sdl3) \
 *     -L$ANGLE_DIR -lGLESv2 -Wl,-rpath,$ANGLE_DIR
 * Run:  ANGLE_DIR=... ./gl_spike [seconds]
 */
#include <SDL3/SDL.h>
#include <GLES3/gl3.h>   /* Khronos headers; Emscripten's sysroot carries a copy */
#include <stdio.h>
#include <stdlib.h>

/* Without this ANGLE picks its OpenGL backend (Apple's deprecated GL 4.1);
 * ask for the Metal backend explicitly when SDL creates the EGL display. */
#define EGL_PLATFORM_ANGLE_TYPE_ANGLE        0x3203
#define EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE  0x3489
static SDL_EGLAttrib *metal_platform_attribs(void *userdata) {
    (void)userdata;
    static SDL_EGLAttrib attrs[] = {
        EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE,
        0x3038 /* EGL_NONE */
    };
    SDL_EGLAttrib *copy = SDL_malloc(sizeof attrs);   /* SDL frees it */
    SDL_memcpy(copy, attrs, sizeof attrs);
    return copy;
}

static GLuint compile(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof log, NULL, log);
        fprintf(stderr, "shader: %s\n", log);
        exit(1);
    }
    return s;
}

int main(int argc, char **argv) {
    double seconds = argc > 1 ? atof(argv[1]) : 2.0;
    const char *dir = getenv("ANGLE_DIR");
    char egl[1024], gles[1024];
    if (dir) {
        snprintf(egl, sizeof egl, "%s/libEGL.dylib", dir);
        snprintf(gles, sizeof gles, "%s/libGLESv2.dylib", dir);
        SDL_SetHint(SDL_HINT_EGL_LIBRARY, egl);
        SDL_SetHint(SDL_HINT_OPENGL_LIBRARY, gles);
    }
    SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
    SDL_SetHint(SDL_HINT_VIDEO_FORCE_EGL, "1");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    /* SDL's Cocoa EGL path calls plain eglGetDisplay, so the platform
     * attribute callback never runs there; ANGLE's own default-platform
     * variable is what selects Metal. Keep the callback for paths that use
     * eglGetPlatformDisplay. ANGLE_GL_BACKEND=1 forces the GL backend. */
    if (!getenv("ANGLE_GL_BACKEND")) {
        setenv("ANGLE_DEFAULT_PLATFORM", "metal", 0);
        SDL_EGL_SetAttributeCallbacks(metal_platform_attribs, NULL, NULL, NULL);
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    SDL_Window *win = SDL_CreateWindow("gl spike", 960, 540, SDL_WINDOW_OPENGL);
    if (!win) { fprintf(stderr, "window: %s\n", SDL_GetError()); return 1; }
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) { fprintf(stderr, "context: %s\n", SDL_GetError()); return 1; }
    SDL_GL_SetSwapInterval(1);

    printf("GL_VENDOR   %s\n", glGetString(GL_VENDOR));
    printf("GL_RENDERER %s\n", glGetString(GL_RENDERER));
    printf("GL_VERSION  %s\n", glGetString(GL_VERSION));

    const char *vs =
        "#version 300 es\n"
        "in vec2 p; void main(){ gl_Position = vec4(p, 0.0, 1.0); }\n";
    const char *fs =
        "#version 300 es\nprecision mediump float;\n"
        "out vec4 c; void main(){ c = vec4(0.69, 0.23, 0.18, 1.0); }\n";
    GLuint prog = glCreateProgram();
    glAttachShader(prog, compile(GL_VERTEX_SHADER, vs));
    glAttachShader(prog, compile(GL_FRAGMENT_SHADER, fs));
    glBindAttribLocation(prog, 0, "p");
    glLinkProgram(prog);
    glUseProgram(prog);

    static const float tri[] = { -0.6f, -0.6f, 0.6f, -0.6f, 0.0f, 0.7f };
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

    Uint64 end = SDL_GetTicks() + (Uint64)(seconds * 1000);
    int frames = 0;
    unsigned char px[4] = {0};
    while (SDL_GetTicks() < end) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) if (e.type == SDL_EVENT_QUIT) end = 0;
        glClearColor(0.11f, 0.08f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        if (frames == 0) {
            int w, h;
            SDL_GetWindowSizeInPixels(win, &w, &h);
            glReadPixels(w / 2, h / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
        }
        SDL_GL_SwapWindow(win);
        frames++;
    }
    printf("centre pixel %u,%u,%u  frames=%d in %.1fs  glError=0x%x\n",
           px[0], px[1], px[2], frames, seconds, glGetError());
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
