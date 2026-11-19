/* mac_gl.c -- the Mac Build's window, GL context, swap, pacing and screenshots.
 *
 * The GL calls themselves are upstream's host_gl_webgl.c, compiled natively
 * (GLES 3.0 against ANGLE). This file supplies what the browser supplied: a
 * drawable and a context (SDL3 + ANGLE on Metal, docs/mac/graphics-spike.md),
 * the per-frame present (event pump, 60 fps pacing, frame timing log,
 * ISAAC_MAC_SHOT screenshots), and the headless switch.
 *
 * Environment:
 *   ISAAC_MAC_HEADLESS=1       no window and no ANGLE: the fake GL answers
 *                              (the M1 path). mac_main also takes --headless.
 *   ISAAC_MAC_VSYNC=0          swap interval 0 (default 1)
 *   ISAAC_MAC_UNCAPPED=1       no 60 fps limiter (measures speed)
 *   ISAAC_MAC_STATS_EVERY=N    frame timing line every N presented frames (120)
 *   ISAAC_MAC_SHOT=F:path[,F:path...]  screenshot at presented frame F; @F is
 *                              F frames after the scripted run started
 *                              (mac_script.c); a relative path is
 *                              relative to where isaac-mac was started
 *   ISAAC_ANGLE_DIR            where libEGL.dylib / libGLESv2.dylib are (default: <exe dir>/../Frameworks
 *                              when it has them, as inside Repentance+.app, else the build's ANGLE_DIR)
 *                              (default: the directory baked in at build time)
 *   ISAAC_MAC_SCALE            integer (default) or fill (mac_window.c; also --scale,
 *                              launch-settings scale_mode)
 *   ISAAC_MAC_WINDOW=WxH       initial window size in points (also --window)
 *   ISAAC_MAC_WINDOW_BORDERLESS=1  test hook: a borderless window (macOS lets it be larger than the screen)
 *
 * The window opens at the game's client size, 960x540, and is resizable. The game
 * draws into an offscreen framebuffer of its own size; mac_window.c scales it into
 * the drawable (whole-number with bars, or fill), handles fullscreen, and maps the
 * mouse through the same rectangle. Screenshots (ISAAC_MAC_SHOT) read the composed
 * drawable, bars included.
 */
#include <SDL3/SDL.h>
#include <GLES3/gl3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <mach-o/dyld.h>

#ifndef ISAAC_ANGLE_DIR
#define ISAAC_ANGLE_DIR ""
#endif

#define NATIVE_W 960
#define NATIVE_H 540

uint32_t isaac_frames_presented(void);
void isaac_input_window_close(void);
int isaac_mac_script_anchor(void);              /* presented frame of the script's trigger, -1 until then */
int isaac_mac_png_write_rgb(const char *path, const uint8_t *rgb, int w, int h);
void isaac_mac_gl_install_fakes(void);
void isaac_mac_input_event(const SDL_Event *ev);
void isaac_mac_input_end_events(void);
void isaac_mac_window_initial_size(int *w, int *h);
int isaac_mac_window_gl_ready(void);
int isaac_mac_window_adapt(void);
void isaac_mac_window_compose(void);
void isaac_mac_window_frame(uint32_t frame);
void isaac_mac_window_event(const SDL_Event *ev);

static SDL_Window *g_win;
SDL_Window *isaac_mac_gl_window(void) { return g_win; }
static SDL_GLContext g_ctx;
static int g_state = -1;            /* -1 untried, 0 headless, 1 context current, -2 failed */
static int g_vsync = 1;

static uint64_t now_ns(void) { return clock_gettime_nsec_np(CLOCK_UPTIME_RAW); }

int isaac_mac_headless(void) {
    const char *e = getenv("ISAAC_MAC_HEADLESS");
    return e && *e && *e != '0';
}

int isaac_mac_gl_ready(void) { return g_state == 1; }

static void fail(const char *what) {
    fprintf(stderr, "[mac][gl] %s: %s\n", what, SDL_GetError());
}

/* 1 context current, 0 headless, -1 failed. Called from wglCreateContext, on
 * the thread that runs the Guest (the main thread: SDL's Cocoa windows need it). */
