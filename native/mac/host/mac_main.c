/* mac_main.c -- the Mac Build's headless driver.
 *
 * Does natively what scripts/recomp/lift/boot_integration.mjs does under
 * Node: reserve the Guest region, place the memory image, seed the game's
 * files into the Host file shim, run the boot path, then main.
 *
 *   isaac-mac <isaac.segs.bin> <instance dir> [boot|main] [save dir]
 *
 * The instance dir is the bundle (the archives in resources/packed, the Lua
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

#include "isaac_host.h"

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
    printf("[mac] reached %u frames (wanted %u); guard %s\n", isaac_frames_presented(), want,
           bad ? "CORRUPTED" : "intact");
    fflush(NULL);
    _exit(bad ? 3 : 0);
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <isaac.segs.bin> <instance dir> [boot|main] [save dir]\n", argv[0]);
        return 2;
    }
    const char *segs = argv[1];
    const char *stage = argc > 3 ? argv[3] : "main";
    /* The driver chdirs into the instance, so both dirs must be absolute. */
    static char inst_abs[4096], save_abs[4096];
    if (!realpath(argv[2], inst_abs)) { perror("[mac] instance dir"); return 1; }
    g_mac_instance_dir = inst_abs;
    g_mac_save_dir = NULL;
    if (argc > 4) {
        mkdir(argv[4], 0755);
        if (!realpath(argv[4], save_abs)) { perror("[mac] save dir"); return 1; }
        g_mac_save_dir = save_abs;
    }

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
    if (chdir(g_mac_instance_dir) != 0) perror("[mac] chdir to the instance");

    setvbuf(stdout, NULL, _IOLBF, 0);
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
