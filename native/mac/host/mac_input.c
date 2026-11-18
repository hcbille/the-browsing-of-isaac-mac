/* mac_input.c -- live keyboard, mouse and gamepad input for the Mac Build (M3).
 *
 * SDL events become the same calls the Browser Build's page makes:
 *   keyboard  isaac_input_key(vk, scancode, extended, down), the Windows virtual-key
 *             and set-1 scancode of the key (the table below holds what
 *             boot_web.mjs's KEYS holds, plus the rest of a PC keyboard); key repeat
 *             is dropped (the Browser Build drops it too)
 *   mouse     isaac_input_mouse_move / _button (left 0, right 1; other buttons are
 *             ignored) / _wheel; window coordinates become game coordinates through
 *             isaac_mac_window_to_game(), the one place M5's letterboxing plugs into
 *   pads      SDL3 gamepads (up to 4, slot = connection order, slots stay put when
 *             another pad leaves) answer the XInput shim (host_shims_xinput.c):
 *             isaac_mac_pad_state() / isaac_mac_pad_rumble()
 *
 * Scripted runs reach the same code: mac_script.c drives SDL's virtual gamepad
 * (isaac_mac_vpad_*) so the whole SDL -> XInput path is exercised without hardware.
 *
 * Environment:
 *   ISAAC_MAC_PADS=0|1      0: no pads at all; 1: pads even when headless
 *                           (default: on when windowed, or when a script uses a pad)
 *   ISAAC_MAC_PAD_LOG=0     silence the "[mac][pad] slot N state" lines (they log
 *                           changes of buttons/triggers and large stick moves, at
 *                           most 400 lines)
 */
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t isaac_frames_presented(void);
void isaac_input_key(uint32_t vk, uint32_t scancode, int extended, int down);
void isaac_input_mouse_move(int32_t x, int32_t y);
void isaac_input_mouse_button(int button, int down);
void isaac_input_mouse_wheel(int32_t delta);
int isaac_mac_gl_ready(void);

#define GAME_W 960
#define GAME_H 540

/* ---- window pixels -> game pixels ---------------------------------------------
 * isaac_mac_window_to_game() lives in mac_window.c: it maps through the same rectangle the
 * picture is drawn in (whole-number scale or fill, centred, bars). It returns 0 when the
 * point lies in the bars; the coordinates are then clamped to the nearest game pixel. */
int isaac_mac_window_to_game(SDL_Window *win, float wx, float wy, int *gx, int *gy);
void isaac_mac_game_to_window(int gx, int gy, float *wx, float *wy);
int isaac_mac_window_toggle_fullscreen(const char *why);

/* ---- keyboard -------------------------------------------------------------------- */
typedef struct { uint8_t vk, sc, ext; } vkey;
static vkey g_keymap[SDL_SCANCODE_COUNT];
static uint8_t g_keyheld[SDL_SCANCODE_COUNT];
static int g_keymap_ready;

static void km(int sdl, uint8_t vk, uint8_t sc, uint8_t ext) {
    if (sdl > 0 && sdl < SDL_SCANCODE_COUNT) g_keymap[sdl] = (vkey){ vk, sc, ext };
}

