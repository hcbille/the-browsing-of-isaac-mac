/* mac_sys.c -- the Mac Build's answers to the REPENTOGON Isaac.* functions that touch the operating system:
 * window title, window icon, clipboard, error dialog. Called by the layer's bridge (host_rgon.c, ISAAC_NATIVE only).
 *
 * Headless (no window): the title and icon calls do nothing and say so (return 0); the clipboard goes through
 * pbcopy / pbpaste; the dialog is not shown (a test hook, ISAAC_MAC_NODIALOG=1, does the same in a windowed run and
 * answers the first button). */
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SDL_Window *isaac_mac_gl_window(void);
void isaac_log(const char *fmt, ...);

/* ---- title -------------------------------------------------------------------------- */
int isaac_mac_sys_set_title(const char *title) {
    SDL_Window *w = isaac_mac_gl_window();
    if (!w) return 0;
    return SDL_SetWindowTitle(w, title) ? 1 : 0;
}
const char *isaac_mac_sys_get_title(void) {
    SDL_Window *w = isaac_mac_gl_window();
    return w ? SDL_GetWindowTitle(w) : NULL;
}

/* ---- cursor ------------------------------------------------------------------------- */
/* Isaac.CenterCursor: warp the mouse to the middle of the window's client area. Like REPENTOGON's, it does nothing when the
 * game is not the focused window. Returns 1 when the cursor was moved, 0 headless or unfocused. */
int isaac_mac_sys_center_cursor(void) {
    SDL_Window *w = isaac_mac_gl_window();
    if (!w || !SDL_WasInit(SDL_INIT_VIDEO) || SDL_GetKeyboardFocus() != w) return 0;
    int ww = 0, wh = 0;
    if (!SDL_GetWindowSize(w, &ww, &wh) || ww < 1 || wh < 1) return 0;
    SDL_WarpMouseInWindow(w, (float)ww / 2.0f, (float)wh / 2.0f);
    return 1;
}

/* ---- clipboard ---------------------------------------------------------------------- */
static int video_up(void) { return isaac_mac_gl_window() != NULL && SDL_WasInit(SDL_INIT_VIDEO); }

int isaac_mac_sys_clip_set(const char *text, size_t n) {
    if (video_up()) return SDL_SetClipboardText(text) ? 1 : 0;
    FILE *p = popen("pbcopy", "w");
    if (!p) return 0;
    size_t w = fwrite(text, 1, n, p);
    return pclose(p) == 0 && w == n;
}
/* the text (malloc'd, caller frees) or NULL */
char *isaac_mac_sys_clip_get(void) {
    if (video_up()) {
        char *t = SDL_GetClipboardText();
        if (!t) return NULL;
        char *r = strdup(t);
        SDL_free(t);
        return r;
    }
    FILE *p = popen("pbpaste", "r");
    if (!p) return NULL;
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) { pclose(p); return NULL; }
    size_t got;
    while ((got = fread(buf + len, 1, cap - len - 1, p)) > 0) {
        len += got;
        if (len + 1 >= cap) { cap *= 2; char *nb = realloc(buf, cap); if (!nb) { free(buf); pclose(p); return NULL; } buf = nb; }
    }
    pclose(p);
    buf[len] = 0;
    return buf;
}

/* ---- error dialog ------------------------------------------------------------------- */
/* Windows MessageBox values: icon 0x10 error, 0x20 question, 0x30 warning, 0x40 information; buttons 0 OK, 1 OK/Cancel,
 * 2 Abort/Retry/Ignore, 3 Yes/No/Cancel, 4 Yes/No, 5 Retry/Cancel. Answers: IDOK 1, IDCANCEL 2, IDABORT 3, IDRETRY 4,
 * IDIGNORE 5, IDYES 6, IDNO 7. Returns 0 when no dialog could be shown. */
int isaac_mac_sys_dialog(const char *title, const char *text, int icon, int buttons) {
    static const struct { const char *label; int id; } SETS[6][3] = {
        { {"OK", 1} }, { {"OK", 1}, {"Cancel", 2} }, { {"Abort", 3}, {"Retry", 4}, {"Ignore", 5} },
        { {"Yes", 6}, {"No", 7}, {"Cancel", 2} }, { {"Yes", 6}, {"No", 7} }, { {"Retry", 4}, {"Cancel", 2} },
    };
    static const int COUNT[6] = { 1, 2, 3, 3, 2, 2 };
    const int set = (buttons & 0xF) <= 5 ? (buttons & 0xF) : 0;
    const char *no = getenv("ISAAC_MAC_NODIALOG");
    if (no && *no == '1') {
        isaac_log("[mac][dialog] not shown (ISAAC_MAC_NODIALOG): \"%s\" / \"%s\" icon 0x%x buttons %d", title, text, icon & 0xF0, set);
        return SETS[set][0].id;
    }
    if (!isaac_mac_gl_window()) {
        isaac_log("[mac][dialog] headless, not shown: \"%s\" / \"%s\"", title, text);
        return 0;
    }
    SDL_MessageBoxButtonData b[3];
    for (int i = 0; i < COUNT[set]; ++i) {
        SDL_zero(b[i]);
        b[i].buttonID = SETS[set][i].id;
        b[i].text = SETS[set][i].label;
        if (i == 0) b[i].flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
        if (SETS[set][i].id == 2 || (set == 0 && i == 0)) b[i].flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
    }
    SDL_MessageBoxData d;
    SDL_zero(d);
    const int ic = icon & 0xF0;
    d.flags = ic == 0x10 ? SDL_MESSAGEBOX_ERROR : (ic == 0x30 ? SDL_MESSAGEBOX_WARNING : SDL_MESSAGEBOX_INFORMATION);
    d.window = isaac_mac_gl_window();
    d.title = title;
    d.message = text;
    d.numbuttons = COUNT[set];
    d.buttons = b;
    int hit = 0;
    if (!SDL_ShowMessageBox(&d, &hit)) return 0;
    return hit;
}

/* ---- window icon -------------------------------------------------------------------- */
/* rgba: w*h*4 straight-alpha bytes. The window icon on macOS is the app's Dock icon. */
int isaac_mac_sys_set_icon(const uint8_t *rgba, int w, int h) {
    SDL_Window *win = isaac_mac_gl_window();
    if (!win || w < 1 || h < 1) return 0;
    SDL_Surface *s = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, (void *)rgba, w * 4);
    if (!s) return 0;
    int ok = SDL_SetWindowIcon(win, s) ? 1 : 0;
    SDL_DestroySurface(s);       /* SDL copies the pixels */
    return ok;
}
