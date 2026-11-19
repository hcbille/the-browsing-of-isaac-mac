/* mac_window.c -- the Mac Build's window modes (M5): scaling, fullscreen, mouse mapping.
 *
 * In integer and fill the game renders at its own client size, 960x540 (the Guest's window
 * size, DAT_00c799a8/ac; the Host's fake display is 960x540 too, so the game's own
 * fullscreen setup leaves the picture at that size). Its "default framebuffer"
 * is an offscreen framebuffer of that size (host_gl_webgl.c maps framebuffer 0 to
 * isaac_mac_screen_fbo()). At present the picture is scaled into the real
 * drawable and centred:
 *
 *   integer (default)  the largest whole multiple of the game size that fits (1x, 2x, 3x ...),
 *                      nearest filtering, black bars on both axes. A window smaller than 1x
 *                      falls back to a fit (linear) so the whole picture stays visible.
 *   fill               the largest size that fits with the aspect kept (a fractional
 *                      scale), linear filtering unless it happens to be whole, black bars
 *                      on the short axis.
 *
 *   adapt (default)    the GAME does the layout, as the PC build does when its window is resized or
 *                      made full screen: the drawable's pixel size (Retina, SDL_WINDOW_HIGH_PIXEL_DENSITY)
 *                      divided by the smallest whole k that makes it at most 1920 wide is the game's
 *                      window size (adapt_k: the game's own layout, FUN_00954320, floors 960/width+0.5
 *                      for a factor that is 0 above 1920 and then renders 0x0 and crashes). The Host
 *                      tells the game that size the way Windows would: the fake display
 *                      (host_shims_win.c) answers it, GetClientRect answers it, and a WM_SIZE goes
 *                      through the game's own GLFW window procedure into its framebuffer-size callback
 *                      (isaac_native_window_resized), at the first frame and at every change of the
 *                      drawable (window resize, full screen on/off). The game then picks its own
 *                      whole-number scale (min(w/400, h/240) floored, capped by Options MaxScale),
 *                      centres its viewport and anchors the HUD to the edges; the offscreen
 *                      framebuffer is the game's window size and compose is a whole-number (k)
 *                      nearest blit, so every game pixel is an N x N block; only the odd leftover
 *                      pixels (under k per side) are bars.
 *
 * Mode: --scale adapt|integer|fill, launch-settings key scale_mode, env REPENTANCE_MAC_SCALE /
 * ISAAC_MAC_SCALE (mac_launch.c). Window size at start: --window WxH, ISAAC_MAC_WINDOW=WxH (points).
 * The window is resizable. integer and fill render in points (no Retina); adapt in pixels.
 *
 * Fullscreen is SDL3's desktop (borderless, Spaces) fullscreen. It follows the game's
 * Fullscreen option (Options.Fullscreen, the byte at Manager+0x2a360: Manager is the
 * pointer at 0x00c7169c, the Options struct sits at Manager+0x2a33c as
 * EnableDebugConsole at +0x5c is Manager+0x2a398, and Fullscreen is Options+0x24, set by
 * FUN_00925c60): a change of the byte, by the options menu or by the game's own F key,
 * toggles the window (polled once per frame). F is the game's fullscreen key on every
 * platform, so plain F needs nothing here: it reaches the game (mac_input.c forwards it)
 * and the game toggles its option. Cmd+Ctrl+F, the macOS fullscreen key, presses F for
 * the game the same way (mac_input.c does not forward that key), so the option, the
 * menu, options.ini and the window always agree. Neither works while the debug
 * console is open: the console takes F as text.
 *
 * The mouse goes through the same rectangle (isaac_mac_window_to_game); a press in the
 * bars is ignored, a move there is clamped to the nearest game pixel.
 */
#include <SDL3/SDL.h>
#include <GLES3/gl3.h>
#include <dlfcn.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "isaac_host.h"
#include "mac_launch.h"

