/* mac_main.c -- the Mac Build's headless driver.
 *
 * Does natively what scripts/recomp/lift/boot_integration.mjs does under
 * Node: reserve the Guest region, place the memory image, seed the game's
 * files into the Host file shim, run the boot path, then main.
 *
 *   isaac-mac [--headless] [--launch-settings <json>] [--vanilla] [--debug-console] [--lua-debug]
 *             [--segs F] [--bundle D] [--layer-dir D] [--save-dir D] [--mods-dir D]
 *             [--modscan F] [--gate-log F] [--stage boot|main]
 *   isaac-mac [--headless] <isaac.segs.bin> <instance dir> [boot|main] [save dir]   (dev form)
 *
 * Precedence: flag > launch-settings JSON > REPENTANCE_MAC_* env > default
 * (segs, bundle and layer default to <exe dir>/../Resources/...). Env:
 * REPENTANCE_MAC_LAUNCH_SETTINGS, ISAAC_MAC_HEADLESS, ISAAC_MAC_RUN_FRAMES=N
 * (watchdog: guard check and exit 0 after N presented frames), ISAAC_MAC_SHOT,
 * ISAAC_MAC_SCRIPT (see mac_gl.c, mac_script.c).
 *
 * The bundle (instance dir) is the bundle (the archives in resources/packed, the Lua
 * in resources/scripts, savedatapath.txt). It also becomes the working
 * directory, because the Host Lua loads its scripts relative to it.
 */
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#include "isaac_host.h"
#include "mac_launch.h"

int isaac_native_reserve(void);
int isaac_place_image(const uint8_t *blob, uint32_t len);
int isaac_layout_check(void);
void isaac_guard_arm(void);
uint32_t isaac_guard_check(void);
int isaac_run_boot(int verbose);
int isaac_run_main(void);
int isaac_fs_seed_adopt(const char *path, uint8_t *data, uint32_t len);
int isaac_fs_seed_lazy(const char *path, uint32_t len);

uint32_t isaac_frames_presented(void);
extern const char *g_mac_instance_dir;
extern const char *g_mac_save_dir;
extern const char *g_mac_mods_dir, *g_mac_layer_dir, *g_mac_modscan_path, *g_mac_gate_log_path;
extern int g_mac_layer_off;
int mac_seed_mods(void);
unsigned mac_mods_file_count(void);
void isaac_mac_gate_log_off(const char *reason);

/* Boot status lines for the Launcher (it reads the game's stdout):
 *   [mac][boot] status: <text>     a phase change always prints; others at most ~4 a second
 *   [mac][boot] ready              once, at the first presented frame (mac_gl.c)
 * After "ready" no more status lines are printed. */
static int g_boot_ready;
void mac_boot_status(const char *text, int force) {
    static uint64_t last;
    if (g_boot_ready) return;
    uint64_t now = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
    if (!force && last && now - last < 250000000ull) return;
    last = now;
    printf("[mac][boot] status: %s\n", text);
    fflush(stdout);              /* the Launcher shows these live: never wait for the log's flush timer */
}
void mac_boot_ready(void) {
    if (g_boot_ready) return;
    g_boot_ready = 1;
    printf("[mac][boot] ready\n");
    fflush(stdout);
}

static int is_dir_path(const char *p) { struct stat st; return stat(p, &st) == 0 && S_ISDIR(st.st_mode); }
static void mkdirs_path(const char *path) {
    char tmp[4096];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = tmp + 1; *p; ++p)
        if (*p == '/') { *p = 0; mkdir(tmp, 0755); *p = '/'; }
    mkdir(tmp, 0755);
}

static uint8_t *read_all(const char *path, uint32_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(n > 0 ? (size_t)n : 1);
    if (b && n > 0 && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = NULL; }
    fclose(f);
    if (b) *len = (uint32_t)n;
    return b;
}

static const char *BOOT_ARCHIVES[] = { "graphics.a", "config.a", "fonts.a", "animations.a", "rooms.a", "sfx.a" };
static const char *LAZY_ARCHIVES[] = { "music.a", "videos.a", "afterbirth.a", "afterbirthp.a" };