int isaac_mac_gl_open(void) {
    if (g_state != -1) return g_state < 0 ? -1 : g_state;
    if (isaac_mac_headless()) { g_state = 0; return 0; }

    const char *dir = getenv("ISAAC_ANGLE_DIR");
    /* inside Repentance+.app: Contents/MacOS/isaac-mac -> Contents/Frameworks (scripts/mac/build.sh stage 12) */
    static char fw[2048];
    if (!dir || !*dir) {
        char exe[1024], real[PATH_MAX], probe[2048];
        uint32_t n = sizeof exe;
        if (_NSGetExecutablePath(exe, &n) == 0 && realpath(exe, real)) {
            char *slash = strrchr(real, '/');
            if (slash) {
                *slash = 0;
                snprintf(fw, sizeof fw, "%s/../Frameworks", real);
                snprintf(probe, sizeof probe, "%s/libEGL.dylib", fw);
                if (access(probe, R_OK) == 0) dir = fw;
            }
        }
    }
    if (!dir || !*dir) dir = ISAAC_ANGLE_DIR;
    printf("[mac][gl] ANGLE from %s\n", *dir ? dir : "(default search)");
    char egl[2048], gles[2048];
    if (*dir) {
        snprintf(egl, sizeof egl, "%s/libEGL.dylib", dir);
        snprintf(gles, sizeof gles, "%s/libGLESv2.dylib", dir);
        SDL_SetHint(SDL_HINT_EGL_LIBRARY, egl);
        SDL_SetHint(SDL_HINT_OPENGL_LIBRARY, gles);
    }
    SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
    SDL_SetHint(SDL_HINT_VIDEO_FORCE_EGL, "1");
    /* SDL's Cocoa EGL path calls plain eglGetDisplay, so platform attributes
     * are ignored; without this ANGLE picks its desktop-GL backend. */
    setenv("ANGLE_DEFAULT_PLATFORM", "metal", 0);

    if (!SDL_Init(SDL_INIT_VIDEO)) { fail("SDL_Init"); g_state = -2; return -1; }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    int iw = NATIVE_W, ih = NATIVE_H;
    isaac_mac_window_initial_size(&iw, &ih);
    SDL_WindowFlags wf = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if (isaac_mac_window_adapt()) wf |= SDL_WINDOW_HIGH_PIXEL_DENSITY;   /* adapt: the drawable is in Retina pixels */
    /* test hook: macOS keeps a titled window inside the screen; a borderless one can be larger (2x/3x screenshots) */
    { const char *bl = getenv("ISAAC_MAC_WINDOW_BORDERLESS"); if (bl && *bl && *bl != '0') wf |= SDL_WINDOW_BORDERLESS; }
    g_win = SDL_CreateWindow("The Binding of Isaac: Repentance+", iw, ih, wf);
    if (!g_win) { fail("SDL_CreateWindow"); g_state = -2; return -1; }
    g_ctx = SDL_GL_CreateContext(g_win);
    if (!g_ctx) { fail("SDL_GL_CreateContext"); g_state = -2; return -1; }
    const char *v = getenv("ISAAC_MAC_VSYNC");
    g_vsync = !(v && *v == '0');
    SDL_GL_SetSwapInterval(g_vsync ? 1 : 0);
    SDL_SetWindowMinimumSize(g_win, 320, 180);
    SDL_RaiseWindow(g_win);

    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(g_win, &w, &h);
    const char *renderer = (const char *)glGetString(GL_RENDERER);
    printf("[mac][gl] GL_VENDOR   %s\n", (const char *)glGetString(GL_VENDOR));
    printf("[mac][gl] GL_RENDERER %s\n", renderer);
    printf("[mac][gl] GL_VERSION  %s\n", (const char *)glGetString(GL_VERSION));
    printf("[mac][gl] drawable %dx%d, swap interval %d\n", w, h, g_vsync);
    if (!renderer || !strstr(renderer, "ANGLE") || !strstr(renderer, "Metal"))
        printf("[mac][gl] WARNING: the renderer is not ANGLE on Metal\n");
    if (!isaac_mac_window_gl_ready()) { fprintf(stderr, "[mac][gl] the screen framebuffer could not be made\n"); g_state = -2; return -1; }
    g_state = 1;
    return 1;
}