SDL_Window *isaac_mac_gl_window(void);          /* mac_gl.c */
uint32_t isaac_frames_presented(void);
void isaac_input_key(uint32_t vk, uint32_t scancode, int extended, int down);

#define MANAGER_PTR   0x00c7169cu
#define OPT_FULLSCREEN 0x2a360u                 /* Manager + Options (0x2a33c) + 0x24 */
#define GAME_PTR      0x00c71678u
#define CONSOLE_OFF   0x68d78u                  /* Game + this: the console's state dword, 0 closed */
#define GUEST_WIN_W   0x00c799a8u               /* the game's window size, what its glViewport uses */
#define GUEST_WIN_H   0x00c799acu

static GLuint g_fbo, g_rb_color, g_rb_ds;
static int g_gw = 960, g_gh = 540;              /* the game's framebuffer size */
static int g_fs;                                /* fullscreen as last requested or reported */
static int g_opt_seen = -1;                     /* Options.Fullscreen as last seen */

/* x, y, w, h: where the picture lands in the drawable (top-left origin). sx, sy_gl, sw, sh: the part of the game
 * framebuffer shown (sx from the left, sy_gl from the bottom, GL style); the whole framebuffer unless adapt shows only
 * the game's own viewport (a cutscene's 960x540). */
typedef struct { int x, y, w, h, linear; int sx, sy_gl, sw, sh; } wrect;

/* ---- adapt mode ------------------------------------------------------------------- */
static int g_adapt;                             /* 1: the game renders at the drawable's pixel size */
static int g_told_w, g_told_h;                  /* the window size the game was last told (pixels) */
void isaac_native_adapt_begin(int disp_w, int disp_h);   /* host_shims_win.c */
void isaac_native_window_resized(int w, int h);

int isaac_mac_window_adapt(void) { return g_adapt; }

/* The game's own layout (FUN_00954320, found in the decompile) takes floor(960 / width + 0.5) as its "DP per
 * point": 1 up to a window 1920 wide, 0 above, and then its render size is 0x0 and it crashes. So the game is
 * given the drawable divided by the smallest whole k that makes it at most 1920 wide, and the picture is
 * blitted back up by k (nearest, whole pixels). A Retina drawable of 3420 wide is a 1710 wide game window, k 2. */
#define ADAPT_MAX_W 1920
static int adapt_k(int pw) {
    int k = 1;
    while (k < 16 && pw / k > ADAPT_MAX_W) ++k;
    return k;
}
/* the game's window size for a drawable */
static void adapt_game_size(int pw, int ph, int *gw, int *gh) {
    int k = adapt_k(pw);
    *gw = pw / k; *gh = ph / k;
}

/* the main display's size in pixels and points (CoreGraphics through dlsym: the Host links only SDL) */
static int display_px(int *pxw, int *pxh, int *ptw, int *pth) {
    void *cg = dlopen("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics", RTLD_LAZY);
    if (!cg) return 0;
    typedef uint32_t (*f_main)(void);
    typedef void *(*f_mode)(uint32_t);
    typedef size_t (*f_dim)(void *);
    typedef void (*f_rel)(void *);
    f_main fmain = (f_main)dlsym(cg, "CGMainDisplayID");
    f_mode fmode = (f_mode)dlsym(cg, "CGDisplayCopyDisplayMode");
    f_dim fpw = (f_dim)dlsym(cg, "CGDisplayModeGetPixelWidth"), fph = (f_dim)dlsym(cg, "CGDisplayModeGetPixelHeight");
    f_dim fw = (f_dim)dlsym(cg, "CGDisplayModeGetWidth"), fh = (f_dim)dlsym(cg, "CGDisplayModeGetHeight");
    f_rel frel = (f_rel)dlsym(cg, "CGDisplayModeRelease");
    if (!fmain || !fmode || !fpw || !fph || !fw || !fh) return 0;
    void *m = fmode(fmain());
    if (!m) return 0;
    *pxw = (int)fpw(m); *pxh = (int)fph(m); *ptw = (int)fw(m); *pth = (int)fh(m);
    if (frel) frel(m);
    return *pxw > 0 && *pxh > 0 && *ptw > 0 && *pth > 0;
}