static void keymap_init(void) {
    g_keymap_ready = 1;
    static const uint8_t letter_sc[26] = { 0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32,
                                           0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C };
    for (int i = 0; i < 26; ++i) km(SDL_SCANCODE_A + i, (uint8_t)('A' + i), letter_sc[i], 0);
    for (int i = 0; i < 9; ++i) km(SDL_SCANCODE_1 + i, (uint8_t)('1' + i), (uint8_t)(0x02 + i), 0);
    km(SDL_SCANCODE_0, '0', 0x0B, 0);
    km(SDL_SCANCODE_RETURN, 0x0D, 0x1C, 0); km(SDL_SCANCODE_ESCAPE, 0x1B, 0x01, 0);
    km(SDL_SCANCODE_BACKSPACE, 0x08, 0x0E, 0); km(SDL_SCANCODE_TAB, 0x09, 0x0F, 0);
    km(SDL_SCANCODE_SPACE, 0x20, 0x39, 0); km(SDL_SCANCODE_CAPSLOCK, 0x14, 0x3A, 0);
    km(SDL_SCANCODE_MINUS, 0xBD, 0x0C, 0); km(SDL_SCANCODE_EQUALS, 0xBB, 0x0D, 0);
    km(SDL_SCANCODE_LEFTBRACKET, 0xDB, 0x1A, 0); km(SDL_SCANCODE_RIGHTBRACKET, 0xDD, 0x1B, 0);
    km(SDL_SCANCODE_BACKSLASH, 0xDC, 0x2B, 0); km(SDL_SCANCODE_SEMICOLON, 0xBA, 0x27, 0);
    km(SDL_SCANCODE_APOSTROPHE, 0xDE, 0x28, 0); km(SDL_SCANCODE_GRAVE, 0xC0, 0x29, 0);
    km(SDL_SCANCODE_COMMA, 0xBC, 0x33, 0); km(SDL_SCANCODE_PERIOD, 0xBE, 0x34, 0);
    km(SDL_SCANCODE_SLASH, 0xBF, 0x35, 0);
    for (int i = 0; i < 10; ++i) km(SDL_SCANCODE_F1 + i, (uint8_t)(0x70 + i), (uint8_t)(0x3B + i), 0);
    km(SDL_SCANCODE_F11, 0x7A, 0x57, 0); km(SDL_SCANCODE_F12, 0x7B, 0x58, 0);
    km(SDL_SCANCODE_UP, 0x26, 0x48, 1); km(SDL_SCANCODE_DOWN, 0x28, 0x50, 1);
    km(SDL_SCANCODE_LEFT, 0x25, 0x4B, 1); km(SDL_SCANCODE_RIGHT, 0x27, 0x4D, 1);
    km(SDL_SCANCODE_INSERT, 0x2D, 0x52, 1); km(SDL_SCANCODE_DELETE, 0x2E, 0x53, 1);
    km(SDL_SCANCODE_HOME, 0x24, 0x47, 1); km(SDL_SCANCODE_END, 0x23, 0x4F, 1);
    km(SDL_SCANCODE_PAGEUP, 0x21, 0x49, 1); km(SDL_SCANCODE_PAGEDOWN, 0x22, 0x51, 1);
    /* modifiers: the generic VK, as the Browser Build sends; the scancode/extended bit say which side */
    km(SDL_SCANCODE_LSHIFT, 0x10, 0x2A, 0); km(SDL_SCANCODE_RSHIFT, 0x10, 0x36, 0);
    km(SDL_SCANCODE_LCTRL, 0x11, 0x1D, 0); km(SDL_SCANCODE_RCTRL, 0x11, 0x1D, 1);
    km(SDL_SCANCODE_LALT, 0x12, 0x38, 0); km(SDL_SCANCODE_RALT, 0x12, 0x38, 1);
    /* keypad */
    static const uint8_t kp_sc[10] = { 0x52, 0x4F, 0x50, 0x51, 0x4B, 0x4C, 0x4D, 0x47, 0x48, 0x49 };
    km(SDL_SCANCODE_KP_0, 0x60, kp_sc[0], 0);
    for (int i = 1; i < 10; ++i) km(SDL_SCANCODE_KP_1 + i - 1, (uint8_t)(0x60 + i), kp_sc[i], 0);
    km(SDL_SCANCODE_KP_ENTER, 0x0D, 0x1C, 1); km(SDL_SCANCODE_KP_PLUS, 0x6B, 0x4E, 0);
    km(SDL_SCANCODE_KP_MINUS, 0x6D, 0x4A, 0); km(SDL_SCANCODE_KP_MULTIPLY, 0x6A, 0x37, 0);
    km(SDL_SCANCODE_KP_DIVIDE, 0x6F, 0x35, 1); km(SDL_SCANCODE_KP_PERIOD, 0x6E, 0x53, 0);
}

static void key_event(SDL_Scancode sc, int down, int repeat, SDL_Keymod mod) {
    if (!g_keymap_ready) keymap_init();
    if ((int)sc <= 0 || (int)sc >= SDL_SCANCODE_COUNT || !g_keymap[sc].vk) return;
    if (down) {
        if (repeat || g_keyheld[sc]) return;                  /* the game wants edges only */
        if (sc == SDL_SCANCODE_F) {                           /* M5: fullscreen shortcuts (mac_window.c) */
            if ((mod & SDL_KMOD_GUI) && (mod & SDL_KMOD_CTRL)) {   /* Cmd+Ctrl+F, the macOS fullscreen key */
                isaac_mac_window_toggle_fullscreen("Cmd+Ctrl+F");
                return;
            }
            /* plain F is the game's own fullscreen key: it is forwarded below and mac_window.c follows
             * Options.Fullscreen */
        }
        if (mod & SDL_KMOD_GUI) return;                       /* other Cmd+key belongs to the OS */
        g_keyheld[sc] = 1;
    } else {
        if (!g_keyheld[sc]) return;
        g_keyheld[sc] = 0;
    }
    isaac_input_key(g_keymap[sc].vk, g_keymap[sc].sc, g_keymap[sc].ext, down);
}