static int seed_archives(void) {
    int n = 0;
    char path[4096], rel[256];
    for (size_t i = 0; i < sizeof BOOT_ARCHIVES / sizeof *BOOT_ARCHIVES; ++i) {
        snprintf(rel, sizeof rel, "resources/packed/%s", BOOT_ARCHIVES[i]);
        snprintf(path, sizeof path, "%s/%s", g_mac_instance_dir, rel);
        uint32_t len = 0;
        uint8_t *b = read_all(path, &len);
        if (!b) { printf("  (skip %s: not present)\n", rel); continue; }
        int ok = isaac_fs_seed_adopt(rel, b, len);   /* the shim owns b on success */
        if (!ok) free(b);
        printf("  seed %s %u bytes -> %s\n", rel, len, ok ? "ok" : "FAIL");
        n += ok;
    }
    for (size_t i = 0; i < sizeof LAZY_ARCHIVES / sizeof *LAZY_ARCHIVES; ++i) {
        snprintf(rel, sizeof rel, "resources/packed/%s", LAZY_ARCHIVES[i]);
        snprintf(path, sizeof path, "%s/%s", g_mac_instance_dir, rel);
        struct stat st;
        if (stat(path, &st) != 0) { printf("  (skip %s: not present)\n", rel); continue; }
        int ok = isaac_fs_seed_lazy(rel, (uint32_t)st.st_size);
        printf("  seed %s %lld bytes (lazy) -> %s\n", rel, (long long)st.st_size, ok ? "ok" : "FAIL");
        n += ok;
    }
    return n;
}

/* Every other file of the instance is registered by size and read on first
 * open, as the Node driver does; packed archives and mods are skipped. */
static int ends_with_ci(const char *s, const char *suf) {
    size_t a = strlen(s), b = strlen(suf);
    return a >= b && strcasecmp(s + a - b, suf) == 0;
}
static void walk(const char *dir, const char *rel, int *files) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char p[4096], r[4096];
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        snprintf(r, sizeof r, rel[0] ? "%s/%s" : "%s%s", rel, e->d_name);
        struct stat st;
        if (stat(p, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (!strcmp(r, "resources/packed") || !strcmp(r, "packed") || !strcmp(r, "mods")) continue;
            walk(p, r, files);
        } else if (S_ISREG(st.st_mode)) {
            if (ends_with_ci(r, ".exe") || ends_with_ci(r, ".dll") || ends_with_ci(r, ".so") || ends_with_ci(r, ".ogv")) continue;
            if (isaac_fs_seed_lazy(r, (uint32_t)st.st_size)) ++*files;
        }
    }
    closedir(d);
}

static void restore_saves(const char *dir, const char *rel, int *n) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char p[4096], r[4096];
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        snprintf(r, sizeof r, rel[0] ? "%s/%s" : "%s%s", rel, e->d_name);
        struct stat st;
        if (stat(p, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { restore_saves(p, r, n); continue; }
        uint32_t len = 0;
        uint8_t *b = read_all(p, &len);
        if (b && isaac_fs_seed_adopt(r, b, len)) { ++*n; printf("  restored %s (%u bytes)\n", r, len); }
        else free(b);
    }
    closedir(d);
}

/* ISAAC_MAC_RUN_FRAMES=N: a watchdog thread ends the run cleanly once N frames
 * have been presented, after checking the guard canary, so a headless run has
 * a deterministic stop and a verdict. Unset: the game runs until it exits. */
static void *frame_watchdog(void *arg) {
    uint32_t want = (uint32_t)(uintptr_t)arg;
    while (isaac_frames_presented() < want) usleep(20000);
    uint32_t bad = isaac_guard_check();
    { extern void isaac_mac_log_finish(void); isaac_mac_log_finish(); }   /* the repeat totals come before the verdict line */
    printf("[mac] reached %u frames (wanted %u); guard %s\n", isaac_frames_presented(), want,
           bad ? "CORRUPTED" : "intact");
    { extern void isaac_mac_log_finish(void); isaac_mac_log_finish(); }
    fflush(NULL);
    _exit(bad ? 3 : 0);
    return NULL;
}