/* before the game creates its window: the fake display is the real display's pixel size, its first window
 * is the initial window (points) in pixels. Headless and the other modes leave the 960x540 display. */
void isaac_mac_window_adapt_begin(void) {
    const char *hl = getenv("ISAAC_MAC_HEADLESS");
    g_adapt = g_mac_cfg.scale_adapt && !(hl && *hl && *hl != '0');
    if (!g_adapt) return;
    int pxw = 1920, pxh = 1080, ptw = 960, pth = 540;
    if (!display_px(&pxw, &pxh, &ptw, &pth)) printf("[mac][window] adapt: the display size is unknown, assuming 1920x1080\n");
    int dw, dh;                                 /* the display as the game sees it: the pixels divided by k (see adapt_k) */
    adapt_game_size(pxw, pxh, &dw, &dh);
    isaac_native_adapt_begin(dw, dh);
    g_told_w = 0; g_told_h = 0;                 /* so the first frame tells the game its real size (it starts from its option, 960x540) */
    printf("[mac][window] adapt: display %dx%d pixels (%dx%d points); the game sees a %dx%d display (k %d)\n",
           pxw, pxh, ptw, pth, dw, dh, adapt_k(pxw));
}

/* ---- the picture's rectangle -------------------------------------------------- */
static void layout(int pw, int ph, wrect *r) {
    int gw = g_gw, gh = g_gh;
    *r = (wrect){ 0, 0, gw, gh, 0, 0, 0, gw, gh };
    if (g_adapt) {
        /* Show the game's own viewport (0x00c799a8/ac at the origin the game's glViewport uses): in a room it is the
         * whole window less the game's few padding pixels, in a cutscene (FUN_00954150) a fixed 960x540. It is scaled by
         * the largest whole number that fits the drawable (nearest) and centred, black around it: every game pixel is
         * an N x N block, as the PC build shows a 16:9 cutscene in a bigger window. */
        int vw = (int)isaac_r32(GUEST_WIN_W), vh = (int)isaac_r32(GUEST_WIN_H), ox = 0, oy = 0;
        uint32_t fl = isaac_r32(0x00c798e4u), bx = isaac_r32((fl & 0x40u) ? 0x00c79984u : 0x00c7b640u),
                 by = isaac_r32((fl & 0x40u) ? 0x00c79988u : 0x00c7b644u);
        float fx, fy;
        memcpy(&fx, &bx, 4); memcpy(&fy, &by, 4);
        if (fx >= 0.f && fx < 16384.f) ox = (int)fx;
        if (fy >= 0.f && fy < 16384.f) oy = (int)fy;
        if (vw < 64 || vh < 64 || ox + vw > gw || oy + vh > gh) { vw = gw; vh = gh; ox = oy = 0; }   /* not (yet) valid: all of it */
        int m = 1;
        if (pw > 0 && ph > 0) { int mx = pw / vw, my = ph / vh; m = mx < my ? mx : my; if (m < 1) m = 1; }
        r->w = vw * m; r->h = vh * m; r->linear = 0;
        r->x = (pw - r->w) / 2; r->y = (ph - r->h) / 2;
        if (r->x < 0) r->x = 0;
        if (r->y < 0) r->y = 0;
        r->sx = ox; r->sy_gl = oy; r->sw = vw; r->sh = vh;
        return;
    }
    if (pw <= 0 || ph <= 0) return;
    int sx = pw / gw, sy = ph / gh, s = sx < sy ? sx : sy;
    if (!g_mac_cfg.scale_fill && s >= 1) {
        r->w = gw * s; r->h = gh * s; r->linear = 0;
    } else {
        double fx = (double)pw / gw, fy = (double)ph / gh, f = fx < fy ? fx : fy;
        r->w = (int)(gw * f + 0.5); r->h = (int)(gh * f + 0.5);
        if (r->w > pw) r->w = pw;
        if (r->h > ph) r->h = ph;
        if (r->w < 1) r->w = 1;
        if (r->h < 1) r->h = 1;
        r->linear = !(r->w % gw == 0 && r->h % gh == 0);
    }
    r->x = (pw - r->w) / 2;
    r->y = (ph - r->h) / 2;
}