/* ---- mouse ----------------------------------------------------------------------- */
static int g_mx = -1, g_my = -1;          /* last game position sent */
static int g_pend, g_px, g_py;            /* a coalesced move waiting for the end of the event batch */
static uint8_t g_mbtn[2];

static void mouse_flush(void) {
    if (!g_pend) return;
    g_pend = 0;
    if (g_px != g_mx || g_py != g_my) { g_mx = g_px; g_my = g_py; isaac_input_mouse_move(g_mx, g_my); }
}

/* Returns 1 when the pointer is inside the picture; in the bars the position is clamped to the
 * nearest game pixel (a move there still tracks the edge) and 0 is returned. */
static int mouse_pos(SDL_WindowID wid, float x, float y) {
    int gx, gy;
    int inside = isaac_mac_window_to_game(SDL_GetWindowFromID(wid), x, y, &gx, &gy);
    g_px = gx; g_py = gy; g_pend = 1;
    static int logged;
    if (getenv("ISAAC_MAC_MOUSE_LOG") && logged < 400) {      /* every mapped event: window point -> game pixel */
        ++logged;
        printf("[mac][mouse] window %.1f,%.1f -> game %d,%d %s\n", x, y, gx, gy, inside ? "inside" : "in the bars (clamped)");
    }
    return inside;
}

static int g_btn_logged;
static void mouse_button(int sdl_button, int down, int inside, float wx, float wy) {
    int b = sdl_button == SDL_BUTTON_LEFT ? 0 : sdl_button == SDL_BUTTON_RIGHT ? 1 : -1;
    if (b < 0) return;
    if (down && !inside) {                          /* a press in the bars does nothing */
        printf("[mac][mouse] %s press at window %.1f,%.1f is in the bars: ignored\n", b ? "right" : "left", wx, wy);
        return;
    }
    mouse_flush();
    if (g_btn_logged < 100) {
        ++g_btn_logged;
        printf("[mac][mouse] %s %s at window %.1f,%.1f -> game %d,%d\n", b ? "right" : "left", down ? "down" : "up", wx, wy, g_px, g_py);
    }
    if (g_mbtn[b] == (uint8_t)down) return;
    g_mbtn[b] = (uint8_t)down;
    isaac_input_mouse_button(b, down);
}

/* focus lost: nothing stays held (the Browser Build does the same on blur) */
static void release_all(void) {
    for (int i = 1; i < SDL_SCANCODE_COUNT; ++i) if (g_keyheld[i]) key_event((SDL_Scancode)i, 0, 0, SDL_KMOD_NONE);
    for (int b = 0; b < 2; ++b) if (g_mbtn[b]) { g_mbtn[b] = 0; isaac_input_mouse_button(b, 0); }
}

/* One SDL event from the present loop (mac_gl.c). */
void isaac_mac_input_event(const SDL_Event *ev) {
    switch (ev->type) {
    case SDL_EVENT_KEY_DOWN: key_event(ev->key.scancode, 1, ev->key.repeat, ev->key.mod); break;
    case SDL_EVENT_KEY_UP: key_event(ev->key.scancode, 0, 0, SDL_KMOD_NONE); break;
    case SDL_EVENT_MOUSE_MOTION: mouse_pos(ev->motion.windowID, ev->motion.x, ev->motion.y); break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        int in = mouse_pos(ev->button.windowID, ev->button.x, ev->button.y);
        mouse_button(ev->button.button, 1, in, ev->button.x, ev->button.y);
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        int in = mouse_pos(ev->button.windowID, ev->button.x, ev->button.y);
        mouse_button(ev->button.button, 0, in, ev->button.x, ev->button.y);
        break;
    }
    case SDL_EVENT_MOUSE_WHEEL: {
        mouse_flush();
        float dy = ev->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -ev->wheel.y : ev->wheel.y;
        if (dy != 0.0f) isaac_input_mouse_wheel((int32_t)(dy * 120.0f));
        break;
    }
    case SDL_EVENT_WINDOW_FOCUS_LOST: release_all(); break;
    default: break;
    }
}

