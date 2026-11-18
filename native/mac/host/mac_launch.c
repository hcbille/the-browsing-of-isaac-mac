/* mac_launch.c -- how the Mac Build is started: command line, launch-settings.json,
 * and the few things a launch decides (Vanilla Launch, debug console, Lua debug).
 *
 * Contract: docs/mac/launcher.md ("Launch contract") and docs/mac/PROGRESS.md
 * ("Integration"). In short:
 *
 *   isaac-mac [--headless] [--launch-settings <json>] [--vanilla] [--debug-console]
 *             [--lua-debug] [--segs F] [--bundle D] [--layer-dir D] [--save-dir D]
 *             [--mods-dir D] [--modscan F] [--gate-log F] [--stage main|boot]
 *             [--scale integer|fill] [--window WxH]     (M5, mac_window.c; JSON key scale_mode)
 *   isaac-mac [--headless] <isaac.segs.bin> <instance dir> [boot|main] [save dir]   (dev)
 *
 * Precedence for every path: command-line flag, launch-settings.json, environment
 * (REPENTANCE_MAC_*), then the default. The flags --vanilla, --debug-console and
 * --lua-debug can only turn an option on (the JSON is authoritative otherwise).
 * Without --launch-settings the Layer, mods and gate log are simply not configured
 * (the dev positional form keeps behaving as before).
 *
 * Defaults for a bundle (M11): relative to the executable,
 *   <exe dir>/../Resources/isaac.segs.bin, .../game-bundle, .../layer
 * (Repentance+.app/Contents/MacOS/isaac-mac -> Contents/Resources/...).
 */
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <mach-o/dyld.h>

#include "isaac_host.h"

#include "mac_launch.h"

mac_cfg g_mac_cfg;

/* ---- a tiny JSON reader: the top-level object's scalar members ---------------- */
typedef struct { char *key; int type; char *s; } jkv;   /* type: 0 null, 1 true, 2 false, 3 number, 4 string */
static jkv *g_kv;
static int g_nkv;