static void sizes(SDL_Window *win, int *ww, int *wh, int *pw, int *ph) {
    SDL_GetWindowSize(win, ww, wh);
    SDL_GetWindowSizeInPixels(win, pw, ph);
    if (*ww <= 0 || *wh <= 0) { *ww = *pw; *wh = *ph; }
}

/* window (point) coordinates -> game pixel; 1 when inside the picture, 0 in the bars (the result is clamped) */
int isaac_mac_window_to_game(SDL_Window *win, float wx, float wy, int *gx, int *gy) {
    if (!win) win = isaac_mac_gl_window();
    int ww = g_gw, wh = g_gh, pw = g_gw, ph = g_gh;
    if (win) sizes(win, &ww, &wh, &pw, &ph);
    wrect r;
    layout(pw, ph, &r);
    float px = wx * (float)pw / (float)(ww > 0 ? ww : 1), py = wy * (float)ph / (float)(wh > 0 ? wh : 1);
    int x = (int)floorf((px - (float)r.x) * (float)r.sw / (float)r.w);
    int y = (int)floorf((py - (float)r.y) * (float)r.sh / (float)r.h);
    int inside = x >= 0 && y >= 0 && x < r.sw && y < r.sh;
    x += r.sx;                                      /* the shown part's corner, in the game's top-left coordinates */
    y += g_gh - (r.sy_gl + r.sh);
    *gx = x < 0 ? 0 : x >= g_gw ? g_gw - 1 : x;
    *gy = y < 0 ? 0 : y >= g_gh ? g_gh - 1 : y;
    return inside;
}

/* the middle of a game pixel in window (point) coordinates (scripted mouse) */
void isaac_mac_game_to_window(int gx, int gy, float *wx, float *wy) {
    SDL_Window *win = isaac_mac_gl_window();
    int ww = g_gw, wh = g_gh, pw = g_gw, ph = g_gh;
    if (win) sizes(win, &ww, &wh, &pw, &ph);
    wrect r;
    layout(pw, ph, &r);
    float px = (float)r.x + ((float)(gx - r.sx) + 0.5f) * (float)r.w / (float)r.sw;
    float py = (float)r.y + ((float)(gy - (g_gh - (r.sy_gl + r.sh))) + 0.5f) * (float)r.h / (float)r.sh;
    *wx = px * (float)ww / (float)pw;
    *wy = py * (float)wh / (float)ph;
}

int isaac_mac_window_game_size(int *w, int *h) { *w = g_gw; *h = g_gh; return 1; }

/* ---- the offscreen framebuffer --------------------------------------------------- */
static int fbo_alloc(int w, int h) {
    GLint pfb = 0, prb = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &pfb);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &prb);
    int first = !g_fbo;
    if (first) { glGenFramebuffers(1, &g_fbo); glGenRenderbuffers(1, &g_rb_color); glGenRenderbuffers(1, &g_rb_ds); }
    GLenum st = 0;
    for (int pass = 0; pass < 2; ++pass) {             /* RGB8 like the window's own (alpha-less) buffer, RGBA8 as a fallback */
        glBindRenderbuffer(GL_RENDERBUFFER, g_rb_color);
        glRenderbufferStorage(GL_RENDERBUFFER, pass ? GL_RGBA8 : GL_RGB8, w, h);
        glBindRenderbuffer(GL_RENDERBUFFER, g_rb_ds);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
        glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, g_rb_color);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_rb_ds);
        st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (st == GL_FRAMEBUFFER_COMPLETE) break;
    }
    int ok = st == GL_FRAMEBUFFER_COMPLETE;
    if (!ok) fprintf(stderr, "[mac][window] the %dx%d screen framebuffer is incomplete (0x%x)\n", w, h, st);
    glBindFramebuffer(GL_FRAMEBUFFER, first ? g_fbo : (GLuint)pfb);
    glBindRenderbuffer(GL_RENDERBUFFER, (GLuint)prb);
    if (ok) { g_gw = w; g_gh = h; }
    return ok;
}

