/* mac_script.c -- scripted input for the Mac Build: keyboard, mouse, virtual gamepad.
 *
 * A reusable test driver: keyboard and mouse events go through the Host's own
 * isaac_input_poll_js hook, exactly as the Browser Build's page feeds them; gamepad
 * events drive SDL's virtual joystick (mac_input.c), so the SDL gamepad -> XInput
 * shim path is exercised without hardware.
 *
 *   ISAAC_MAC_SCRIPT="until=Room 1.2(Start Room):enter,@60:d:hold=90,@200:w:hold=90"
 *
 * Comma-separated tokens ('F' is a presented frame, '@F' is F frames after the anchor):
 *   F:key[:hold=N]       press key at frame F (hold N frames, default 8)
 *   until=TEXT:key[:from=N][:every=N][:hold=N]
 *                        tap key from frame N (default 120) every N frames
 *                        (default 90) until a line of the game's own log
 *                        contains TEXT; that frame becomes the anchor for the
 *                        @F tokens and the @F screenshots (ISAAC_MAC_SHOT).
 *                        TEXT has no ',' or ':'.
 *   F:mouse:X:Y          move the pointer to game pixel X,Y (960x540)
 *   F:mouse:left|right[:hold=N]     press that button (default hold 4)
 *   F:mouse:wheel=D      wheel, D notches (positive = up)
 *   F:pad[K]:BTN[:hold=N]           virtual gamepad K (default 0..3): press a button
 *                        a b x y lb rb back start guide ls rs up down left right
 *   F:pad[K]:AXIS=V[:hold=N]        set an axis, back to rest after hold frames
 *                        lx ly rx ry (-32768..32767, SDL sense: ly negative = up)
 *                        lt rt (0..32767)
 *   F:pad[K]:connect | disconnect   plug / unplug the virtual pad (hot-plug test).
 *                        A pad the script uses is attached at frame 1, unless the
 *                        first connect/disconnect token for it is a connect.
 *   until=TEXT:pad-BTN   like until=TEXT:key, tapping virtual pad 0's button
 *   F:pad[K]:rumble      (no-op hook, rumble is the game's call; see "[mac][pad] rumble")
 * ISAAC_MAC_SCRIPT_VIA_SDL=1 (windowed) pushes the keyboard and mouse tokens as SDL events
 * instead, so mac_input.c's live path (scancode table, coordinates) is what is tested.
 * Keys: enter esc space tab ctrl shift alt backspace, a-z, 0-9, f1-f12, up down left
 * right, grave minus equals lbracket rbracket backslash semicolon quote comma period slash.
 *
 * drive_interactive.mjs's sequence: Enter held 120 ms every 1.5 s until the log says
 * "Room 1.2(Start Room)" (a run started), then D held 1.5 s (move right), W held 1.5 s (up).
 */
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t isaac_frames_presented(void);
void isaac_mac_pad_script_wants(void);
int isaac_mac_vpad_connect(int idx);
int isaac_mac_vpad_disconnect(int idx);
void isaac_mac_vpad_button(int idx, int sdl_button, int down);
void isaac_mac_vpad_axis(int idx, int sdl_axis, int value);
void isaac_mac_input_pump_headless(void);

enum { K_KEY, K_MMOVE, K_MBTN, K_MWHEEL, K_PBTN, K_PAXIS, K_PCONN, K_PDISC };
typedef struct { int frame, rel, kind, a, b, c, d, done; } sev;   /* key: a=vk b=sc|ext<<8 c=down */
#define MAX_EV 1024
static sev g_ev[MAX_EV];
static int g_nev;
static int g_parsed;
static int g_anchor = -1;
static int g_vpad_named[4], g_vpad_used[4];

static struct {
    int active;
    char text[256];
    uint32_t vk, sc;
    int ext, from, every, hold, pad_btn;
    int next, up_at, pressing;
} g_until;

int isaac_mac_script_anchor(void) { return g_anchor; }

