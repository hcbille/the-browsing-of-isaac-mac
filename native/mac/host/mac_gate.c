/* mac_gate.c -- the Layer's gate decisions as logs/layer-gate.json.
 *
 * host_rgon.c's B.gate(R, ALLOW) runs at the end of rgon.lua, once the gate has
 * weighed every scanned mod; under ISAAC_NATIVE it first calls
 * isaac_mac_gate_log(L, 1), which writes REPENTOGON.Extras.Gate.mods to
 * gate_log_path (format: docs/mac/launcher.md). The Layer's own plain-text
 * "gate: ..." lines stay in the game log. Called once per Lua state the Layer
 * loads into; each call rewrites the file atomically with the same content.
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "lua.h"
#include "lauxlib.h"

typedef struct { char *p; size_t n, cap; } sbuf;
static void sb_put(sbuf *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        b->cap = (b->n + n + 1) * 2 + 256;
        b->p = realloc(b->p, b->cap);
        if (!b->p) abort();
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = 0;
}
static void sb_str(sbuf *b, const char *s) { sb_put(b, s, strlen(s)); }
static void sb_fmt(sbuf *b, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#include <stdarg.h>
static void sb_fmt(sbuf *b, const char *fmt, ...) {
    char t[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(t, sizeof t, fmt, ap);
    va_end(ap);
    sb_str(b, t);
}
/* a JSON string literal */
static void sb_json(sbuf *b, const char *s) {
    sb_put(b, "\"", 1);
    for (; *s; ++s) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { char e[2] = { '\\', (char)c }; sb_put(b, e, 2); }
        else if (c == '\n') sb_str(b, "\\n");
        else if (c == '\r') sb_str(b, "\\r");
        else if (c == '\t') sb_str(b, "\\t");
        else if (c < 0x20) sb_fmt(b, "\\u%04x", c);
        else sb_put(b, (const char *)&c, 1);
    }
    sb_put(b, "\"", 1);
}

const char *g_mac_gate_log_path;      /* NULL: nothing is written */

static void mkdirs_for(const char *path) {
    char tmp[4096];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = tmp + 1; *p; ++p)
        if (*p == '/') { *p = 0; mkdir(tmp, 0755); *p = '/'; }
}

static int write_atomic(const char *path, const char *data, size_t n) {
    char tmp[4200];
    mkdirs_for(path);
    snprintf(tmp, sizeof tmp, "%s.tmp.%d", path, (int)getpid());
    FILE *f = fopen(tmp, "wb");
    if (!f) return 0;
    int ok = fwrite(data, 1, n, f) == n && fflush(f) == 0 && fsync(fileno(f)) == 0;
    ok = (fclose(f) == 0) && ok;
    if (ok && rename(tmp, path) != 0) ok = 0;
    if (!ok) unlink(tmp);
    return ok;
}

static void stamp(sbuf *b) {
    time_t t = time(NULL);
    struct tm tm;
    gmtime_r(&t, &tm);
    char s[40];
    strftime(s, sizeof s, "%Y-%m-%dT%H:%M:%SZ", &tm);
    sb_json(b, s);
}

/* Vanilla Launch (or no Layer to load): the file says the Layer is off. */
void isaac_mac_gate_log_off(const char *reason) {
    if (!g_mac_gate_log_path) return;
    sbuf b = { 0 };
    sb_str(&b, "{\n  \"version\": 1,\n  \"layer_on\": false,\n  \"reason\": ");
    sb_json(&b, reason ? reason : "");
    sb_str(&b, ",\n  \"written_at\": ");
    stamp(&b);
    sb_str(&b, ",\n  \"mods\": {}\n}\n");
    if (write_atomic(g_mac_gate_log_path, b.p, b.n)) printf("[mac] gate log: layer off (%s) -> %s\n", reason, g_mac_gate_log_path);
    else fprintf(stderr, "[mac] gate log: cannot write %s: %s\n", g_mac_gate_log_path, strerror(errno));
    free(b.p);
}