uint32_t isaac_mac_screen_fbo(void) { return g_fbo; }

/* called once, with the context current, before the first frame */
int isaac_mac_window_gl_ready(void) {
    int w = (int)isaac_r32(GUEST_WIN_W), h = (int)isaac_r32(GUEST_WIN_H);
    if (w < 64 || h < 64 || w > 8192 || h > 8192) { w = 960; h = 540; }
    if (g_adapt) {                                      /* the drawable's size, not the game's viewport size */
        SDL_Window *win = isaac_mac_gl_window();
        int pw = 0, ph = 0;
        if (win) SDL_GetWindowSizeInPixels(win, &pw, &ph);
        if (pw >= 64 && ph >= 64) adapt_game_size(pw, ph, &w, &h);
    }
    if (!fbo_alloc(w, h)) return 0;
    glViewport(0, 0, g_gw, g_gh);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    printf("[mac][window] game framebuffer %dx%d, scale mode %s\n", g_gw, g_gh, g_adapt ? "adapt" : g_mac_cfg.scale_fill ? "fill" : "integer");
    return 1;
}

void isaac_mac_window_initial_size(int *w, int *h) {
    *w = g_mac_cfg.win_w > 0 ? g_mac_cfg.win_w : 960;
    *h = g_mac_cfg.win_h > 0 ? g_mac_cfg.win_h : 540;
}

/* ---- compose the picture into the window (every present, before the swap) --------------- */
void isaac_mac_window_compose(void) {
    SDL_Window *win = isaac_mac_gl_window();
    if (!g_fbo || !win) return;
    int ww, wh, pw, ph;
    sizes(win, &ww, &wh, &pw, &ph);
    if (g_adapt && pw >= 16 && ph >= 16 && pw <= 16384 && ph <= 16384) {
        int aw, ah;                     /* the drawable changed: the screen framebuffer follows (the game's window size) */
        adapt_game_size(pw, ph, &aw, &ah);
        if (aw != g_gw || ah != g_gh) {
            int ow = g_gw, oh = g_gh;
            if (fbo_alloc(aw, ah)) printf("[mac][window] screen framebuffer %dx%d (was %dx%d)\n", aw, ah, ow, oh);
        }
    }
    wrect r;
    layout(pw, ph, &r);

    GLint rfb = 0, dfb = 0;
    GLfloat cc[4];
    GLboolean cm[4];
    GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &rfb);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &dfb);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, cc);
    glGetBooleanv(GL_COLOR_WRITEMASK, cm);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, g_fbo);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBlitFramebuffer(r.sx, r.sy_gl, r.sx + r.sw, r.sy_gl + r.sh, r.x, ph - (r.y + r.h), r.x + r.w, ph - r.y,
                      GL_COLOR_BUFFER_BIT, r.linear ? GL_LINEAR : GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)rfb);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)dfb);
    glClearColor(cc[0], cc[1], cc[2], cc[3]);
    glColorMask(cm[0], cm[1], cm[2], cm[3]);
    if (scissor) glEnable(GL_SCISSOR_TEST);

    static int lpw, lph, lx, ly, lw, lh, lfs = -1;
    if (pw != lpw || ph != lph || r.x != lx || r.y != ly || r.w != lw || r.h != lh || g_fs != lfs) {
        lpw = pw; lph = ph; lx = r.x; ly = r.y; lw = r.w; lh = r.h; lfs = g_fs;
        printf("[mac][window] %s %dx%d (window %dx%d points): game %dx%d at %d,%d size %dx%d (x%.2f, %s), bars l/r %d/%d t/b %d/%d, mode %s\n",
               g_fs ? "fullscreen" : "windowed", pw, ph, ww, wh, g_gw, g_gh, r.x, r.y, r.w, r.h,
               (double)r.w / r.sw, r.linear ? "linear" : "nearest", r.x, pw - r.x - r.w, r.y, ph - r.y - r.h,
               g_adapt ? "adapt" : g_mac_cfg.scale_fill ? "fill" : "integer");
    }

    /* the game changed its window size (a mod, an option): follow it. The picture of this frame is already shown. */
    int gw = (int)isaac_r32(GUEST_WIN_W), gh = (int)isaac_r32(GUEST_WIN_H);
    if (!g_adapt && gw >= 64 && gh >= 64 && gw <= 8192 && gh <= 8192 && (gw != g_gw || gh != g_gh)) {
        int ow = g_gw, oh = g_gh;
        if (fbo_alloc(gw, gh)) printf("[mac][window] the game's window is now %dx%d (was %dx%d)\n", gw, gh, ow, oh);
    }
}