/* after the batch: one move per frame instead of one per SDL motion event */
void isaac_mac_input_end_events(void) { mouse_flush(); }

/* ---- scripted input through the live path (ISAAC_MAC_SCRIPT_VIA_SDL=1, windowed) ------------
 * The script's keys and mouse events are pushed as SDL events instead of going to the Host's
 * poll hook, so the scancode table and the window -> game conversion above are what the game
 * sees. Each returns 1 when pushed. */
static SDL_Window *first_window(void) {
    int n = 0;
    SDL_Window **w = SDL_GetWindows(&n);
    SDL_Window *r = (w && n > 0) ? w[0] : NULL;
    SDL_free(w);
    return r;
}

/* mod: SDL_Keymod bits the key event carries (a pushed event does not change SDL's own modifier state) */
int isaac_mac_input_push_key_mod(uint32_t vk, uint32_t sc, int ext, int down, unsigned mod) {
    if (!g_keymap_ready) keymap_init();
    for (int s = 1; s < SDL_SCANCODE_COUNT; ++s) {
        if (g_keymap[s].vk != vk || g_keymap[s].sc != (sc & 0xFF) || g_keymap[s].ext != (ext != 0)) continue;
        SDL_Event e;
        SDL_zero(e);
        e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        e.key.scancode = (SDL_Scancode)s;
        e.key.down = down != 0;
        e.key.mod = (SDL_Keymod)mod;
        return SDL_PushEvent(&e) ? 1 : 0;
    }
    return 0;
}

int isaac_mac_input_push_key(uint32_t vk, uint32_t sc, int ext, int down) {
    return isaac_mac_input_push_key_mod(vk, sc, ext, down, 0);
}

static float g_push_wx, g_push_wy;      /* last scripted pointer position, window points */

static void push_pos(float *wx, float *wy) { *wx = g_push_wx; *wy = g_push_wy; }

/* a point in the window itself (points), bars included */
int isaac_mac_input_push_mouse_window(float x, float y) {
    SDL_Window *w = first_window();
    if (!w) return 0;
    g_push_wx = x; g_push_wy = y;
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_EVENT_MOUSE_MOTION;
    e.motion.windowID = SDL_GetWindowID(w);
    push_pos(&e.motion.x, &e.motion.y);
    return SDL_PushEvent(&e) ? 1 : 0;
}

/* the middle of game pixel gx,gy wherever the picture is in the window now */
int isaac_mac_input_push_mouse_move(int gx, int gy) {
    SDL_Window *w = first_window();
    if (!w) return 0;
    isaac_mac_game_to_window(gx, gy, &g_push_wx, &g_push_wy);
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_EVENT_MOUSE_MOTION;
    e.motion.windowID = SDL_GetWindowID(w);
    push_pos(&e.motion.x, &e.motion.y);
    return SDL_PushEvent(&e) ? 1 : 0;
}

int isaac_mac_input_push_mouse_button(int right, int down) {
    SDL_Window *w = first_window();
    if (!w) return 0;
    SDL_Event e;
    SDL_zero(e);
    e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    e.button.windowID = SDL_GetWindowID(w);
    e.button.button = right ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT;
    e.button.down = down != 0;
    push_pos(&e.button.x, &e.button.y);
    return SDL_PushEvent(&e) ? 1 : 0;
}

int isaac_mac_input_push_mouse_wheel(int delta) {
    SDL_Window *w = first_window();
    if (!w) return 0;
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_EVENT_MOUSE_WHEEL;
    e.wheel.windowID = SDL_GetWindowID(w);
    e.wheel.y = (float)delta / 120.0f;
    return SDL_PushEvent(&e) ? 1 : 0;
}

/* ---- gamepads ---------------------------------------------------------------------- */
#define NPADS 4
static struct {
    SDL_Gamepad *pad;
    SDL_JoystickID id;
    uint32_t packet;
    uint8_t last[16];
    int logged_lx, logged_ly, logged_rx, logged_ry;
} g_slot[NPADS];
static int g_pad_init;        /* 0 untried, 1 ready, -1 off */
static int g_pad_wanted;      /* a script uses a pad: also when headless */
static uint32_t g_pad_sync_frame = 0xFFFFFFFFu;
static int g_pad_log_lines;