static void skip_ws(const char **p) { while (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r') ++*p; }

static void put_utf8(char **o, unsigned cp) {
    if (cp < 0x80) *(*o)++ = (char)cp;
    else if (cp < 0x800) { *(*o)++ = (char)(0xC0 | (cp >> 6)); *(*o)++ = (char)(0x80 | (cp & 63)); }
    else if (cp < 0x10000) { *(*o)++ = (char)(0xE0 | (cp >> 12)); *(*o)++ = (char)(0x80 | ((cp >> 6) & 63)); *(*o)++ = (char)(0x80 | (cp & 63)); }
    else { *(*o)++ = (char)(0xF0 | (cp >> 18)); *(*o)++ = (char)(0x80 | ((cp >> 12) & 63)); *(*o)++ = (char)(0x80 | ((cp >> 6) & 63)); *(*o)++ = (char)(0x80 | (cp & 63)); }
}

/* *p at the opening quote; returns a malloc'd decoded string, *p after the closing quote */
static char *parse_string(const char **p) {
    const char *s = *p + 1;
    char *out = malloc(strlen(s) + 8), *o = out;
    if (!out) return NULL;
    while (*s && *s != '"') {
        if (*s == '\\') {
            ++s;
            switch (*s) {
            case 'n': *o++ = '\n'; ++s; break;
            case 't': *o++ = '\t'; ++s; break;
            case 'r': *o++ = '\r'; ++s; break;
            case 'b': *o++ = '\b'; ++s; break;
            case 'f': *o++ = '\f'; ++s; break;
            case 'u': {
                unsigned cp = 0;
                for (int i = 1; i <= 4 && s[i]; ++i) cp = cp * 16 + (unsigned)(s[i] <= '9' ? s[i] - '0' : (s[i] | 32) - 'a' + 10);
                s += 5;
                if (cp >= 0xD800 && cp < 0xDC00 && s[0] == '\\' && s[1] == 'u') {
                    unsigned lo = 0;
                    for (int i = 2; i <= 5 && s[i]; ++i) lo = lo * 16 + (unsigned)(s[i] <= '9' ? s[i] - '0' : (s[i] | 32) - 'a' + 10);
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    s += 6;
                }
                put_utf8(&o, cp);
                break;
            }
            default: if (*s) *o++ = *s++; break;       /* \" \\ \/ */
            }
        } else *o++ = *s++;
    }
    *o = 0;
    *p = *s ? s + 1 : s;
    return out;
}

static void skip_value(const char **p) {
    skip_ws(p);
    if (**p == '"') { free(parse_string(p)); return; }
    if (**p == '{' || **p == '[') {
        int depth = 0;
        do {
            if (**p == '"') { free(parse_string(p)); continue; }
            if (**p == '{' || **p == '[') ++depth;
            else if (**p == '}' || **p == ']') --depth;
            ++*p;
        } while (depth > 0 && **p);
        return;
    }
    while (**p && **p != ',' && **p != '}' && **p != ']') ++*p;
}

static int json_parse(const char *p) {
    skip_ws(&p);
    if (*p == '\xEF') p += 3;                    /* BOM */
    skip_ws(&p);
    if (*p != '{') return 0;
    ++p;
    for (;;) {
        skip_ws(&p);
        if (*p == '}' || !*p) return 1;
        if (*p == ',') { ++p; continue; }
        if (*p != '"') return 0;
        char *key = parse_string(&p);
        skip_ws(&p);
        if (*p != ':') { free(key); return 0; }
        ++p;
        skip_ws(&p);
        jkv kv = { key, 0, NULL };
        if (*p == '"') { kv.type = 4; kv.s = parse_string(&p); }
        else if (!strncmp(p, "true", 4)) { kv.type = 1; p += 4; }
        else if (!strncmp(p, "false", 5)) { kv.type = 2; p += 5; }
        else if (!strncmp(p, "null", 4)) { kv.type = 0; p += 4; }
        else if (*p == '{' || *p == '[') { skip_value(&p); free(key); continue; }
        else {
            const char *q = p;
            while (*q && *q != ',' && *q != '}' && *q != ' ' && *q != '\n' && *q != '\r' && *q != '\t') ++q;
            kv.type = 3; kv.s = strndup(p, (size_t)(q - p)); p = q;
        }
        g_kv = realloc(g_kv, sizeof *g_kv * (size_t)(g_nkv + 1));
        g_kv[g_nkv++] = kv;
    }
}

static const jkv *json_get(const char *key) {
    for (int i = 0; i < g_nkv; ++i) if (!strcmp(g_kv[i].key, key)) return &g_kv[i];
    return NULL;
}
static const char *json_str(const char *key) {
    const jkv *k = json_get(key);
    return k && k->type == 4 && k->s[0] ? k->s : NULL;
}
static int json_bool(const char *key, int dflt) {
    const jkv *k = json_get(key);
    if (!k) return dflt;
    if (k->type == 1) return 1;
    if (k->type == 2) return 0;
    if (k->type == 3) return atoi(k->s) != 0;
    return dflt;
}

/* ---- paths ------------------------------------------------------------------- */
static char *xstrdup(const char *s) { char *d = strdup(s); if (!d) abort(); return d; }

/* absolute form of a path that may not exist yet (the part that exists is realpath'd) */
static char *absolute(const char *path) {
    char buf[PATH_MAX], cwd[PATH_MAX];
    if (realpath(path, buf)) return xstrdup(buf);
    if (path[0] == '/') return xstrdup(path);
    if (!getcwd(cwd, sizeof cwd)) return xstrdup(path);
    snprintf(buf, sizeof buf, "%s/%s", cwd, strncmp(path, "./", 2) == 0 ? path + 2 : path);
    return xstrdup(buf);
}

static int is_dir(const char *p) { struct stat st; return stat(p, &st) == 0 && S_ISDIR(st.st_mode); }
static int is_file(const char *p) { struct stat st; return stat(p, &st) == 0 && S_ISREG(st.st_mode); }

static char *exe_dir(void) {
    char raw[PATH_MAX], real[PATH_MAX];
    uint32_t n = sizeof raw;
    if (_NSGetExecutablePath(raw, &n) != 0 || !realpath(raw, real)) return NULL;
    char *slash = strrchr(real, '/');
    if (slash) *slash = 0;
    return xstrdup(real);
}

static const char *env_nonempty(const char *name) {
    const char *e = getenv(name);
    return e && *e ? e : NULL;
}

static char *default_support_dir(void) {
    const char *e = env_nonempty("REPENTANCE_MAC_SUPPORT_DIR");
    if (e) return absolute(e);
    const char *home = getenv("HOME");
    char buf[PATH_MAX];
    snprintf(buf, sizeof buf, "%s/Library/Application Support/Repentance+ Mac", home ? home : ".");
    return xstrdup(buf);
}

/* ---- arguments --------------------------------------------------------------- */
/* "--name value" or "--name=value" */
static int take(int argc, char **argv, int *i, const char *name, const char **out) {
    size_t n = strlen(name);
    if (strncmp(argv[*i], name, n) != 0) return 0;
    if (argv[*i][n] == '=') { *out = argv[*i] + n + 1; return 1; }
    if (argv[*i][n] != 0) return 0;
    if (*i + 1 >= argc) { fprintf(stderr, "[mac] %s needs a value\n", name); exit(2); }
    *out = argv[++*i];
    return 1;
}

int mac_launch_parse(int argc, char **argv) {
    const char *f_segs = NULL, *f_bundle = NULL, *f_layer = NULL, *f_save = NULL, *f_mods = NULL,
               *f_scan = NULL, *f_gate = NULL, *f_stage = NULL, *f_ls = NULL, *f_scale = NULL, *f_window = NULL;
    int f_vanilla = 0, f_console = 0, f_lua = 0;
    const char *pos[4] = { 0 };
    int npos = 0;
    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (take(argc, argv, &i, "--launch-settings", &f_ls) || take(argc, argv, &i, "--segs", &f_segs) ||
            take(argc, argv, &i, "--bundle", &f_bundle) || take(argc, argv, &i, "--layer-dir", &f_layer) ||
            take(argc, argv, &i, "--save-dir", &f_save) || take(argc, argv, &i, "--mods-dir", &f_mods) ||
            take(argc, argv, &i, "--modscan", &f_scan) || take(argc, argv, &i, "--gate-log", &f_gate) ||
            take(argc, argv, &i, "--stage", &f_stage) || take(argc, argv, &i, "--scale", &f_scale) ||
            take(argc, argv, &i, "--window", &f_window)) continue;
        if (!strcmp(a, "--vanilla")) f_vanilla = 1;
        else if (!strcmp(a, "--debug-console")) f_console = 1;
        else if (!strcmp(a, "--lua-debug")) f_lua = 1;
        else if (a[0] == '-' && a[1] == '-') fprintf(stderr, "[mac] ignoring unknown option %s\n", a);
        else if (npos < 4) pos[npos++] = a;
    }
    if (!f_ls) f_ls = env_nonempty("REPENTANCE_MAC_LAUNCH_SETTINGS");
    mac_cfg *c = &g_mac_cfg;
    memset(c, 0, sizeof *c);
    if (f_ls) {
        FILE *f = fopen(f_ls, "rb");
        if (!f) { fprintf(stderr, "[mac] cannot read launch settings %s: %s\n", f_ls, strerror(errno)); return 0; }
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *buf = malloc((size_t)n + 1);
        if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n) { fclose(f); fprintf(stderr, "[mac] cannot read %s\n", f_ls); return 0; }
        fclose(f);
        buf[n] = 0;
        if (!json_parse(buf)) { fprintf(stderr, "[mac] %s is not a JSON object\n", f_ls); return 0; }
        free(buf);
        c->launch_settings = absolute(f_ls);
    }
    int configured = c->launch_settings != NULL;

    /* options: JSON, plus the flags turning one on */
    c->vanilla = json_bool("vanilla_launch", 0) || f_vanilla;
    c->debug_console = json_bool("debug_console", 0) || f_console;
    c->lua_debug = json_bool("lua_debug", 0) || f_lua;

    /* window scaling: flag, JSON scale_mode, env REPENTANCE_MAC_SCALE / ISAAC_MAC_SCALE, then "integer" */
    {
        const char *m = f_scale;
        if (!m) m = json_str("scale_mode");
        if (!m) m = env_nonempty("REPENTANCE_MAC_SCALE");
        if (!m) m = env_nonempty("ISAAC_MAC_SCALE");
        c->scale_fill = 0;
        if (m && !strcmp(m, "fill")) c->scale_fill = 1;
        else if (m && strcmp(m, "integer")) fprintf(stderr, "[mac] unknown scale mode '%s' (integer or fill); using integer\n", m);
        /* initial window size WxH in points (flag --window, env ISAAC_MAC_WINDOW); a test and power-user knob */
        const char *w = f_window ? f_window : env_nonempty("ISAAC_MAC_WINDOW");
        int ww = 0, wh = 0;
        if (w && sscanf(w, "%dx%d", &ww, &wh) == 2 && ww >= 160 && wh >= 90 && ww <= 16384 && wh <= 16384) { c->win_w = ww; c->win_h = wh; }
        else if (w) fprintf(stderr, "[mac] bad window size '%s' (want WxH); using the default\n", w);
    }

    /* where things are */
    const char *v;
    c->support_dir = (v = json_str("support_dir")) ? absolute(v) : (configured ? default_support_dir() : NULL);
    if ((v = f_save)) c->save_dir = absolute(v);
    else if ((v = json_str("save_dir")) || (v = env_nonempty("REPENTANCE_MAC_SAVE_DIR"))) c->save_dir = absolute(v);
    else if (c->support_dir && configured) { char b[PATH_MAX]; snprintf(b, sizeof b, "%s/saves", c->support_dir); c->save_dir = xstrdup(b); }
    if ((v = f_mods)) c->mods_dir = absolute(v);
    else if ((v = json_str("mods_dir")) || (v = env_nonempty("REPENTANCE_MAC_MODS_DIR"))) c->mods_dir = absolute(v);
    else if (c->support_dir && configured) { char b[PATH_MAX]; snprintf(b, sizeof b, "%s/mods", c->support_dir); c->mods_dir = xstrdup(b); }
    if ((v = f_scan)) c->modscan_path = absolute(v);
    else if ((v = json_str("modscan_path"))) c->modscan_path = absolute(v);
    else if (c->support_dir && configured) { char b[PATH_MAX]; snprintf(b, sizeof b, "%s/modscan.lua", c->support_dir); c->modscan_path = xstrdup(b); }
    if ((v = f_gate)) c->gate_log_path = absolute(v);
    else if ((v = json_str("gate_log_path")) || (v = env_nonempty("REPENTANCE_MAC_GATE_LOG"))) c->gate_log_path = absolute(v);
    else if (c->support_dir && configured) { char b[PATH_MAX]; snprintf(b, sizeof b, "%s/logs/layer-gate.json", c->support_dir); c->gate_log_path = xstrdup(b); }

    /* program files: flags, JSON (segs_path, bundle_dir, layer_dir), env, the bundle's Resources */
    char *ed = exe_dir();
    char res[PATH_MAX] = "";
    if (ed) snprintf(res, sizeof res, "%s/../Resources", ed);
    char b[PATH_MAX];
    if (f_segs) c->segs = absolute(f_segs);
    else if ((v = json_str("segs_path")) || (v = env_nonempty("REPENTANCE_MAC_SEGS"))) c->segs = absolute(v);
    else if (npos >= 1) c->segs = absolute(pos[0]);
    else if (res[0]) { snprintf(b, sizeof b, "%s/isaac.segs.bin", res); if (is_file(b)) c->segs = absolute(b); }
    if (f_bundle) c->bundle = absolute(f_bundle);
    else if ((v = json_str("bundle_dir")) || (v = env_nonempty("REPENTANCE_MAC_BUNDLE"))) c->bundle = absolute(v);
    else if (npos >= 2) c->bundle = absolute(pos[1]);
    else if (res[0]) { snprintf(b, sizeof b, "%s/game-bundle", res); if (is_dir(b)) c->bundle = absolute(b); }
    if (f_layer) c->layer_dir = absolute(f_layer);
    else if ((v = json_str("layer_dir")) || (v = env_nonempty("REPENTANCE_MAC_LAYER_DIR"))) c->layer_dir = absolute(v);
    else if (res[0]) { snprintf(b, sizeof b, "%s/layer", res); if (is_dir(b)) c->layer_dir = absolute(b); }

    c->stage = f_stage ? f_stage : (npos >= 3 ? pos[2] : "main");
    if (npos >= 4 && !c->save_dir) { c->save_dir = absolute(pos[3]); c->positional_save = 1; }
    return 1;
}