/* ---- fullscreen ---------------------------------------------------------------------- */
static uint8_t *opt_fullscreen_byte(void) {
    uint32_t mgr = isaac_r32(MANAGER_PTR);
    return mgr ? (uint8_t *)isaac_g(mgr + OPT_FULLSCREEN) : NULL;
}

/* on: 1 enter, 0 leave, -1 toggle. write_opt: also set Options.Fullscreen (the shortcut path). Returns the new state. */
static int fullscreen_set(int on, int write_opt, const char *why) {
    SDL_Window *win = isaac_mac_gl_window();
    if (!win) return 0;
    int want = on < 0 ? !g_fs : on != 0;
    if (want != g_fs) {
        if (!SDL_SetWindowFullscreen(win, want ? true : false)) {
            fprintf(stderr, "[mac][window] SDL_SetWindowFullscreen(%d) failed: %s\n", want, SDL_GetError());
            return g_fs;
        }
        g_fs = want;
        printf("[mac][window] fullscreen %s by %s (frame %u)\n", want ? "on" : "off", why, isaac_frames_presented());
    }
    uint8_t *p = opt_fullscreen_byte();
    if (write_opt && p && *p != (uint8_t)want) *p = (uint8_t)want;
    if (p) g_opt_seen = *p;
    return g_fs;
}

int isaac_mac_console_open(void);
int isaac_mac_window_toggle_fullscreen(const char *why);
int isaac_mac_window_set_fullscreen(int on, const char *why) {
    return (on != 0) == (g_fs != 0) ? g_fs : isaac_mac_window_toggle_fullscreen(why);
}

/* Cmd+Ctrl+F (and the script's fullscreen toggle). F is the game's own fullscreen key: the game toggles
 * Options.Fullscreen and its window state itself (and keeps the byte, which it re-syncs from its own
 * window state, in step), and the poll below follows the byte. So the shortcut presses F for the game; if
 * the game has not changed the option after a few frames (a menu that ignores F), the window is toggled
 * directly. Not while the debug console is open: F would be typed into it. */
static int g_f_pending = -1;                     /* frame the shortcut pressed F, -1 none */
static int g_f_down_at = -1;                     /* frame F was pressed; released 4 frames later */
static int g_f_before;