void isaac_mac_pad_script_wants(void) { g_pad_wanted = 1; }

static int pad_log_on(void) {
    static int on = -1;
    if (on < 0) { const char *e = getenv("ISAAC_MAC_PAD_LOG"); on = !(e && *e == '0'); }
    return on && g_pad_log_lines < 400;
}

static int pad_ready(void) {
    if (g_pad_init) return g_pad_init > 0;
    const char *e = getenv("ISAAC_MAC_PADS");
    int force = e && *e == '1', off = e && *e == '0';
    int headless = !isaac_mac_gl_ready();
    if (off || (headless && !force && !g_pad_wanted)) { g_pad_init = -1; return 0; }
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "[mac][pad] SDL_INIT_GAMEPAD failed: %s\n", SDL_GetError());
        g_pad_init = -1;
        return 0;
    }
    g_pad_init = 1;
    printf("[mac][pad] gamepad subsystem ready\n");
    return 1;
}

static int slot_of(SDL_JoystickID id) {
    for (int i = 0; i < NPADS; ++i) if (g_slot[i].pad && g_slot[i].id == id) return i;
    return -1;
}

/* Reconcile the slots with the pads SDL has: a new pad takes the lowest free slot,
 * a vanished one frees its slot (XInput's user indexes stay put). */
static void pad_sync(void) {
    int n = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&n);
    for (int s = 0; s < NPADS; ++s) {
        if (!g_slot[s].pad) continue;
        int present = 0;
        for (int i = 0; i < n; ++i) if (ids && ids[i] == g_slot[s].id) present = 1;
        if (!present) {
            printf("[mac][pad] slot %d disconnected (%s)\n", s, SDL_GetGamepadName(g_slot[s].pad));
            SDL_RumbleGamepad(g_slot[s].pad, 0, 0, 0);
            SDL_CloseGamepad(g_slot[s].pad);
            memset(&g_slot[s], 0, sizeof g_slot[s]);
        }
    }
    for (int i = 0; i < n && ids; ++i) {
        if (slot_of(ids[i]) >= 0) continue;
        int free_slot = -1;
        for (int s = 0; s < NPADS; ++s) if (!g_slot[s].pad) { free_slot = s; break; }
        if (free_slot < 0) break;
        SDL_Gamepad *p = SDL_OpenGamepad(ids[i]);
        if (!p) { fprintf(stderr, "[mac][pad] cannot open gamepad %u: %s\n", (unsigned)ids[i], SDL_GetError()); continue; }
        memset(&g_slot[free_slot], 0, sizeof g_slot[free_slot]);
        g_slot[free_slot].pad = p;
        g_slot[free_slot].id = ids[i];
        printf("[mac][pad] slot %d connected: %s\n", free_slot, SDL_GetGamepadName(p));
    }
    SDL_free(ids);
}

static void pad_frame(void) {
    uint32_t f = isaac_frames_presented();
    if (f == g_pad_sync_frame) return;
    g_pad_sync_frame = f;
    SDL_UpdateGamepads();
    pad_sync();
}

/* For headless runs nobody pumps SDL's event queue: keep it empty (called per frame by mac_script.c). */
void isaac_mac_input_pump_headless(void) {
    if (g_pad_init <= 0 || isaac_mac_gl_ready()) return;
    SDL_UpdateGamepads();
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
}

static int16_t clamp16(int v) { return (int16_t)(v < -32768 ? -32768 : v > 32767 ? 32767 : v); }

