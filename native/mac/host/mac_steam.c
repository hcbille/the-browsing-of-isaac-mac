/* mac_steam.c -- the Mac Build's Steam bridge (M9).  Design: docs/mac/steam-bridge.md
 * (sections 3 and 7; section 7 wins where they differ).
 *
 * What it does
 *   - On the first SteamInternal_ContextInit it tries to load the real
 *     libsteam_api.dylib (arm64), calls SteamAPI_InitFlat and fetches the
 *     native ISteamUserStats 013.  Any failure logs ONE
 *     "[mac][steam] disabled: <reason>" line and the Host keeps its previous
 *     behaviour (host_shims_steam.c: fake context for two slots, NULL for the
 *     rest).  Never exits, never prompts, never blocks.
 *   - When live, the accessor slot 0xbf93c8 (ISteamUserStats013) gets a guest
 *     object whose vtable holds one token per observed method, each with the
 *     exact callee purge from steam-bridge.md 7.2.  Achievements and the stat
 *     calls the game uses are forwarded to the native object.  Every other
 *     accessor slot (including ISteamUser at 0xc5c510 and ISteamRemoteStorage
 *     at 0xc5c3a4) reads NULL, the game's own no-Steam arm.
 *
 * Hard limits kept here
 *   - SteamAPI_RestartAppIfNecessary is never resolved or called.
 *   - No SteamAPI_ISteamRemoteStorage_* symbol is ever resolved: Steam Cloud
 *     can not be written from this file.  FileWrite/FileShare/
 *     AttachLeaderboardUGC and UploadLeaderboardScore are refused (never
 *     forwarded); the leaderboard offsets answer 0.
 *   - ISAAC_STEAM_DRYRUN=1: every write is logged and answered "true" without
 *     being forwarded.
 *
 * Token pool (why this file rewrites rows of isaac_imports)
 *   The vtable slots must be shim tokens, and a token's identity IS the method
 *   (a handler sees only the CPU state).  The generated table has no spare
 *   PROVIDED rows for 17 distinct methods and gen_shims.py is outside M9's
 *   file list, so when the bridge is live this file re-binds fn/arg_bytes of
 *   rows that are dead in a live bridge: the 8 CSteamAPIContext_* tokens
 *   (only used by the fake context, which is not built when live) and, if more
 *   are needed, steam-independent rows with verdict NEVER_CALLED and 0 call
 *   sites (lua5.3 luaopen_*, vcruntime140 __CxxLongjmpUnwind/__std_terminate/
 *   _purecall: 8 + 7 + 3 + user32 DefWindowProcA = 19 rows for the 19 needed).  Cleaner follow-up: 17 PROVIDED rows in
 *   gen_shims.py, then delete pool_bind().
 *
 * Callbacks: the Guest's SteamAPI_RegisterCallback records stay in
 * host_shims_misc.c.  No achievement call needs a callback (7.1 item 6), so
 * ManualDispatch is NOT initialised and nothing is dispatched.  TODO (call
 * results for leaderboards): ManualDispatch_Init, per-frame RunFrame +
 * GetNextCallback/FreeLastCallback, pack-4 -> pack-8 conversion, then the
 * guest object's Run(void*, bool, uint64).
 */
#if defined(ISAAC_NATIVE) && ISAAC_NATIVE

#include <dlfcn.h>
#include <libgen.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mac_steam.h"

#define SLOT_USER      0x00c5c510u           /* initData of ISteamUser (SteamUser023) */
#define SLOT_USERSTATS 0x00bf93c8u           /* initData of STEAMUSERSTATS_INTERFACE_VERSION013 */
#define US_VERSION     "STEAMUSERSTATS_INTERFACE_VERSION013"