int isaac_mac_window_toggle_fullscreen(const char *why) {
    uint8_t *p = opt_fullscreen_byte();
    if (!p || g_f_pending >= 0) return g_fs;
    if (isaac_mac_console_open()) {
        printf("[mac][window] %s ignored: the debug console is open\n", why);
        return g_fs;
    }
    g_f_before = *p;
    g_f_pending = (int)isaac_frames_presented();
    g_f_down_at = g_f_pending;
    printf("[mac][window] %s: pressing the game's own fullscreen key, F (frame %d, Options.Fullscreen is %d)\n", why, g_f_pending, g_f_before);
    isaac_input_key(0x46, 0x21, 0, 1);
    return g_fs;
}

/* a poll of Options.Fullscreen, once per frame: the game's F key and its options menu change it */
void isaac_mac_window_frame(uint32_t frame) {
    if (g_adapt) {                               /* the drawable's size changed: tell the game, as Windows would (WM_SIZE) */
        SDL_Window *win = isaac_mac_gl_window();
        int pw = 0, ph = 0;
        if (win) SDL_GetWindowSizeInPixels(win, &pw, &ph);
        int aw = 0, ah = 0;
        if (pw >= 16 && ph >= 16) adapt_game_size(pw, ph, &aw, &ah);
        if (aw >= 16 && (aw != g_told_w || ah != g_told_h)) {
            g_told_w = aw; g_told_h = ah;
            printf("[mac][window] adapt: drawable %dx%d pixels, telling the game its window is %dx%d (frame %u)\n", pw, ph, aw, ah, frame);
            isaac_native_window_resized(aw, ah);
        }
    }
    if (g_f_down_at >= 0 && (int)frame >= g_f_down_at + 4) { g_f_down_at = -1; isaac_input_key(0x46, 0x21, 0, 0); }   /* let go of F */
    if (g_f_pending >= 0) {
        uint8_t *q = opt_fullscreen_byte();
        if (q && *q != (uint8_t)g_f_before) g_f_pending = -1;                      /* the game took it; the poll below applies it */
        else if ((int)frame >= g_f_pending + 12) {
            g_f_pending = -1;
            printf("[mac][window] the game ignored F; toggling the window directly\n");
            fullscreen_set(-1, 0, "the shortcut (direct)");
        }
    }
    if (frame < 20) return;
    uint8_t *p = opt_fullscreen_byte();
    if (!p) return;
    if (g_opt_seen < 0) {
        g_opt_seen = *p;
        printf("[mac][window] Options.Fullscreen is %d at frame %u\n", *p, frame);
        if (*p) fullscreen_set(1, 0, "the game option (options.ini)");
        return;
    }
    if (*p != (uint8_t)g_opt_seen) {
        g_opt_seen = *p;
        fullscreen_set(*p ? 1 : 0, 0, "the game option");
    }
}

/* 1 while the game's debug console takes keys as text */
int isaac_mac_console_open(void) {
    uint32_t game = isaac_r32(GAME_PTR);
    return game && isaac_r32(game + CONSOLE_OFF) != 0;
}

void isaac_mac_window_set_size(int w, int h) {
    SDL_Window *win = isaac_mac_gl_window();
    if (!win || w < 160 || h < 90) return;
    if (g_fs) { printf("[mac][window] resize %dx%d ignored: fullscreen\n", w, h); return; }
    SDL_SetWindowSize(win, w, h);
    printf("[mac][window] window size set to %dx%d points (frame %u)\n", w, h, isaac_frames_presented());
}

/* window events from the present loop: the OS (green button, Ctrl+Cmd+F in the menu) can change fullscreen too */
void isaac_mac_window_event(const SDL_Event *ev) {
    switch (ev->type) {
    case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
    case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN: {
        int on = ev->type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN;
        if (on != g_fs) { g_fs = on; printf("[mac][window] fullscreen %s by the system\n", on ? "on" : "off"); }
        uint8_t *p = opt_fullscreen_byte();
        if (p && *p != (uint8_t)on) *p = (uint8_t)on;
        if (p) g_opt_seen = *p;
        break;
    }
    default: break;
    }
}