/* XINPUT_STATE (16 bytes) for the pad in `slot`; 0 when none is connected. */
int isaac_mac_pad_state(int slot, uint8_t *out) {
    if (slot < 0 || slot >= NPADS || !pad_ready()) return 0;
    pad_frame();
    SDL_Gamepad *p = g_slot[slot].pad;
    if (!p) return 0;
    static const struct { SDL_GamepadButton b; uint16_t bit; } M[] = {
        { SDL_GAMEPAD_BUTTON_DPAD_UP, 0x0001 }, { SDL_GAMEPAD_BUTTON_DPAD_DOWN, 0x0002 },
        { SDL_GAMEPAD_BUTTON_DPAD_LEFT, 0x0004 }, { SDL_GAMEPAD_BUTTON_DPAD_RIGHT, 0x0008 },
        { SDL_GAMEPAD_BUTTON_START, 0x0010 }, { SDL_GAMEPAD_BUTTON_BACK, 0x0020 },
        { SDL_GAMEPAD_BUTTON_LEFT_STICK, 0x0040 }, { SDL_GAMEPAD_BUTTON_RIGHT_STICK, 0x0080 },
        { SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, 0x0100 }, { SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, 0x0200 },
        { SDL_GAMEPAD_BUTTON_GUIDE, 0x0400 },
        { SDL_GAMEPAD_BUTTON_SOUTH, 0x1000 }, { SDL_GAMEPAD_BUTTON_EAST, 0x2000 },
        { SDL_GAMEPAD_BUTTON_WEST, 0x4000 }, { SDL_GAMEPAD_BUTTON_NORTH, 0x8000 },
    };
    uint16_t buttons = 0;
    for (unsigned i = 0; i < sizeof M / sizeof *M; ++i) if (SDL_GetGamepadButton(p, M[i].b)) buttons |= M[i].bit;
    int lt = SDL_GetGamepadAxis(p, SDL_GAMEPAD_AXIS_LEFT_TRIGGER), rt = SDL_GetGamepadAxis(p, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    uint8_t ltb = (uint8_t)(((lt < 0 ? 0 : lt) * 255 + 16383) / 32767), rtb = (uint8_t)(((rt < 0 ? 0 : rt) * 255 + 16383) / 32767);
    int lx = SDL_GetGamepadAxis(p, SDL_GAMEPAD_AXIS_LEFTX), ly = -SDL_GetGamepadAxis(p, SDL_GAMEPAD_AXIS_LEFTY);
    int rx = SDL_GetGamepadAxis(p, SDL_GAMEPAD_AXIS_RIGHTX), ry = -SDL_GetGamepadAxis(p, SDL_GAMEPAD_AXIS_RIGHTY);
    /* SDL: stick down is positive; XInput: up is positive */
    uint8_t st[16] = { 0 };
    st[4] = (uint8_t)buttons; st[5] = (uint8_t)(buttons >> 8);
    st[6] = ltb; st[7] = rtb;
    int16_t v[4] = { clamp16(lx), clamp16(ly), clamp16(rx), clamp16(ry) };
    for (int i = 0; i < 4; ++i) { st[8 + 2 * i] = (uint8_t)v[i]; st[9 + 2 * i] = (uint8_t)((uint16_t)v[i] >> 8); }
    if (memcmp(st + 4, g_slot[slot].last + 4, 12) != 0) {
        ++g_slot[slot].packet;
        if (pad_log_on()) {
            int big = abs(v[0] - g_slot[slot].logged_lx) > 4096 || abs(v[1] - g_slot[slot].logged_ly) > 4096 ||
                      abs(v[2] - g_slot[slot].logged_rx) > 4096 || abs(v[3] - g_slot[slot].logged_ry) > 4096;
            int digital = memcmp(st + 4, g_slot[slot].last + 4, 4) != 0;
            if (big || digital) {
                g_slot[slot].logged_lx = v[0]; g_slot[slot].logged_ly = v[1];
                g_slot[slot].logged_rx = v[2]; g_slot[slot].logged_ry = v[3];
                ++g_pad_log_lines;
                printf("[mac][pad] slot %d state frame %u: packet %u buttons 0x%04x lt %u rt %u lx %d ly %d rx %d ry %d\n",
                       slot, isaac_frames_presented(), g_slot[slot].packet, buttons, ltb, rtb, v[0], v[1], v[2], v[3]);
            }
        }
        memcpy(g_slot[slot].last, st, 16);
    }
    st[0] = (uint8_t)g_slot[slot].packet; st[1] = (uint8_t)(g_slot[slot].packet >> 8);
    st[2] = (uint8_t)(g_slot[slot].packet >> 16); st[3] = (uint8_t)(g_slot[slot].packet >> 24);
    memcpy(out, st, 16);
    return 1;
}

/* XInputSetState's motors (16-bit words; left = low frequency, right = high). The game calls this
 * only while its RUMBLE option is on. */
void isaac_mac_pad_rumble(int slot, int left, int right) {
    if (slot < 0 || slot >= NPADS || !g_slot[slot].pad) return;
    static int last_lo[NPADS] = { -1, -1, -1, -1 }, last_hi[NPADS] = { -1, -1, -1, -1 };
    const unsigned ms = 1000;       /* XInput holds a setting until the next call; the game re-sends while it matters */
    int lo = left & 0xFFFF, hi = right & 0xFFFF;
    if (lo != last_lo[slot] || hi != last_hi[slot]) {
        last_lo[slot] = lo; last_hi[slot] = hi;
        printf("[mac][pad] rumble slot %d frame %u lo=%d hi=%d ms=%u\n", slot, isaac_frames_presented(), lo, hi, lo || hi ? ms : 0u);
    }
    SDL_RumbleGamepad(g_slot[slot].pad, (Uint16)lo, (Uint16)hi, lo || hi ? ms : 0u);
}

/* ---- SDL's virtual gamepad, for scripted runs (mac_script.c) ---------------------------- */
static struct { SDL_JoystickID id; SDL_Joystick *joy; } g_vpad[NPADS];

static bool SDLCALL vpad_rumble(void *userdata, Uint16 lo, Uint16 hi) {
    printf("[mac][pad] virtual device %d received rumble lo=%u hi=%u\n", (int)(intptr_t)userdata, lo, hi);
    return true;
}

static int vpad_trigger_joy(int v) {       /* gamepad trigger 0..32767 -> the joystick axis (-32768..32767) */
    if (v < 0) v = 0;
    if (v > 32767) v = 32767;
    return v * 2 - 32768;
}

int isaac_mac_vpad_connect(int idx) {
    if (idx < 0 || idx >= NPADS || g_vpad[idx].joy) return 0;
    g_pad_wanted = 1;
    if (!pad_ready()) return 0;
    SDL_VirtualJoystickDesc d;
    SDL_INIT_INTERFACE(&d);
    d.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    d.naxes = SDL_GAMEPAD_AXIS_COUNT;
    d.nbuttons = SDL_GAMEPAD_BUTTON_DPAD_RIGHT + 1;
    d.name = "Isaac Mac virtual pad";
    d.userdata = (void *)(intptr_t)idx;
    d.Rumble = vpad_rumble;
    SDL_JoystickID id = SDL_AttachVirtualJoystick(&d);
    if (!id) { fprintf(stderr, "[mac][pad] SDL_AttachVirtualJoystick: %s\n", SDL_GetError()); return 0; }
    SDL_Joystick *j = SDL_OpenJoystick(id);
    if (!j) { fprintf(stderr, "[mac][pad] SDL_OpenJoystick: %s\n", SDL_GetError()); SDL_DetachVirtualJoystick(id); return 0; }
    g_vpad[idx].id = id;
    g_vpad[idx].joy = j;
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, (Sint16)vpad_trigger_joy(0));
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, (Sint16)vpad_trigger_joy(0));
    printf("[mac][pad] virtual gamepad %d attached (joystick id %u, mapped: %s)\n", idx, (unsigned)id,
           SDL_IsGamepad(id) ? "yes" : "NO");
    return 1;
}

int isaac_mac_vpad_disconnect(int idx) {
    if (idx < 0 || idx >= NPADS || !g_vpad[idx].joy) return 0;
    SDL_CloseJoystick(g_vpad[idx].joy);
    SDL_DetachVirtualJoystick(g_vpad[idx].id);
    printf("[mac][pad] virtual gamepad %d detached\n", idx);
    memset(&g_vpad[idx], 0, sizeof g_vpad[idx]);
    return 1;
}

void isaac_mac_vpad_button(int idx, int sdl_button, int down) {
    if (idx < 0 || idx >= NPADS || !g_vpad[idx].joy) return;
    SDL_SetJoystickVirtualButton(g_vpad[idx].joy, sdl_button, down != 0);
}

/* axis in gamepad terms: sticks -32768..32767 (SDL: down positive), triggers 0..32767 */
void isaac_mac_vpad_axis(int idx, int sdl_axis, int value) {
    if (idx < 0 || idx >= NPADS || !g_vpad[idx].joy) return;
    int v = (sdl_axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || sdl_axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
                ? vpad_trigger_joy(value) : value;
    SDL_SetJoystickVirtualAxis(g_vpad[idx].joy, sdl_axis, clamp16(v));
}