/* ---- what the launch decides inside the Guest -------------------------------- */

/* The Guest's own argv (the game scans it for --luadebug, --luaheapsize=N). It is
 * what a Windows launch would pass, so --lua-debug does exactly what --luadebug does:
 * the game opens io, os and debug for mods. */
int mac_guest_argc(void) { return 1 + (g_mac_cfg.lua_debug ? 1 : 0); }
const char *mac_guest_argv(int i) {
    if (i == 0) return "isaac-ng.exe";
    if (i == 1 && g_mac_cfg.lua_debug) return "--luadebug";
    return NULL;
}

/* Options.EnableDebugConsole is the byte at Manager+0x2a398 (Manager at [0x00c7169c]);
 * Console::Update reads it every frame, so setting it takes effect at once. The game
 * rewrites options.ini with it on exit, so the Launcher's switch is authoritative in
 * both directions: the byte is set once, after the Manager has read options.ini. */
void mac_launch_frame(uint32_t frame) {
    static int done;
    if (done || frame < 20) return;
    uint32_t mgr = isaac_r32(0x00c7169cu);
    if (!mgr) return;
    done = 1;
    uint8_t *p = (uint8_t *)isaac_g(mgr + 0x2a398u);
    uint8_t before = *p;
    *p = g_mac_cfg.debug_console ? 1 : 0;
    printf("[mac] debug console %s by the launch settings (was %s in options.ini)\n",
           *p ? "on" : "off", before ? "on" : "off");
}
