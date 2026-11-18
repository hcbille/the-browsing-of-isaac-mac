/* mac_script.c -- scripted keyboard input for the Mac Build.
 *
 * A reusable test driver (not a temporary hack): feeds the Host's own
 * isaac_input_key path through the weak isaac_input_poll_js hook, exactly as
 * the Browser Build's page feeds it. Live SDL keyboard/mouse/pad input is M3
 * and will call isaac_input_key directly; this stays for scripted runs.
 *
 *   ISAAC_MAC_SCRIPT="until=Room 1.2(Start Room):enter,@60:d:hold=90,@200:w:hold=90"
 *
 * Comma-separated tokens:
 *   F:key[:hold=N]       press key at presented frame F (hold N frames, default 8)
 *   @F:key[:hold=N]      the same, F frames after the anchor (below)
 *   until=TEXT:key[:from=N][:every=N][:hold=N]
 *                        tap key from frame N (default 120) every N frames
 *                        (default 90) until a line of the game's own log
 *                        contains TEXT; that frame becomes the anchor for the
 *                        @F tokens and the @F screenshots (ISAAC_MAC_SHOT).
 *                        TEXT has no ',' or ':'.
 * Keys: enter esc space tab ctrl shift, a-z, 0-9, up down left right.
 *
 * This is drive_interactive.mjs's sequence: Enter held 120 ms every 1.5 s until
 * the log says "Room 1.2(Start Room)" (a run started), then D held 1.5 s, then
 * W held 1.5 s (D = move right, W = up).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t isaac_frames_presented(void);

typedef struct { int frame, rel; uint32_t vk, sc; int ext, down, done; } sev;
#define MAX_EV 256
static sev g_ev[MAX_EV];
static int g_nev;
static int g_parsed;
static int g_anchor = -1;

static struct {
    int active;
    char text[256];
    uint32_t vk, sc;
    int ext, from, every, hold;
    int next, up_at, pressing;
} g_until;

int isaac_mac_script_anchor(void) { return g_anchor; }

static int key_lookup(const char *name, uint32_t *vk, uint32_t *sc, int *ext) {
    static const struct { const char *n; uint32_t vk, sc; int ext; } T[] = {
        { "enter", 0x0D, 0x1C, 0 }, { "esc", 0x1B, 0x01, 0 }, { "space", 0x20, 0x39, 0 },
        { "tab", 0x09, 0x0F, 0 }, { "ctrl", 0x11, 0x1D, 0 }, { "shift", 0x10, 0x2A, 0 },
        { "left", 0x25, 0x4B, 1 }, { "up", 0x26, 0x48, 1 }, { "right", 0x27, 0x4D, 1 }, { "down", 0x28, 0x50, 1 },
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

static int opt(char *s, const char *key, int def) {      /* "key=N" among the ':' fields of s */
    size_t k = strlen(key);
    for (char *p = s; p; p = strchr(p, ':')) {
        if (*p == ':') ++p;
        if (!strncmp(p, key, k) && p[k] == '=') return atoi(p + k + 1);
    }
    return def;
}

static void add_press(int frame, int rel, uint32_t vk, uint32_t sc, int ext, int hold) {
    if (g_nev + 2 > MAX_EV) return;
    g_ev[g_nev++] = (sev){ frame, rel, vk, sc, ext, 1, 0 };
    g_ev[g_nev++] = (sev){ frame + hold, rel, vk, sc, ext, 0, 0 };
}

static void parse(void) {
    g_parsed = 1;
    const char *e = getenv("ISAAC_MAC_SCRIPT");
    if (!e || !*e) return;
    char *dup = strdup(e), *save = NULL;
    for (char *tok = strtok_r(dup, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        uint32_t vk, sc; int ext;
        if (!strncmp(tok, "until=", 6)) {
            char *text = tok + 6, *c = strchr(text, ':');
            if (!c) { fprintf(stderr, "[mac][script] bad token '%s'\n", tok); continue; }
            *c = 0;
            char *key = c + 1, *rest = strchr(key, ':');
            if (rest) *rest = 0;
            if (!key_lookup(key, &vk, &sc, &ext)) { fprintf(stderr, "[mac][script] unknown key '%s'\n", key); continue; }
            snprintf(g_until.text, sizeof g_until.text, "%s", text);
            g_until.vk = vk; g_until.sc = sc; g_until.ext = ext;
            g_until.from = rest ? opt(rest + 1, "from", 120) : 120;
            g_until.every = rest ? opt(rest + 1, "every", 90) : 90;
            g_until.hold = rest ? opt(rest + 1, "hold", 8) : 8;
            g_until.next = g_until.from;
            g_until.active = 1;
            continue;
        }
        int rel = 0;
        char *t = tok;
        if (*t == '@') { rel = 1; ++t; }
        char *c = strchr(t, ':');
        if (!c) { fprintf(stderr, "[mac][script] bad token '%s'\n", tok); continue; }
        *c = 0;
        int frame = atoi(t);
        char *key = c + 1, *rest = strchr(key, ':');
        if (rest) *rest = 0;
        if (!key_lookup(key, &vk, &sc, &ext)) { fprintf(stderr, "[mac][script] unknown key '%s'\n", key); continue; }
        add_press(frame, rel, vk, sc, ext, rest ? opt(rest + 1, "hold", 8) : 8);
    }
    free(dup);
    printf("[mac][script] %d key event(s)%s from ISAAC_MAC_SCRIPT\n", g_nev,
           g_until.active ? ", plus a tap-until-log trigger" : "");
}

/* the game's own log, line by line (OutputDebugStringA in host_shims_gl.c) */
void isaac_mac_log_line(const char *line) {
    if (!g_until.active || g_anchor >= 0 || !strstr(line, g_until.text)) return;
    g_anchor = (int)isaac_frames_presented();
    printf("[mac][script] log line \"%s\" seen at presented frame %d: anchor set\n", g_until.text, g_anchor);
}

static int emit(int32_t *out, uint32_t vk, uint32_t sc, int ext, int down, int frame) {
    out[0] = 1; out[1] = (int32_t)vk; out[2] = (int32_t)(sc | (ext ? 0x100u : 0u)); out[3] = down;
    printf("[mac][script] frame %d: key 0x%02x %s\n", frame, vk, down ? "down" : "up");
    return 1;
}

/* Overrides the Host's weak hook; called until it returns 0, once per input pass. */
int isaac_input_poll_js(int frame, int32_t *out) {
    if (!g_parsed) parse();
    if (g_until.active) {
        if (g_until.pressing && (g_anchor >= 0 || frame >= g_until.up_at)) {
            g_until.pressing = 0;
            return emit(out, g_until.vk, g_until.sc, g_until.ext, 0, frame);
        }
        if (g_anchor < 0 && !g_until.pressing && frame >= g_until.next) {
            g_until.pressing = 1;
            g_until.up_at = frame + g_until.hold;
            g_until.next = frame + g_until.every;
            return emit(out, g_until.vk, g_until.sc, g_until.ext, 1, frame);
        }
    }
    for (int i = 0; i < g_nev; ++i) {
        sev *e = &g_ev[i];
        if (e->done) continue;
        int due = e->rel ? (g_anchor >= 0 && frame >= g_anchor + e->frame) : frame >= e->frame;
        if (!due) continue;
        e->done = 1;
        return emit(out, e->vk, e->sc, e->ext, e->down, frame);
    }
    return 0;
}