void isaac_mac_gl_make_current(void) {
    if (g_state == 1) SDL_GL_MakeCurrent(g_win, g_ctx);
}

/* ---- screenshots ---------------------------------------------------------- */
#define MAX_SHOTS 32
static struct { int frame, rel; char path[1024]; int done; } g_shots[MAX_SHOTS];
static int g_nshots = -1;

static void shots_parse(void) {
    g_nshots = 0;
    const char *e = getenv("ISAAC_MAC_SHOT");
    if (!e || !*e) return;
    char *dup = strdup(e), *save = NULL;
    for (char *tok = strtok_r(dup, ",", &save); tok && g_nshots < MAX_SHOTS; tok = strtok_r(NULL, ",", &save)) {
        int rel = 0;
        if (*tok == '@') { rel = 1; ++tok; }
        char *colon = strchr(tok, ':');
        if (!colon) { fprintf(stderr, "[mac] ISAAC_MAC_SHOT: bad entry '%s' (want frame:path)\n", tok); continue; }
        *colon = 0;
        g_shots[g_nshots].frame = atoi(tok);
        g_shots[g_nshots].rel = rel;
        const char *cwd = getenv("ISAAC_MAC_LAUNCH_CWD");
        if (colon[1] != '/' && cwd)
            snprintf(g_shots[g_nshots].path, sizeof g_shots[g_nshots].path, "%s/%s", cwd, colon + 1);
        else
            snprintf(g_shots[g_nshots].path, sizeof g_shots[g_nshots].path, "%s", colon + 1);
        ++g_nshots;
    }
    free(dup);
}

static void shot_take(const char *path) {
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(g_win, &w, &h);
    uint8_t *rgba = malloc((size_t)w * h * 4), *rgb = malloc((size_t)w * h * 3);
    if (!rgba || !rgb) { free(rgba); free(rgb); return; }
    GLint prev = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prev);
    for (int y = 0; y < h; ++y) {                      /* GL rows run bottom-up */
        const uint8_t *s = rgba + (size_t)(h - 1 - y) * w * 4;
        uint8_t *d = rgb + (size_t)y * w * 3;
        for (int x = 0; x < w; ++x) { d[3 * x] = s[4 * x]; d[3 * x + 1] = s[4 * x + 1]; d[3 * x + 2] = s[4 * x + 2]; }
    }
    int ok = isaac_mac_png_write_rgb(path, rgb, w, h);
    printf("[mac] screenshot %s (%dx%d) at presented frame %u: %s\n", path, w, h, isaac_frames_presented(), ok ? "written" : "FAILED");
    free(rgba);
    free(rgb);
}

static void shots_check(uint32_t frame) {
    if (g_nshots < 0) shots_parse();
    for (int i = 0; i < g_nshots; ++i) {
        if (g_shots[i].done) continue;
        int target = g_shots[i].frame;
        if (g_shots[i].rel) {
            int a = isaac_mac_script_anchor();
            if (a < 0) continue;
            target += a;
        }
        if ((int)frame >= target) { g_shots[i].done = 1; shot_take(g_shots[i].path); }
    }
}

/* ---- pacing and timing ------------------------------------------------------ */
#define FRAME_NS 16666667ull
static uint64_t g_deadline;          /* when the next present may happen */
static uint64_t g_last_end;          /* end of the previous present */
static uint64_t g_last_swap;
static struct {
    uint32_t n, first;
    double sum, min, max, work_sum, work_max;
    uint32_t slow;
} g_st;
static double g_all_sum, g_all_min = 1e9, g_all_max;
static uint32_t g_all_n;
static int g_quit_frame = -1;

static void pace(void) {
    static int uncapped = -1;
    if (uncapped < 0) { const char *e = getenv("ISAAC_MAC_UNCAPPED"); uncapped = e && *e && *e != '0'; }
    if (uncapped) return;
    uint64_t t = now_ns();
    if (g_deadline == 0 || t > g_deadline + 4 * FRAME_NS) g_deadline = t;   /* fell far behind: do not catch up in a burst */
    while ((t = now_ns()) + 600000ull < g_deadline) {
        uint64_t left = g_deadline - t - 500000ull;
        struct timespec ts = { (time_t)(left / 1000000000ull), (long)(left % 1000000000ull) };
        nanosleep(&ts, NULL);
    }
    while (now_ns() < g_deadline) { /* spin the last half millisecond */ }
    g_deadline += FRAME_NS;
}