/* guest scratch (host-written, below ISAAC_GUEST_LIMIT_VA; 0xd100-0xd8ff are taken) */
#define US_OBJ_VA      (ISAAC_TEB_VA + 0xd900u)
#define US_VTBL_VA     (ISAAC_TEB_VA + 0xda00u)
#define US_VTBL_SLOTS  45u                   /* +0x00 .. +0xb0 */
#define USR_VTBL_VA    (ISAAC_TEB_VA + 0xdb00u)
#define USR_OBJ_VA     (ISAAC_TEB_VA + 0xdbc0u)
#define USR_VTBL_SLOTS 48u
#define ST_SCRATCH_VA  (ISAAC_TEB_VA + 0xdc00u)   /* selftest only */

typedef int32_t (*fn_InitFlat)(char *errMsg);
typedef bool    (*fn_IsSteamRunning)(void);
typedef void    (*fn_void)(void);
typedef int32_t (*fn_GetHSteamUser)(void);
typedef void   *(*fn_FindIface)(int32_t, const char *);
typedef void   *(*fn_Accessor)(void);
typedef bool    (*fn_GetAch)(void *, const char *, bool *);
typedef bool    (*fn_Name)(void *, const char *);
typedef bool    (*fn_Void)(void *);
typedef bool    (*fn_GetStatI)(void *, const char *, int32_t *);
typedef bool    (*fn_SetStatI)(void *, const char *, int32_t);
typedef bool    (*fn_Indicate)(void *, const char *, uint32_t, uint32_t);
typedef bool    (*fn_Limits)(void *, const char *, int32_t *, int32_t *);

static struct {
    int state;                 /* 0 untried, 1 live, -1 disabled */
    void *h;
    int dryrun;
    void *us;                  /* native ISteamUserStats* */
    fn_void Shutdown;
    fn_GetAch GetAch;
    fn_Name SetAch;
    fn_Void Store;
    fn_GetStatI GetStatI;
    fn_SetStatI SetStatI;
    fn_Indicate Indicate;
    fn_Limits LimitsI;
} S;

#define SLOG(...) isaac_log("[mac][steam] " __VA_ARGS__)