void isaac_mac_gl_install_fakes(void);
int isaac_mac_headless(void);

int main(int argc, char **argv) {
    { extern void isaac_mac_log_setup(void); isaac_mac_log_setup(); }   /* buffered log, flushed every 100 ms (mac_log.c) */
    /* --headless anywhere on the line is ISAAC_MAC_HEADLESS=1 */
    for (int i = 1; i < argc; ++i)
        if (!strcmp(argv[i], "--headless")) {
            setenv("ISAAC_MAC_HEADLESS", "1", 1);
            for (int j = i; j < argc; ++j) argv[j] = argv[j + 1];
            --argc;
            --i;
        }
    /* the driver chdirs into the instance; relative output paths (screenshots) are
     * meant relative to where the user ran it */
    { char cwd[4096]; if (getcwd(cwd, sizeof cwd)) setenv("ISAAC_MAC_LAUNCH_CWD", cwd, 1); }
    if (!mac_launch_parse(argc, argv)) return 2;
    { extern void isaac_mac_window_adapt_begin(void); isaac_mac_window_adapt_begin(); }   /* adapt: the fake display (mac_window.c) */
    if (!g_mac_cfg.segs || !g_mac_cfg.bundle) {
        fprintf(stderr, "usage: %s [--headless] [--launch-settings <json>] [--vanilla] [--debug-console] [--lua-debug]\n"
                        "          [--segs F] [--bundle D] [--layer-dir D] [--save-dir D] [--mods-dir D] [--modscan F] [--gate-log F] [--stage boot|main]\n"
                        "       %s [--headless] <isaac.segs.bin> <instance dir> [boot|main] [save dir]     (dev)\n"
                        "  segs, bundle and layer default to <exe dir>/../Resources/{isaac.segs.bin,game-bundle,layer}\n"
                        "  windowed by default (SDL3 + ANGLE on Metal); --headless or ISAAC_MAC_HEADLESS=1: no window, fake GL\n",
                argv[0], argv[0]);
        return 2;
    }
    const char *segs = g_mac_cfg.segs;
    const char *stage = g_mac_cfg.stage;
    /* The driver chdirs into the instance, so every dir must be absolute (mac_launch_parse does it). */
    g_mac_instance_dir = g_mac_cfg.bundle;
    if (!is_dir_path(g_mac_instance_dir)) { fprintf(stderr, "[mac] bundle %s is not a directory\n", g_mac_instance_dir); return 1; }
    g_mac_save_dir = NULL;
    if (g_mac_cfg.save_dir) {
        mkdirs_path(g_mac_cfg.save_dir);
        g_mac_save_dir = g_mac_cfg.save_dir;
    }
    g_mac_mods_dir = g_mac_cfg.mods_dir;
    g_mac_layer_off = g_mac_cfg.vanilla;
    g_mac_modscan_path = g_mac_cfg.modscan_path;
    g_mac_gate_log_path = g_mac_cfg.gate_log_path;
    g_mac_layer_dir = g_mac_cfg.layer_dir;
    printf("[mac] launch: segs %s\n[mac] launch: bundle %s\n[mac] launch: saves %s\n[mac] launch: mods %s\n",
           segs, g_mac_instance_dir, g_mac_save_dir ? g_mac_save_dir : "(none: nothing persists)", g_mac_mods_dir ? g_mac_mods_dir : "(none)");
    if (g_mac_cfg.launch_settings) printf("[mac] launch: settings %s\n", g_mac_cfg.launch_settings);
    printf("[mac] launch: vanilla=%d debug_console=%d lua_debug=%d\n", g_mac_cfg.vanilla, g_mac_cfg.debug_console, g_mac_cfg.lua_debug);
    /* The game's QueryPerformanceCounter: the real clock in a windowed run (the Theora video
     * player times its frames with it, and the audio runs on the real clock, so a deterministic
     * clock - one tick per query plus each Sleep - played the intro far too fast). Headless runs
     * keep the deterministic clock so they repeat exactly. ISAAC_MAC_CLOCK=wall|deterministic overrides. */
    {
        extern void isaac_time_set_mode(int mode);
        const char *c = getenv("ISAAC_MAC_CLOCK");
        int wall = c && *c ? (strcmp(c, "wall") == 0) : !isaac_mac_headless();
        isaac_time_set_mode(wall);
    }
    /* Layer: built into the app, never listed as a mod. Vanilla Launch turns it off through
     * the one switch host_rgon.c honours (ISAAC_RGON=0) and the file layer stops serving it. */
    if (g_mac_cfg.vanilla) {
        setenv("ISAAC_RGON", "0", 1);
        printf("[mac] Vanilla Launch: the Layer is off (ISAAC_RGON=0), no mod sees REPENTOGON\n");
        isaac_mac_gate_log_off("vanilla launch");
    } else if (!g_mac_layer_dir || !is_dir_path(g_mac_layer_dir)) {
        printf("[mac] the Layer is not installed (no layer dir: %s): no REPENTOGON support this launch\n", g_mac_layer_dir ? g_mac_layer_dir : "unset");
        g_mac_layer_dir = NULL;
        isaac_mac_gate_log_off("layer not installed");
    } else {
        printf("[mac] Layer served from %s as mods/repentogon/ (not listed as a mod); modscan.lua from %s\n", g_mac_layer_dir,
               g_mac_modscan_path ? g_mac_modscan_path : "(none)");
    }

    mac_boot_status("Loading the game", 1);
    if (!isaac_native_reserve()) return 1;
    printf("[mac] Guest region at %p\n", (void *)recomp_mem_base);

    uint32_t len = 0;
    uint8_t *blob = read_all(segs, &len);
    if (!blob) { fprintf(stderr, "cannot read %s\n", segs); return 1; }
    int nseg = isaac_place_image(blob, len);
    free(blob);
    printf("[mac] placed memory image: %d segment(s)\n", nseg);
    if (nseg <= 0) return 1;

    int layout_bad = isaac_layout_check();
    printf("[mac] layout check: %s\n", layout_bad ? "MISMATCH" : "ok");
    isaac_guard_arm();

    mac_boot_status("Reading packed archives", 1);
    printf("[mac] seed packed archives\n");
    seed_archives();
    int files = 0;
    walk(g_mac_instance_dir, "", &files);
    printf("[mac] registered %d loose file(s) lazily\n", files);
    if (g_mac_save_dir) {
        int n = 0;
        restore_saves(g_mac_save_dir, "", &n);
        printf("[mac] %d saved file(s) restored from %s\n", n, g_mac_save_dir);
    }
    mac_seed_mods();
    { char t[64]; snprintf(t, sizeof t, "Reading mods (%u files)", mac_mods_file_count()); mac_boot_status(t, 1); }
    if (chdir(g_mac_instance_dir) != 0) perror("[mac] chdir to the instance");

    if (isaac_mac_headless()) {
        isaac_mac_gl_install_fakes();      /* no window, no ANGLE: the fake GL answers */
        printf("[mac] headless: no window\n");
    }
    mac_boot_status("Starting the game", 1);
    int boot_rc = isaac_run_boot(1);
    uint32_t g = isaac_guard_check();
    printf("[mac] isaac_run_boot -> %d, guard %s\n", boot_rc, g ? "CORRUPTED" : "intact");
    if (strcmp(stage, "boot") == 0) return boot_rc == 1 ? 0 : 1;

    const char *wf = getenv("ISAAC_MAC_RUN_FRAMES");
    if (wf && atoi(wf) > 0) {
        pthread_t t;
        if (pthread_create(&t, NULL, frame_watchdog, (void *)(uintptr_t)atoi(wf)) == 0) pthread_detach(t);
    }
    int main_rc = isaac_run_main();
    printf("[mac] main returned %d\n", main_rc);
    return main_rc == 0 ? 0 : 1;
}