static int cmp(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

/* R is at stack index `r`: R.Extras.Gate = { scanned, mods = { [folder] = { shown, why, used, missing = {...} } } } */
void isaac_mac_gate_log(lua_State *L, int r) {
    if (!g_mac_gate_log_path) return;
    r = lua_absindex(L, r);
    const int top = lua_gettop(L);
    if (!lua_checkstack(L, 12) || !lua_istable(L, r)) return;
    lua_getfield(L, r, "Extras");
    if (!lua_istable(L, -1)) { lua_settop(L, top); return; }
    lua_getfield(L, -1, "Gate");
    if (!lua_istable(L, -1)) { lua_settop(L, top); return; }
    const int gate = lua_gettop(L);
    lua_getfield(L, gate, "scanned");
    const int scanned = lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, gate, "mods");
    if (!lua_istable(L, -1)) { lua_settop(L, top); return; }
    const int mods = lua_gettop(L);

    char **keys = NULL;
    int nk = 0;
    lua_pushnil(L);
    while (lua_next(L, mods)) {
        if (lua_type(L, -2) == LUA_TSTRING) {
            keys = realloc(keys, sizeof *keys * (size_t)(nk + 1));
            keys[nk++] = strdup(lua_tostring(L, -2));
        }
        lua_pop(L, 1);
    }
    qsort(keys, (size_t)nk, sizeof *keys, cmp);

    sbuf b = { 0 };
    sb_str(&b, "{\n  \"version\": 1,\n  \"layer_on\": true,\n  \"scanned\": ");
    sb_str(&b, scanned ? "true" : "false");
    sb_str(&b, ",\n  \"written_at\": ");
    stamp(&b);
    sb_str(&b, ",\n  \"mods\": {");
    int shown_n = 0;
    for (int i = 0; i < nk; ++i) {
        lua_getfield(L, mods, keys[i]);
        const int row = lua_gettop(L);
        if (lua_istable(L, row)) {
            lua_getfield(L, row, "shown");
            int shown = lua_toboolean(L, -1);
            lua_pop(L, 1);
            lua_getfield(L, row, "why");
            const char *why = lua_tostring(L, -1);
            lua_getfield(L, row, "used");
            lua_Integer used = lua_tointeger(L, -1);
            lua_pop(L, 1);
            shown_n += shown;
            sb_str(&b, i ? ",\n    " : "\n    ");
            sb_json(&b, keys[i]);
            sb_str(&b, ": { \"shown\": ");
            sb_str(&b, shown ? "true" : "false");
            sb_str(&b, ", \"why\": ");
            sb_json(&b, why ? why : "");
            sb_fmt(&b, ", \"used\": %lld, \"missing\": [", (long long)used);
            lua_getfield(L, row, "missing");
            if (lua_istable(L, -1)) {
                lua_Unsigned nm = lua_rawlen(L, -1);
                for (lua_Unsigned k = 1; k <= nm; ++k) {
                    lua_rawgeti(L, -1, (lua_Integer)k);
                    const char *name = lua_tostring(L, -1);
                    if (name) { if (k > 1) sb_str(&b, ", "); sb_json(&b, name); }
                    lua_pop(L, 1);
                }
            }
            sb_str(&b, "] }");
        }
        lua_settop(L, mods);
    }
    sb_str(&b, nk ? "\n  }\n}\n" : "}\n}\n");
    if (write_atomic(g_mac_gate_log_path, b.p, b.n))
        printf("[mac] gate log: %d mod(s) decided, %d shown -> %s\n", nk, shown_n, g_mac_gate_log_path);
    else
        fprintf(stderr, "[mac] gate log: cannot write %s: %s\n", g_mac_gate_log_path, strerror(errno));
    free(b.p);
    for (int i = 0; i < nk; ++i) free(keys[i]);
    free(keys);
    lua_settop(L, top);
}