static int key_lookup(const char *name, uint32_t *vk, uint32_t *sc, int *ext) {
    static const struct { const char *n; uint32_t vk, sc; int ext; } T[] = {
        { "enter", 0x0D, 0x1C, 0 }, { "esc", 0x1B, 0x01, 0 }, { "escape", 0x1B, 0x01, 0 }, { "space", 0x20, 0x39, 0 },
        { "tab", 0x09, 0x0F, 0 }, { "ctrl", 0x11, 0x1D, 0 }, { "shift", 0x10, 0x2A, 0 }, { "alt", 0x12, 0x38, 0 },
        { "backspace", 0x08, 0x0E, 0 },
        { "left", 0x25, 0x4B, 1 }, { "up", 0x26, 0x48, 1 }, { "right", 0x27, 0x4D, 1 }, { "down", 0x28, 0x50, 1 },
        { "grave", 0xC0, 0x29, 0 }, { "minus", 0xBD, 0x0C, 0 }, { "equals", 0xBB, 0x0D, 0 },
        { "lbracket", 0xDB, 0x1A, 0 }, { "rbracket", 0xDD, 0x1B, 0 }, { "backslash", 0xDC, 0x2B, 0 },
        { "semicolon", 0xBA, 0x27, 0 }, { "quote", 0xDE, 0x28, 0 }, { "comma", 0xBC, 0x33, 0 },
        { "period", 0xBE, 0x34, 0 }, { "slash", 0xBF, 0x35, 0 },
        { "f1", 0x70, 0x3B, 0 }, { "f2", 0x71, 0x3C, 0 }, { "f3", 0x72, 0x3D, 0 }, { "f4", 0x73, 0x3E, 0 },
        { "f5", 0x74, 0x3F, 0 }, { "f6", 0x75, 0x40, 0 }, { "f7", 0x76, 0x41, 0 }, { "f8", 0x77, 0x42, 0 },
        { "f9", 0x78, 0x43, 0 }, { "f10", 0x79, 0x44, 0 }, { "f11", 0x7A, 0x57, 0 }, { "f12", 0x7B, 0x58, 0 },
    };
    for (unsigned i = 0; i < sizeof T / sizeof *T; ++i)
        if (!strcmp(name, T[i].n)) { *vk = T[i].vk; *sc = T[i].sc; *ext = T[i].ext; return 1; }
    if (name[0] && !name[1]) {
        static const char row1[] = "qwertyuiop", row2[] = "asdfghjkl", row3[] = "zxcvbnm";
        char c = name[0];
        if (c >= 'a' && c <= 'z') {
            const char *p;
            *vk = (uint32_t)(c - 'a' + 'A'); *ext = 0;
            if ((p = strchr(row1, c))) *sc = 0x10u + (uint32_t)(p - row1);
            else if ((p = strchr(row2, c))) *sc = 0x1Eu + (uint32_t)(p - row2);
            else { p = strchr(row3, c); *sc = 0x2Cu + (uint32_t)(p - row3); }
            return 1;
        }
        if (c >= '0' && c <= '9') { *vk = (uint32_t)c; *sc = c == '0' ? 0x0Bu : 0x02u + (uint32_t)(c - '1'); *ext = 0; return 1; }
    }
    return 0;
}

static int opt(const char *s, const char *key, int def) {      /* "key=N" among the ':' fields of s */
    size_t k = strlen(key);
    for (const char *p = s; p; p = strchr(p, ':')) {
        if (*p == ':') ++p;
        if (!strncmp(p, key, k) && p[k] == '=') return atoi(p + k + 1);
    }
    return def;
}

static void add(int frame, int rel, int kind, int a, int b, int c, int d) {
    if (g_nev >= MAX_EV) return;
    g_ev[g_nev++] = (sev){ frame, rel, kind, a, b, c, d, 0 };
}

static void add_press(int frame, int rel, uint32_t vk, uint32_t sc, int ext, int hold) {
    add(frame, rel, K_KEY, (int)vk, (int)(sc | (ext ? 0x100u : 0u)), 1, 0);
    add(frame + hold, rel, K_KEY, (int)vk, (int)(sc | (ext ? 0x100u : 0u)), 0, 0);
}