static void disable(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#include <stdarg.h>
static void disable(const char *fmt, ...) {
    char why[1300];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(why, sizeof why, fmt, ap);
    va_end(ap);
    if (S.state != -1) SLOG("disabled: %s", why);
    S.state = -1;
}

static void *sym(const char *n) { return S.h ? dlsym(S.h, n) : NULL; }

/* ------------------------------------------------------------- loading --- */

/* dlopen does not expand @executable_path for absolute-path lookups here, so
 * the executable directory is resolved by hand. */
static int exe_dir(char *out, size_t cap) {
    char raw[PATH_MAX], real[PATH_MAX];
    uint32_t n = sizeof raw;
    if (_NSGetExecutablePath(raw, &n) != 0) return 0;
    if (!realpath(raw, real)) return 0;
    char *d = dirname(real);
    if (!d || strlen(d) + 1 > cap) return 0;
    strcpy(out, d);
    return 1;
}

static void *open_dylib(char *why, size_t whycap) {
    char dir[PATH_MAX], p0[PATH_MAX + 64], p1[PATH_MAX + 64];
    const char *cand[3] = { NULL, NULL, getenv("ISAAC_STEAM_DYLIB") };
    if (exe_dir(dir, sizeof dir)) {
        snprintf(p0, sizeof p0, "%s/../Frameworks/libsteam_api.dylib", dir);
        snprintf(p1, sizeof p1, "%s/libsteam_api.dylib", dir);
        cand[0] = p0;
        cand[1] = p1;
    }
    char tried[3 * (PATH_MAX + 70)];
    tried[0] = 0;
    for (int i = 0; i < 3; ++i) {
        if (!cand[i] || !*cand[i]) continue;
        if (strlen(tried) + strlen(cand[i]) + 3 < sizeof tried) {
            if (tried[0]) strcat(tried, ", ");
            strcat(tried, cand[i]);
        }
        if (access(cand[i], F_OK) != 0) continue;
        void *h = dlopen(cand[i], RTLD_NOW | RTLD_LOCAL);
        if (h) {
            SLOG("dlopen ok (%s)", cand[i]);
            return h;
        }
        const char *e = dlerror();
        snprintf(why, whycap, "dlopen failed (%s)", e ? e : "unknown error");
        return NULL;      /* first file that exists but will not load decides */
    }
    snprintf(why, whycap, "dlopen failed (libsteam_api.dylib not found; tried: %s)",
             tried[0] ? tried : "no candidate path");
    return NULL;
}

/* ----------------------------------------------------- guest-side helpers - */

static int gname(uint32_t va, char *buf, size_t cap) {
    return isaac_guest_cstr(va, buf, cap, "steam") != NULL;
}
static void w8(uint32_t va, uint8_t v) { *(uint8_t *)isaac_g(va) = v; }

/* ---------------------------------------------------- live token handlers - */
/* thiscall: ECX = this (ignored; one object), arguments from isaac_arg(cpu, 0).
 * The dispatcher pops 4 + arg_bytes after the handler returns; arg_bytes is
 * set in k_methods below (steam-bridge.md 7.2 purges). */

/* +0x04 GetStat(const char*, int32*) -- 8 */
static void h_get_stat(CpuState *restrict cpu) {
    char n[128]; uint32_t out = isaac_arg(cpu, 1);
    static unsigned c;
    if (!gname(isaac_arg(cpu, 0), n, sizeof n)) { cpu->EAX = 0; return; }
    int32_t v = 0;
    int ok = S.GetStatI && S.GetStatI(S.us, n, &v);
    if (ok && isaac_is_guest_va(out)) isaac_w32(out, (uint32_t)v);
    if (c++ < 8) SLOG("GetStat %s -> ok=%d value=%d", n, ok, (int)v);
    cpu->EAX = ok ? 1 : 0;
}

/* +0x0c SetStat(const char*, int32) -- 8 */
static void h_set_stat(CpuState *restrict cpu) {
    char n[128]; int32_t v = (int32_t)isaac_arg(cpu, 1);
    if (!gname(isaac_arg(cpu, 0), n, sizeof n)) { cpu->EAX = 0; return; }
    if (S.dryrun) { SLOG("DRYRUN SetStat %s=%d", n, (int)v); cpu->EAX = 1; return; }
    int ok = S.SetStatI && S.SetStatI(S.us, n, v);
    SLOG("SetStat %s=%d -> %d", n, (int)v, ok);
    cpu->EAX = ok ? 1 : 0;
}

/* +0x14 GetAchievement(const char*, bool*) -- 8 */
static void h_get_ach(CpuState *restrict cpu) {
    char n[128]; uint32_t out = isaac_arg(cpu, 1);
    static unsigned c;
    if (!gname(isaac_arg(cpu, 0), n, sizeof n)) { cpu->EAX = 0; return; }
    bool a = false;
    int ok = S.GetAch && S.GetAch(S.us, n, &a);
    if (isaac_is_guest_va(out)) w8(out, ok && a ? 1 : 0);
    if (c++ < 8) SLOG("GetAchievement %s -> ok=%d achieved=%d", n, ok, (int)a);
    cpu->EAX = ok ? 1 : 0;
}

/* +0x18 SetAchievement(const char*) -- 4 */
static void h_set_ach(CpuState *restrict cpu) {
    char n[128];
    if (!gname(isaac_arg(cpu, 0), n, sizeof n)) { cpu->EAX = 0; return; }
    if (S.dryrun) { SLOG("DRYRUN SetAchievement %s", n); cpu->EAX = 1; return; }
    int ok = S.SetAch && S.SetAch(S.us, n);
    SLOG("SetAchievement %s -> %d", n, ok);
    cpu->EAX = ok ? 1 : 0;
}

/* +0x24 StoreStats() -- 0 */
static void h_store(CpuState *restrict cpu) {
    if (S.dryrun) { SLOG("DRYRUN StoreStats"); cpu->EAX = 1; return; }
    int ok = S.Store && S.Store(S.us);
    SLOG("StoreStats -> %d", ok);
    cpu->EAX = ok ? 1 : 0;
}

/* +0x30 IndicateAchievementProgress(const char*, u32, u32) -- 12 */
static void h_indicate(CpuState *restrict cpu) {
    char n[128]; uint32_t cur = isaac_arg(cpu, 1), max = isaac_arg(cpu, 2);
    if (!gname(isaac_arg(cpu, 0), n, sizeof n)) { cpu->EAX = 0; return; }
    if (S.dryrun) { SLOG("DRYRUN IndicateAchievementProgress %s %u/%u", n, cur, max); cpu->EAX = 1; return; }
    int ok = S.Indicate && S.Indicate(S.us, n, cur, max);
    SLOG("IndicateAchievementProgress %s %u/%u -> %d", n, cur, max, ok);
    cpu->EAX = ok ? 1 : 0;
}

/* +0xac GetAchievementProgressLimits(const char*, int32*, int32*) -- 12 (int overload) */
static void h_limits(CpuState *restrict cpu) {
    char n[128]; uint32_t pmin = isaac_arg(cpu, 1), pmax = isaac_arg(cpu, 2);
    if (!gname(isaac_arg(cpu, 0), n, sizeof n)) { cpu->EAX = 0; return; }
    int32_t lo = 0, hi = 0;
    int ok = S.LimitsI && S.LimitsI(S.us, n, &lo, &hi);
    if (ok && isaac_is_guest_va(pmin)) isaac_w32(pmin, (uint32_t)lo);
    if (ok && isaac_is_guest_va(pmax)) isaac_w32(pmax, (uint32_t)hi);
    cpu->EAX = ok ? 1 : 0;
}

/* Leaderboard offsets: no call results exist yet, so every one answers 0
 * (EDX:EAX = 0 for the SteamAPICall_t returns).  +0x78 and +0x7c are refusals:
 * profile write / Steam Cloud share chain, never forwarded. */
static void zero_answer(CpuState *restrict cpu, const char *what, int refused) {
    static uint32_t seen[16]; static unsigned nseen;
    uint32_t key = (uint32_t)(uintptr_t)what;
    unsigned k;
    for (k = 0; k < nseen; ++k) if (seen[k] == key) break;
    if (k == nseen && nseen < 16) {
        seen[nseen++] = key;
        SLOG("UserStats %s %s -> 0", what, refused ? "REFUSED (never forwarded)" : "not wired (answered 0)");
    }
    cpu->EAX = 0;
    cpu->EDX = 0;
}
#define ZERO_H(fname, what, refused) \
    static void fname(CpuState *restrict cpu) { zero_answer(cpu, what, refused); }
ZERO_H(h_lb_54, "FindOrCreateLeaderboard(+0x54)", 0)
ZERO_H(h_lb_58, "FindLeaderboard(+0x58)", 0)
ZERO_H(h_lb_5c, "GetLeaderboardName(+0x5c)", 0)
ZERO_H(h_lb_60, "GetLeaderboardEntryCount(+0x60)", 0)
ZERO_H(h_lb_64, "GetLeaderboardSortMethod(+0x64)", 0)
ZERO_H(h_lb_6c, "DownloadLeaderboardEntries(+0x6c)", 0)
ZERO_H(h_lb_74, "GetDownloadedLeaderboardEntry(+0x74)", 0)
/* +0x50 ResetAllStats(bool achievementsToo): the game calls it with true when the
 * "version" stat differs from 1000 (found by the first stub run, caller 0x009aa878).
 * On a real account that wipes stats AND achievements, so it is refused. */
ZERO_H(h_reset, "ResetAllStats(+0x50)", 1)
ZERO_H(h_lb_78, "UploadLeaderboardScore(+0x78)", 1)
ZERO_H(h_lb_7c, "AttachLeaderboardUGC(+0x7c)", 1)

/* Every vtable slot nobody observed.  Purge unknown -> 0; logged with the
 * caller so a first live boot closes steam-bridge.md 7.5. */
static void h_unmapped(CpuState *restrict cpu) {
    static unsigned c;
    if (c++ < 8) {
        uint32_t ra = (uint32_t)isaac_retaddr(cpu);
        SLOG("UserStats UNMAPPED slot called from 0x%08x -> 0 (purge 0, ESP may drift); "
             "code before: %02x %02x %02x %02x %02x %02x %02x %02x", (unsigned)ra,
             *(uint8_t *)isaac_g(ra - 8), *(uint8_t *)isaac_g(ra - 7), *(uint8_t *)isaac_g(ra - 6),
             *(uint8_t *)isaac_g(ra - 5), *(uint8_t *)isaac_g(ra - 4), *(uint8_t *)isaac_g(ra - 3),
             *(uint8_t *)isaac_g(ra - 2), *(uint8_t *)isaac_g(ra - 1));
    }
    cpu->EAX = 0;
    cpu->EDX = 0;
}

/* ISteamUser (SteamUser023, slot 0xc5c510).  The game's own "Steam is up" flag
 * (object 0xc5ab04, set in 0xa617e0) needs BOTH ContextInit(0xbf93c8) and
 * ContextInit(0xc5c510) to be non-NULL, and every achievement call is gated on that
 * flag, so ISteamUser cannot be the NULL arm when live (the first stub run showed
 * `achievement 2` never reaching SetAchievement).  One token, purge 4, serves the two
 * observed methods; the vtable offset is decoded from the caller's `call [eax+disp]`.
 *   +0x08 GetSteamID(hidden ptr): writes a zero CSteamID, returns the pointer
 *   +0x38 GetAuthTicketForWebApi(identity): REFUSED, answers 0 (invalid ticket), never forwarded
 * Every other slot points at the unmapped token (the game never calls them). */
static int call_off(uint32_t ra) {
    const uint8_t *p = (const uint8_t *)isaac_g(ra - 6);
    if (p[3] == 0xff && p[4] == 0x50) return p[5];                       /* call [eax+d8]  */
    if (p[0] == 0xff && p[1] == 0x90) return (int)(p[2] | p[3] << 8 | p[4] << 16 | (unsigned)p[5] << 24);
    if (p[4] == 0xff && p[5] == 0x10) return 0;                          /* call [eax]     */
    return -1;
}
static void h_user(CpuState *restrict cpu) {
    static unsigned c;
    int off = call_off((uint32_t)isaac_retaddr(cpu));
    uint32_t a0 = isaac_arg(cpu, 0);
    if (off == 0x08) {
        if (isaac_is_guest_va(a0) && isaac_is_guest_va(a0 + 7u)) { isaac_w32(a0, 0); isaac_w32(a0 + 4u, 0); }
        cpu->EAX = a0;
        if (c++ < 4) SLOG("ISteamUser GetSteamID -> 0 (not forwarded)");
    } else {
        if (c++ < 4) SLOG("ISteamUser +0x%02x %s -> 0 (not forwarded)", off,
                          off == 0x38 ? "GetAuthTicketForWebApi REFUSED" : "UNMAPPED");
        cpu->EAX = 0;
    }
    cpu->EDX = 0;
}

typedef struct { uint32_t off; uint16_t purge; const char *name; isaac_shim_fn fn; } us_method;
static const us_method k_methods[] = {
    { 0x04,  8, "GetStat",                      h_get_stat },
    { 0x0c,  8, "SetStat",                      h_set_stat },
    { 0x14,  8, "GetAchievement",               h_get_ach  },
    { 0x18,  4, "SetAchievement",               h_set_ach  },
    { 0x24,  0, "StoreStats",                   h_store    },
    { 0x30, 12, "IndicateAchievementProgress",  h_indicate },
    { 0xac, 12, "GetAchievementProgressLimits", h_limits   },
    { 0x50,  4, "ResetAllStats",                h_reset    },
    { 0x54, 12, "FindOrCreateLeaderboard",     h_lb_54    },
    { 0x58,  4, "FindLeaderboard",              h_lb_58    },
    { 0x5c,  8, "GetLeaderboardName",           h_lb_5c    },
    { 0x60,  8, "GetLeaderboardEntryCount",     h_lb_60    },
    { 0x64,  8, "GetLeaderboardSortMethod",     h_lb_64    },
    { 0x6c, 20, "DownloadLeaderboardEntries",   h_lb_6c    },
    { 0x74, 24, "GetDownloadedLeaderboardEntry", h_lb_74   },
    { 0x78, 24, "UploadLeaderboardScore",       h_lb_78    },
    { 0x7c, 16, "AttachLeaderboardUGC",         h_lb_7c    },
};
#define N_METHODS (sizeof k_methods / sizeof k_methods[0])

/* ------------------------------------------------------- token binding --- */

static isaac_import *g_tok[N_METHODS + 1];     /* last = unmapped */
static isaac_import *g_tok_user;               /* ISteamUser token */

/* Collect N_METHODS + 2 re-bindable rows; touch nothing until all are found. */
static int pool_bind(void) {
    isaac_import *pool[N_METHODS + 2];
    unsigned np = 0;
    for (int pass = 0; pass < 2; ++pass) {
        for (unsigned i = 0; i < isaac_import_count && np < N_METHODS + 2; ++i) {
            isaac_import *imp = &isaac_imports[i];
            if (imp->shim_va < ISAAC_SHIM_BASE || !imp->fn) continue;
            if (pass == 0) {
                if (strcmp(imp->dll, "steam_api.dll") == 0 &&
                    strncmp(imp->symbol, "CSteamAPIContext_", 17) == 0)
                    pool[np++] = imp;
            } else if (imp->verdict == ISAAC_V_NEVER_CALLED && imp->call_sites == 0 &&
                       imp->iat_slot_va != 0 &&
                       (strncmp(imp->symbol, "luaopen_", 8) == 0 ||
                        strcmp(imp->dll, "vcruntime140.dll") == 0 ||
                        strcmp(imp->symbol, "DefWindowProcA") == 0)) {
                pool[np++] = imp;
            }
        }
    }
    if (np < N_METHODS + 2) return 0;
    for (unsigned k = 0; k < N_METHODS; ++k) {
        pool[k]->fn = k_methods[k].fn;
        pool[k]->arg_bytes = k_methods[k].purge;
        g_tok[k] = pool[k];
    }
    pool[N_METHODS]->fn = h_unmapped;
    pool[N_METHODS]->arg_bytes = 0;
    g_tok[N_METHODS] = pool[N_METHODS];
    pool[N_METHODS + 1]->fn = h_user;
    pool[N_METHODS + 1]->arg_bytes = 4;
    g_tok_user = pool[N_METHODS + 1];
    return 1;
}

static void build_vtable(void) {
    for (unsigned j = 0; j < US_VTBL_SLOTS; ++j)
        isaac_w32(US_VTBL_VA + 4u * j, g_tok[N_METHODS]->shim_va);
    for (unsigned k = 0; k < N_METHODS; ++k)
        isaac_w32(US_VTBL_VA + k_methods[k].off, g_tok[k]->shim_va);
    isaac_w32(US_OBJ_VA, US_VTBL_VA);
    for (unsigned j = 0; j < USR_VTBL_SLOTS; ++j)
        isaac_w32(USR_VTBL_VA + 4u * j, g_tok[N_METHODS]->shim_va);
    isaac_w32(USR_VTBL_VA + 0x08u, g_tok_user->shim_va);
    isaac_w32(USR_VTBL_VA + 0x38u, g_tok_user->shim_va);
    isaac_w32(USR_OBJ_VA, USR_VTBL_VA);
}

/* ISAAC_STEAM_SELFTEST=1: drives the live tokens through a fabricated stack
 * (name string, out cell, return address) and checks result and purge, so the
 * stub run shows [stub-steam] SetAchievement/StoreStats without playing.  Use
 * with ISAAC_STEAM_DRYRUN=1 against a real Steam client. */
static void selftest(void) {
    const uint32_t str = ST_SCRATCH_VA + 0x100u, out = ST_SCRATCH_VA + 0x180u;
    const uint32_t stk = ST_SCRATCH_VA + 0x200u;
    const char *nm = "ISAAC_SELFTEST_NOOP";
    for (size_t i = 0; i <= strlen(nm); ++i) w8(str + (uint32_t)i, (uint8_t)nm[i]);
    for (unsigned k = 0; k < N_METHODS; ++k) {
        uint32_t off = k_methods[k].off;
        if (off != 0x14 && off != 0x18 && off != 0x24) continue;
        CpuState c;
        memset(&c, 0, sizeof c);
        c.ESP = stk;
        isaac_w32(stk, 0x00401000u);          /* return address */
        isaac_w32(stk + 4, off == 0x24 ? 0 : str);
        isaac_w32(stk + 8, out);
        isaac_w32(out, 0xffffffffu);
        isaac_import *t = isaac_resolve_shim(isaac_r32(US_VTBL_VA + off));
        if (!t) { SLOG("selftest: vtable +0x%02x does not resolve to a token", off); continue; }
        t->fn(&c);
        SLOG("selftest: %s via vtable +0x%02x -> eax=%u out=0x%08x purge=%u (want %u)",
             k_methods[k].name, off, (unsigned)c.EAX, (unsigned)isaac_r32(out),
             (unsigned)t->arg_bytes, (unsigned)k_methods[k].purge);
    }
}

/* ----------------------------------------------------------- bring-up ---- */

static void bridge_up(void) {
    if (S.state != 0) return;
    const char *e;
    /* Opt-in until M9 is verified against the stub (steam-bridge.md section 8): a live
     * bridge registers the process with the user's Steam client as app 250900 and can
     * write achievements to the real account. ISAAC_STEAM=1 turns it on. */
    if (!(e = getenv("ISAAC_STEAM")) || e[0] != '1') { disable("off by default; ISAAC_STEAM=1 turns it on"); return; }
    S.dryrun = (e = getenv("ISAAC_STEAM_DRYRUN")) && e[0] == '1';

    char why[1200];
    S.h = open_dylib(why, sizeof why);
    if (!S.h) { disable("%s", why); return; }

    fn_InitFlat InitFlat = (fn_InitFlat)sym("SteamAPI_InitFlat");
    fn_FindIface Find = (fn_FindIface)sym("SteamInternal_FindOrCreateUserInterface");
    if (!InitFlat || !Find) { disable("SteamAPI_InitFlat / SteamInternal_FindOrCreateUserInterface missing"); return; }
    fn_IsSteamRunning running = (fn_IsSteamRunning)sym("SteamAPI_IsSteamRunning");
    if (running && !running()) { disable("Steam is not running"); return; }

    /* Hand launch: the env vars are enough; steam_appid.txt is never written.
     * SteamAPI_RestartAppIfNecessary is deliberately never resolved. */
    if (!getenv("SteamAppId"))  setenv("SteamAppId",  "250900", 0);
    if (!getenv("SteamGameId")) setenv("SteamGameId", "250900", 0);

    char err[1024] = {0};
    int32_t r = InitFlat(err);
    if (r != 0) { disable("SteamAPI_InitFlat failed (%d) %s", (int)r, err); return; }
    S.Shutdown = (fn_void)sym("SteamAPI_Shutdown");

    fn_GetHSteamUser gu = (fn_GetHSteamUser)sym("SteamAPI_GetHSteamUser");
    S.us = Find(gu ? gu() : 0, US_VERSION);
    if (!S.us) {
        fn_Accessor acc = (fn_Accessor)sym("SteamAPI_SteamUserStats_v013");
        S.us = acc ? acc() : NULL;
    }
    if (!S.us) {
        if (S.Shutdown) S.Shutdown();
        disable("ISteamUserStats 013 unavailable");
        return;
    }
    S.GetAch   = (fn_GetAch)sym("SteamAPI_ISteamUserStats_GetAchievement");
    S.SetAch   = (fn_Name)sym("SteamAPI_ISteamUserStats_SetAchievement");
    S.Store    = (fn_Void)sym("SteamAPI_ISteamUserStats_StoreStats");
    S.GetStatI = (fn_GetStatI)sym("SteamAPI_ISteamUserStats_GetStatInt32");
    S.SetStatI = (fn_SetStatI)sym("SteamAPI_ISteamUserStats_SetStatInt32");
    S.Indicate = (fn_Indicate)sym("SteamAPI_ISteamUserStats_IndicateAchievementProgress");
    S.LimitsI  = (fn_Limits)sym("SteamAPI_ISteamUserStats_GetAchievementProgressLimitsInt32");
    /* Intentionally NOT resolved: SteamAPI_RestartAppIfNecessary, every
     * SteamAPI_ISteamRemoteStorage_*, SteamAPI_ISteamUser_GetAuthTicketForWebApi,
     * SteamAPI_ISteamUserStats_{UploadLeaderboardScore,AttachLeaderboardUGC}. */
    if (!S.SetAch || !S.Store || !S.GetAch) {
        if (S.Shutdown) S.Shutdown();
        disable("ISteamUserStats flat entries missing");
        return;
    }
    if (!pool_bind()) {
        if (S.Shutdown) S.Shutdown();
        disable("no re-bindable shim tokens");
        return;
    }
    build_vtable();
    S.state = 1;
    if (S.Shutdown) atexit((void (*)(void))S.Shutdown);
    SLOG("live (app %s, dryrun=%d)", getenv("SteamAppId"), S.dryrun);
    if ((e = getenv("ISAAC_STEAM_SELFTEST")) && e[0] == '1') selftest();
}

int mac_steam_is_live(void) { bridge_up(); return S.state == 1; }

int mac_steam_context_init(CpuState *restrict cpu, uint32_t slot) {
    bridge_up();
    if (S.state != 1 || !isaac_is_guest_va(slot) || !isaac_is_guest_va(slot + 8u)) return 0;
    static uint32_t seen[32]; static unsigned nseen;
    unsigned k;
    for (k = 0; k < nseen; ++k) if (seen[k] == slot) break;
    int first = (k == nseen && nseen < 32);
    if (first) seen[nseen++] = slot;
    if (slot == SLOT_USERSTATS) {
        /* initData {pFn, counter, ptr}: counter at +4, interface at +8, return +8 */
        isaac_w32(slot + 4u, 1u);
        isaac_w32(slot + 8u, US_OBJ_VA);
        cpu->EAX = slot + 8u;
        if (first) SLOG("ContextInit(slot=0x%08x) -> live ISteamUserStats013", (unsigned)slot);
    } else if (slot == SLOT_USER) {
        isaac_w32(slot + 4u, 1u);
        isaac_w32(slot + 8u, USR_OBJ_VA);
        cpu->EAX = slot + 8u;
        if (first) SLOG("ContextInit(slot=0x%08x) -> live ISteamUser (GetSteamID 0, auth ticket refused)", (unsigned)slot);
    } else {
        /* the game's own no-Steam arm, byte-for-byte what host_shims_steam.c does */
        isaac_w32(slot, 0);
        cpu->EAX = slot;
        if (first) SLOG("ContextInit(slot=0x%08x) -> NULL interface (not wired)", (unsigned)slot);
    }
    return 1;
}

#endif /* ISAAC_NATIVE */