static void stats_frame(uint64_t start, uint64_t end, uint32_t frame) {
    if (g_last_swap) {
        double dt = (double)(end - g_last_swap) / 1e6;
        double work = (double)(start - g_last_end) / 1e6;
        if (!g_st.n) g_st.first = frame;
        ++g_st.n;
        g_st.sum += dt;
        if (g_st.n == 1 || dt < g_st.min) g_st.min = dt;
        if (dt > g_st.max) g_st.max = dt;
        g_st.work_sum += work;
        if (work > g_st.work_max) g_st.work_max = work;
        if (dt > 20.0) ++g_st.slow;
        if (frame > 30) {                     /* the load-time frames are not a steady state */
            ++g_all_n; g_all_sum += dt;
            if (dt < g_all_min) g_all_min = dt;
            if (dt > g_all_max) g_all_max = dt;
        }
        static uint32_t every;
        if (!every) { const char *e = getenv("ISAAC_MAC_STATS_EVERY"); every = e && atoi(e) > 0 ? (uint32_t)atoi(e) : 120u; }
        if (g_st.n >= every) {
            printf("[mac][frame] frames %u-%u: avg %.2f ms (%.1f fps) min %.2f max %.2f | game work avg %.2f max %.2f ms | %u over 20 ms\n",
                   g_st.first, frame, g_st.sum / g_st.n, 1000.0 * g_st.n / g_st.sum, g_st.min, g_st.max,
                   g_st.work_sum / g_st.n, g_st.work_max, g_st.slow);
            memset(&g_st, 0, sizeof g_st);
        }
    }
    g_last_swap = end;
    g_last_end = end;
}

static void atexit_report(void) {
    if (g_all_n)
        printf("[mac][frame] steady state (%u frames after frame 30): avg %.2f ms (%.1f fps) min %.2f max %.2f\n",
               g_all_n, g_all_sum / g_all_n, 1000.0 * g_all_n / g_all_sum, g_all_min, g_all_max);
}

/* ---- the per-frame present (called from SwapBuffers through host_gl_webgl.c) ---- */
void isaac_mac_present(void) {
    static int registered;
    if (!registered) { registered = 1; atexit(atexit_report); }
    { extern void mac_boot_ready(void); mac_boot_ready(); }   /* first frame: "[mac][boot] ready" (once) */
    uint32_t frame = isaac_frames_presented();
    uint64_t start = now_ns();
    isaac_mac_window_compose();          /* the game's picture into the drawable (mac_window.c) */
    shots_check(frame);                  /* ... so a screenshot shows the whole drawable, bars included */

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_EVENT_QUIT || ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            if (g_quit_frame < 0) {
                printf("[mac] quit requested: asking the game to close\n");
                g_quit_frame = (int)frame;
                isaac_input_window_close();
            }
        }
        isaac_mac_window_event(&ev);     /* fullscreen changes by the system (mac_window.c) */
        isaac_mac_input_event(&ev);      /* keyboard, mouse, focus (mac_input.c) */
    }
    isaac_mac_input_end_events();
    isaac_mac_window_frame(frame);       /* Options.Fullscreen changed by the game's menu? */
    /* the game did not leave within a second: end it ourselves */
    if (g_quit_frame >= 0 && (int)frame > g_quit_frame + 90) {
        printf("[mac] the game did not close; exiting\n");
        { extern void isaac_mac_log_finish(void); isaac_mac_log_finish(); }
        fflush(NULL);
        _exit(0);
    }

    { extern void mac_launch_frame(uint32_t); mac_launch_frame(frame); }   /* debug console option (mac_launch.c) */
    { extern void isaac_mac_audio_tick(void); isaac_mac_audio_tick(); }
    pace();
    SDL_GL_SwapWindow(g_win);
    stats_frame(start, now_ns(), frame);
}

void isaac_mac_gl_shutdown(void) {
    if (g_ctx) SDL_GL_DestroyContext(g_ctx);
    if (g_win) SDL_DestroyWindow(g_win);
    g_ctx = NULL;
    g_win = NULL;
    if (g_state == 1) SDL_Quit();
    g_state = -1;
}