static int pad_button(const char *n) {
    static const struct { const char *n; int b; } T[] = {
        { "a", SDL_GAMEPAD_BUTTON_SOUTH }, { "b", SDL_GAMEPAD_BUTTON_EAST }, { "x", SDL_GAMEPAD_BUTTON_WEST },
        { "y", SDL_GAMEPAD_BUTTON_NORTH }, { "lb", SDL_GAMEPAD_BUTTON_LEFT_SHOULDER },
        { "rb", SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER }, { "back", SDL_GAMEPAD_BUTTON_BACK },
        { "start", SDL_GAMEPAD_BUTTON_START }, { "guide", SDL_GAMEPAD_BUTTON_GUIDE },
        { "ls", SDL_GAMEPAD_BUTTON_LEFT_STICK }, { "rs", SDL_GAMEPAD_BUTTON_RIGHT_STICK },
        { "up", SDL_GAMEPAD_BUTTON_DPAD_UP }, { "down", SDL_GAMEPAD_BUTTON_DPAD_DOWN },
        { "left", SDL_GAMEPAD_BUTTON_DPAD_LEFT }, { "right", SDL_GAMEPAD_BUTTON_DPAD_RIGHT },
    };
    for (unsigned i = 0; i < sizeof T / sizeof *T; ++i) if (!strcmp(n, T[i].n)) return T[i].b;
    return -1;
}

static int pad_axis(const char *n) {
    static const struct { const char *n; int a; } T[] = {
        { "lx", SDL_GAMEPAD_AXIS_LEFTX }, { "ly", SDL_GAMEPAD_AXIS_LEFTY }, { "rx", SDL_GAMEPAD_AXIS_RIGHTX },
        { "ry", SDL_GAMEPAD_AXIS_RIGHTY }, { "lt", SDL_GAMEPAD_AXIS_LEFT_TRIGGER }, { "rt", SDL_GAMEPAD_AXIS_RIGHT_TRIGGER },
    };
    for (unsigned i = 0; i < sizeof T / sizeof *T; ++i) if (!strcmp(n, T[i].n)) return T[i].a;
    return -1;
}

static void parse_token(char *tok) {
    uint32_t vk, sc; int ext;
    if (!strncmp(tok, "until=", 6)) {
        char *text = tok + 6, *c = strchr(text, ':');
        if (!c) { fprintf(stderr, "[mac][script] bad token '%s'\n", tok); return; }
        *c = 0;
        char *key = c + 1, *rest = strchr(key, ':');
        if (rest) *rest = 0;
        g_until.pad_btn = -1;
        if (!strncmp(key, "pad-", 4)) {                     /* until=TEXT:pad-a: tap a virtual pad 0 button */
            g_until.pad_btn = pad_button(key + 4);
            if (g_until.pad_btn < 0) { fprintf(stderr, "[mac][script] unknown pad button '%s'\n", key); return; }
            isaac_mac_pad_script_wants();
            g_vpad_used[0] = 1;
            vk = sc = 0; ext = 0;
        } else if (!key_lookup(key, &vk, &sc, &ext)) { fprintf(stderr, "[mac][script] unknown key '%s'\n", key); return; }
        snprintf(g_until.text, sizeof g_until.text, "%s", text);
        g_until.vk = vk; g_until.sc = sc; g_until.ext = ext;
        g_until.from = rest ? opt(rest + 1, "from", 120) : 120;
        g_until.every = rest ? opt(rest + 1, "every", 90) : 90;
        g_until.hold = rest ? opt(rest + 1, "hold", 8) : 8;
        g_until.next = g_until.from;
        g_until.active = 1;
        return;
    }
    int rel = 0;
    char *t = tok;
    if (*t == '@') { rel = 1; ++t; }
    char *c = strchr(t, ':');
    if (!c) { fprintf(stderr, "[mac][script] bad token '%s'\n", tok); return; }
    *c = 0;
    int frame = atoi(t);
    char *what = c + 1, *rest = strchr(what, ':');
    if (rest) *rest++ = 0;                                  /* what = field 2, rest = the remaining fields (or NULL) */
    const char *r = rest ? rest : "";

    if (!strcmp(what, "mouse")) {
        char first[32] = { 0 };
        snprintf(first, sizeof first, "%s", r);
        char *e = strchr(first, ':');
        if (e) *e = 0;
        if (first[0] >= '0' && first[0] <= '9') {
            const char *c2 = strchr(r, ':');
            add(frame, rel, K_MMOVE, atoi(first), c2 ? atoi(c2 + 1) : 0, 0, 0);
        } else if (!strcmp(first, "left") || !strcmp(first, "right")) {
            int b = first[0] == 'r', hold = opt(r, "hold", 4);
            add(frame, rel, K_MBTN, b, 1, 0, 0);
            add(frame + hold, rel, K_MBTN, b, 0, 0, 0);
        } else if (!strncmp(first, "wheel=", 6)) {
            add(frame, rel, K_MWHEEL, atoi(first + 6) * 120, 0, 0, 0);
        } else fprintf(stderr, "[mac][script] bad mouse token '%s'\n", tok);
        return;
    }
    if (!strncmp(what, "pad", 3) && (what[3] == 0 || (what[3] >= '0' && what[3] <= '3' && what[4] == 0))) {
        int k = what[3] ? what[3] - '0' : 0;
        char first[64] = { 0 };
        snprintf(first, sizeof first, "%s", r);
        char *e = strchr(first, ':');
        if (e) *e = 0;
        isaac_mac_pad_script_wants();
        g_vpad_used[k] = 1;
        int hold = opt(r, "hold", 8);
        char *eq = strchr(first, '=');
        if (!strcmp(first, "connect")) { if (!g_vpad_named[k]) g_vpad_named[k] = 1; add(frame, rel, K_PCONN, k, 0, 0, 0); }
        else if (!strcmp(first, "disconnect")) { if (!g_vpad_named[k]) g_vpad_named[k] = 2; add(frame, rel, K_PDISC, k, 0, 0, 0); }
        else if (eq) {
            *eq = 0;
            int ax = pad_axis(first), v = atoi(eq + 1);
            if (ax < 0) { fprintf(stderr, "[mac][script] unknown pad axis '%s'\n", first); return; }
            add(frame, rel, K_PAXIS, k, ax, v, 0);
            add(frame + hold, rel, K_PAXIS, k, ax, 0, 0);
        } else {
            int b = pad_button(first);
            if (b < 0) { fprintf(stderr, "[mac][script] unknown pad button '%s'\n", first); return; }
            add(frame, rel, K_PBTN, k, b, 1, 0);
            add(frame + hold, rel, K_PBTN, k, b, 0, 0);
        }
        return;
    }
    if (!key_lookup(what, &vk, &sc, &ext)) { fprintf(stderr, "[mac][script] unknown key '%s'\n", what); return; }
    add_press(frame, rel, vk, sc, ext, opt(r, "hold", 8));
}

static void parse(void) {
    g_parsed = 1;
    const char *e = getenv("ISAAC_MAC_SCRIPT");
    if (!e || !*e) return;
    char *dup = strdup(e), *save = NULL;
    for (char *tok = strtok_r(dup, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) parse_token(tok);
    for (int k = 0; k < 4; ++k)
        if (g_vpad_used[k] && g_vpad_named[k] != 1) add(1, 0, K_PCONN, k, 0, 0, 0);
    free(dup);
    printf("[mac][script] %d event(s)%s from ISAAC_MAC_SCRIPT\n", g_nev,
           g_until.active ? ", plus a tap-until-log trigger" : "");
}

/* the game's own log, line by line (OutputDebugStringA in host_shims_gl.c) */
void isaac_mac_log_line(const char *line) {
    if (!g_until.active || g_anchor >= 0 || !strstr(line, g_until.text)) return;
    g_anchor = (int)isaac_frames_presented();
    printf("[mac][script] log line \"%s\" seen at presented frame %d: anchor set\n", g_until.text, g_anchor);
}

/* ISAAC_MAC_SCRIPT_VIA_SDL=1 (windowed): keyboard and mouse events go in as SDL events and
 * take mac_input.c's live path (scancode table, window -> game coordinates). */
int isaac_mac_gl_ready(void);
int isaac_mac_input_push_key(uint32_t vk, uint32_t sc, int ext, int down);
int isaac_mac_input_push_mouse_move(int gx, int gy);
int isaac_mac_input_push_mouse_button(int right, int down);
int isaac_mac_input_push_mouse_wheel(int delta);
static int via_sdl(void) {
    static int v = -1;
    if (v < 0) { const char *e = getenv("ISAAC_MAC_SCRIPT_VIA_SDL"); v = e && *e && *e != '0'; }
    return v && isaac_mac_gl_ready();
}

static int emit_key(int32_t *out, uint32_t vk, uint32_t sc, int ext, int down, int frame) {
    if (via_sdl()) {
        int ok = isaac_mac_input_push_key(vk, sc, ext, down);
        printf("[mac][script] frame %d: key 0x%02x %s (SDL event %s)\n", frame, vk, down ? "down" : "up", ok ? "pushed" : "FAILED");
        return 0;
    }
    out[0] = 1; out[1] = (int32_t)vk; out[2] = (int32_t)(sc | (ext ? 0x100u : 0u)); out[3] = down;
    printf("[mac][script] frame %d: key 0x%02x %s\n", frame, vk, down ? "down" : "up");
    return 1;
}

/* One due event: 1 when it filled `out` for the Host, 0 when it acted by itself (pads). */
static int run_event(sev *e, int32_t *out, int frame) {
    switch (e->kind) {
    case K_KEY: return emit_key(out, (uint32_t)e->a, (uint32_t)e->b & 0xFFu, (e->b >> 8) & 1, e->c, frame);
    case K_MMOVE:
        if (via_sdl()) {
            printf("[mac][script] frame %d: mouse move %d,%d (SDL event)\n", frame, e->a, e->b);
            isaac_mac_input_push_mouse_move(e->a, e->b);
            return 0;
        }
        out[0] = 2; out[1] = e->a; out[2] = e->b; out[3] = 0;
        printf("[mac][script] frame %d: mouse move %d,%d\n", frame, e->a, e->b);
        return 1;
    case K_MBTN:
        if (via_sdl()) {
            printf("[mac][script] frame %d: mouse %s %s (SDL event)\n", frame, e->a ? "right" : "left", e->b ? "down" : "up");
            isaac_mac_input_push_mouse_button(e->a, e->b);
            return 0;
        }
        out[0] = 3; out[1] = e->a; out[2] = e->b; out[3] = 0;
        printf("[mac][script] frame %d: mouse %s %s\n", frame, e->a ? "right" : "left", e->b ? "down" : "up");
        return 1;
    case K_MWHEEL:
        if (via_sdl()) {
            printf("[mac][script] frame %d: mouse wheel %d (SDL event)\n", frame, e->a);
            isaac_mac_input_push_mouse_wheel(e->a);
            return 0;
        }
        out[0] = 4; out[1] = e->a; out[2] = 0; out[3] = 0;
        printf("[mac][script] frame %d: mouse wheel %d\n", frame, e->a);
        return 1;
    case K_PCONN:
        printf("[mac][script] frame %d: pad %d connect\n", frame, e->a);
        isaac_mac_vpad_connect(e->a);
        return 0;
    case K_PDISC:
        printf("[mac][script] frame %d: pad %d disconnect\n", frame, e->a);
        isaac_mac_vpad_disconnect(e->a);
        return 0;
    case K_PBTN:
        printf("[mac][script] frame %d: pad %d button %d %s\n", frame, e->a, e->b, e->c ? "down" : "up");
        isaac_mac_vpad_button(e->a, e->b, e->c);
        return 0;
    case K_PAXIS:
        printf("[mac][script] frame %d: pad %d axis %d = %d\n", frame, e->a, e->b, e->c);
        isaac_mac_vpad_axis(e->a, e->b, e->c);
        return 0;
    }
    return 0;
}

/* Overrides the Host's weak hook; called until it returns 0, once per input pass. */
int isaac_input_poll_js(int frame, int32_t *out) {
    static int last_frame = -1;
    if (!g_parsed) parse();
    if (frame != last_frame) { last_frame = frame; isaac_mac_input_pump_headless(); }
    if (g_until.active) {
        int press = -1;
        if (g_until.pressing && (g_anchor >= 0 || frame >= g_until.up_at)) { g_until.pressing = 0; press = 0; }
        else if (g_anchor < 0 && !g_until.pressing && frame >= g_until.next) {
            g_until.pressing = 1;
            g_until.up_at = frame + g_until.hold;
            g_until.next = frame + g_until.every;
            press = 1;
        }
        if (press >= 0) {
            if (g_until.pad_btn < 0) return emit_key(out, g_until.vk, g_until.sc, g_until.ext, press, frame);
            printf("[mac][script] frame %d: pad 0 button %d %s (until)\n", frame, g_until.pad_btn, press ? "down" : "up");
            isaac_mac_vpad_button(0, g_until.pad_btn, press);
        }
    }
    for (int i = 0; i < g_nev; ++i) {
        sev *e = &g_ev[i];
        if (e->done) continue;
        int due = e->rel ? (g_anchor >= 0 && frame >= g_anchor + e->frame) : frame >= e->frame;
        if (!due) continue;
        e->done = 1;
        if (run_event(e, out, frame)) return 1;
    }
    return 0;
}
