/* mac_fs.c -- the file callbacks host_shims_fs.c makes, as real file I/O.
 *
 * In the Browser Build these are JavaScript (Module.isaacLazyRead and friends,
 * set up by scripts/recomp/lift/boot_integration.mjs or the page). Here they
 * read the game folder (g_mac_instance_dir) and write saves to
 * g_mac_save_dir, with the same path rules as the Node driver. Three more things
 * live here (docs/mac/launcher.md, "Game side"):
 *
 *  - Mods. The mods folder (g_mac_mods_dir, a real directory) is seeded into the
 *    Guest's file table lazily under both roots the game scans (mods/ and
 *    documents/my games/binding of isaac repentance+/mods/), as the Browser
 *    Build's seedMods does. A folder with disable.it is not seeded at all (the
 *    engine does not read the file back, mods.mjs). A bytes read goes to the real
 *    file; the only write that reaches the mods folder is disable.it, which the
 *    in-game MODS menu creates and deletes, exactly like the PC.
 *  - The Layer. mods/repentogon/<file> is served from g_mac_layer_dir (and
 *    modscan.lua from g_mac_modscan_path) through isaac_fs_peek's fallback
 *    (isaac_mac_layer_peek), without ever entering the file table: it is not listed
 *    as a mod (ADR 0003).
 *  - Saves are written atomically (temp file, fsync, rename) the moment the game
 *    closes the file, so a kill or a crash cannot leave a half-written save.
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

const char *g_mac_instance_dir = ".";
const char *g_mac_save_dir;          /* NULL: nothing the game writes persists */
const char *g_mac_mods_dir;          /* NULL: no mods */
const char *g_mac_layer_dir;         /* NULL: no Layer */
const char *g_mac_modscan_path;      /* NULL: the Layer finds no modscan.lua */
int g_mac_layer_off;                 /* Vanilla Launch */

int isaac_fs_seed_lazy(const char *path, uint32_t len);

static void join(char *out, size_t cap, const char *dir, const char *rel) {
    snprintf(out, cap, "%s/%s", dir, rel);
    for (char *p = out; *p; ++p) if (*p == '\\') *p = '/';
}

#define USER_MODS "documents/my games/binding of isaac repentance+/mods/"

/* rel is a Guest-relative path: the part after a mods root, or NULL */
static const char *mods_rel(const char *rel) {
    if (strncasecmp(rel, "mods/", 5) == 0) return rel + 5;
    if (strncasecmp(rel, USER_MODS, sizeof USER_MODS - 1) == 0) return rel + sizeof USER_MODS - 1;
    return NULL;
}

/* the path of a seed (a lazy entry's src) on this machine */
static void resolve_src(char *out, size_t cap, const char *src) {
    const char *m = g_mac_mods_dir ? mods_rel(src) : NULL;
    if (m) join(out, cap, g_mac_mods_dir, m);
    else join(out, cap, g_mac_instance_dir, src);
}

int isaac_fs_lazy_read_js(const char *src, uint8_t *dst, uint32_t len) {
    char path[4096];
    resolve_src(path, sizeof path, src);
    int fd = open(path, O_RDONLY);
    if (fd < 0) { fprintf(stderr, "[isaac][mac] lazy read FAILED for %s: %s\n", src, strerror(errno)); return 0; }
    uint32_t got = 0;
    while (got < len) {
        ssize_t n = read(fd, dst + got, len - got);
        if (n <= 0) break;
        got += (uint32_t)n;
    }
    close(fd);
    if (got < len) { fprintf(stderr, "[isaac][mac] lazy read of %s: %u bytes on disk, %u registered\n", src, got, len); return 0; }
    return 1;
}

/* Ranged reads keep one descriptor per file open, as the Node driver does. */
static struct { char *src; int fd; } g_fds[64];
static unsigned g_nfds;

int isaac_fs_lazy_pread_avail(void) { return 1; }

int isaac_fs_lazy_pread_js(const char *src, uint8_t *dst, uint32_t off, uint32_t len) {
    int fd = -1;
    for (unsigned i = 0; i < g_nfds; ++i)
        if (strcmp(g_fds[i].src, src) == 0) { fd = g_fds[i].fd; break; }
    if (fd < 0) {
        char path[4096];
        resolve_src(path, sizeof path, src);
        fd = open(path, O_RDONLY);
        if (fd < 0) { fprintf(stderr, "[isaac][mac] pread open FAILED for %s: %s\n", src, strerror(errno)); return -1; }
        if (g_nfds < 64) { g_fds[g_nfds].src = strdup(src); g_fds[g_nfds].fd = fd; ++g_nfds; }
    }
    ssize_t n = pread(fd, dst, len, (off_t)off);
    return n < 0 ? -1 : (int)n;
}

/* The save store keys a file by the path the game wrote it under, less the
 * fake "c:/isaac/" root (boot_integration.mjs storeRel). */
static const char *store_rel(const char *key, const char *src) {
    if (src && *src) return src;
    return strncmp(key, "c:/isaac/", 9) == 0 ? key + 9 : key;
}

static void mkdirs_for(const char *path) {
    char tmp[4096];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = tmp + 1; *p; ++p)
        if (*p == '/') { *p = 0; mkdir(tmp, 0755); *p = '/'; }
}

/* write beside, flush to the disk, then rename over: the old file or the new one, never a mix */
static int write_atomic(const char *path, const uint8_t *data, uint32_t len) {
    char tmp[4200];
    snprintf(tmp, sizeof tmp, "%s.tmp.%d", path, (int)getpid());
    FILE *f = fopen(tmp, "wb");
    if (!f) return 0;
    int ok = (len == 0 || fwrite(data, 1, len, f) == len) && fflush(f) == 0 && fsync(fileno(f)) == 0;
    if (fclose(f) != 0) ok = 0;
    if (ok && rename(tmp, path) != 0) ok = 0;
    if (!ok) unlink(tmp);
    return ok;
}

/* The game's key is lower case; the folder on disk keeps its own case. A mod
 * folder is matched case-insensitively. rest = "<mod>/disable.it". */
static int real_mod_path(char *out, size_t cap, const char *rest) {
    const char *slash = strchr(rest, '/');
    if (!slash || strcasecmp(slash + 1, "disable.it") != 0 || slash == rest) return 0;
    char name[512];
    size_t n = (size_t)(slash - rest);
    if (n >= sizeof name) return 0;
    memcpy(name, rest, n); name[n] = 0;
    DIR *d = opendir(g_mac_mods_dir);
    if (!d) return 0;
    struct dirent *e;
    int found = 0;
    while ((e = readdir(d))) {
        if (e->d_name[0] != '.' && strcasecmp(e->d_name, name) == 0) {
            snprintf(out, cap, "%s/%s/disable.it", g_mac_mods_dir, e->d_name);
            found = 1;
            break;
        }
    }
    closedir(d);
    return found;
}

int isaac_fs_persist_js(const char *key, const char *src, const uint8_t *data, uint32_t len) {
    const char *rel = store_rel(key, src);
    const char *m = mods_rel(rel);
    if (m) {
        /* the mods folder is the user's: only the on/off flag is ever written there */
        char path[4096];
        if (!g_mac_mods_dir || !real_mod_path(path, sizeof path, m)) {
            static int said;
            if (said++ < 8) fprintf(stderr, "[isaac][mac] not written (only disable.it is written to the mods folder): %s\n", rel);
            return 0;
        }
        int ok = write_atomic(path, data, len);
        printf("[mac] mods: wrote %s (%s)\n", path, ok ? "mod off" : "FAILED");
        return ok;
    }
    if (!g_mac_save_dir) return 0;
    char path[4096];
    join(path, sizeof path, g_mac_save_dir, rel);
    mkdirs_for(path);
    if (!write_atomic(path, data, len)) { fprintf(stderr, "[isaac][mac] save write FAILED for %s: %s\n", key, strerror(errno)); return 0; }
    return 1;
}

int isaac_fs_unlink_js(const char *key, const char *src) {
    const char *rel = store_rel(key, src);
    const char *m = mods_rel(rel);
    if (m) {
        char path[4096];
        if (!g_mac_mods_dir || !real_mod_path(path, sizeof path, m)) return 0;
        int ok = unlink(path) == 0 || errno == ENOENT;
        printf("[mac] mods: removed %s (%s)\n", path, ok ? "mod on" : "FAILED");
        return ok;
    }
    if (!g_mac_save_dir) return 0;
    char path[4096];
    join(path, sizeof path, g_mac_save_dir, rel);
    return unlink(path) == 0 || errno == ENOENT;
}

/* ---- the mods folder, seeded ------------------------------------------------- */
static int has_disable_it(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return 0;
    struct dirent *e;
    int off = 0;
    while ((e = readdir(d))) if (strcasecmp(e->d_name, "disable.it") == 0) { off = 1; break; }
    closedir(d);
    return off;
}

static struct { unsigned folders, off, files, seeded, skipped_name, skipped_long, aliased; } g_ms;
extern int isaac_fs_seed_lazy_as(const char *path, const char *src, uint32_t len);
unsigned mac_mods_file_count(void) { return g_ms.files; }

/* dir: real folder; rel: path under the mods root ("<mod>/<...>") */
static void seed_tree(const char *dir, const char *rel) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char p[4096], r[4096];
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        snprintf(r, sizeof r, "%s/%s", rel, e->d_name);
        struct stat st;
        if (stat(p, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { seed_tree(p, r); continue; }
        if (!S_ISREG(st.st_mode)) continue;
        ++g_ms.files;
        char v1[4096], v2[4096];
        snprintf(v1, sizeof v1, "mods/%s", r);
        snprintf(v2, sizeof v2, USER_MODS "%s", r);
        /* the file table's keys are 255 characters here (host_shims_fs.c), with its "c:/isaac/" root */
        int a = 0, b = 0;
        if (strlen(v1) + 9 < 255) a = isaac_fs_seed_lazy(v1, (uint32_t)st.st_size); else ++g_ms.skipped_long;
        if (strlen(v2) + 9 < 255) b = isaac_fs_seed_lazy(v2, (uint32_t)st.st_size); else ++g_ms.skipped_long;
        g_ms.seeded += (unsigned)(a + b);
        /* content-repentogon: REPENTOGON reads it in place of content (its changelog v1.1.1); this game reads
         * only content/. With the Layer on, each file of a mod's content-repentogon/ is also listed as the
         * file of content/ unless content/ has its own (Fiend Folio ships only content-repentogon/, so
         * without this none of its items, entities or characters exist: Isaac.GetItemIdByName gives -1). */
        if (!g_mac_layer_off) {
            const char *slash = strchr(r, '/');
            if (slash && strncasecmp(slash + 1, "content-repentogon/", 19) == 0) {
                char own[4096], c1[4096], c2[4096];
                snprintf(own, sizeof own, "%s/%.*s/content/%s", g_mac_mods_dir, (int)(slash - r), r, slash + 20);
                struct stat cs;
                if (stat(own, &cs) != 0) {
                    snprintf(c1, sizeof c1, "mods/%.*s/content/%s", (int)(slash - r), r, slash + 20);
                    snprintf(c2, sizeof c2, USER_MODS "%.*s/content/%s", (int)(slash - r), r, slash + 20);
                    int x = 0, y = 0;
                    if (strlen(c1) + 9 < 255) x = isaac_fs_seed_lazy_as(c1, v1, (uint32_t)st.st_size);
                    if (strlen(c2) + 9 < 255) y = isaac_fs_seed_lazy_as(c2, v1, (uint32_t)st.st_size);
                    g_ms.seeded += (unsigned)(x + y);
                    ++g_ms.aliased;
                }
            }
        }
    }
    closedir(d);
}

static void seed_off_mod(const char *dir, const char *name) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strcasecmp(e->d_name, "metadata.xml") != 0 && strcasecmp(e->d_name, "disable.it") != 0) continue;
        char p[4096], r[4096], v1[4096];
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(p, &st) != 0 || !S_ISREG(st.st_mode)) continue;
        snprintf(r, sizeof r, "%s/%s", name, e->d_name);
        snprintf(v1, sizeof v1, "mods/%s", r);
        if (strlen(v1) + 9 < 255) isaac_fs_seed_lazy(v1, (uint32_t)st.st_size);   /* the mods/ root only, see above */
    }
    closedir(d);
}

int mac_seed_mods(void) {
    memset(&g_ms, 0, sizeof g_ms);
    if (!g_mac_mods_dir) return 0;
    DIR *d = opendir(g_mac_mods_dir);
    if (!d) { printf("[mac] mods: %s does not exist: no mods\n", g_mac_mods_dir); return 0; }
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char p[4096];
        snprintf(p, sizeof p, "%s/%s", g_mac_mods_dir, e->d_name);
        struct stat st;
        if (stat(p, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        if (strcasecmp(e->d_name, "repentogon") == 0) { ++g_ms.skipped_name; continue; }   /* the Layer's own folder name */
        if (has_disable_it(p)) {
            /* The game lists a mod with disable.it greyed and does not run its Lua, but only
             * metadata.xml and the flag are shown to it: nothing of an off mod can load.
             * Toggling it on in the MODS menu deletes disable.it (isaac_fs_unlink_js). */
            ++g_ms.off;
            seed_off_mod(p, e->d_name);
            continue;
        }
        ++g_ms.folders;
        seed_tree(p, e->d_name);
    }
    closedir(d);
    printf("[mac] mods: %s: %u folder(s) on, %u off (disable.it)%s; %u file(s), %u file-table entries seeded%s\n",
           g_mac_mods_dir, g_ms.folders, g_ms.off, g_ms.skipped_name ? ", a folder named repentogon ignored" : "",
           g_ms.files, g_ms.seeded, "");
    if (g_ms.aliased) printf("[mac] mods: %u file(s) of content-repentogon/ listed as content/ (REPENTOGON reads that folder first)\n", g_ms.aliased);
    if (g_ms.skipped_long)
        printf("[mac] mods: %u path(s) are too long for the file table's 255-character keys and were not seeded\n", g_ms.skipped_long);
    return (int)g_ms.folders;
}

/* luaL_loadfilex (host_lua.c) opens a mod's script with the C library, and the game
 * asks for "//mods/<dir>/main.lua": map it into the mods folder. Returns 0 for any
 * other path. */
int isaac_mac_lua_path(const char *path, char *out, size_t cap) {
    if (!g_mac_mods_dir || !path) return 0;
    while (*path == '/' || *path == '\\') ++path;
    if (strncasecmp(path, "mods/", 5) != 0 && strncasecmp(path, "mods\\", 5) != 0) return 0;
    path += 5;
    size_t n = (size_t)snprintf(out, cap, "%s/", g_mac_mods_dir);
    for (; *path && n + 1 < cap; ++path) {
        char c = *path == '\\' ? '/' : *path;
        if (c == '/' && out[n - 1] == '/') continue;
        out[n++] = c;
    }
    out[n] = 0;
    return 1;
}

/* ---- the Layer, served --------------------------------------------------------- */
static struct { char name[64]; uint8_t *data; uint32_t len; } g_layer[64];
static unsigned g_nlayer;

/* host_shims_fs.c isaac_fs_peek calls this when the file table has no such file. */
const uint8_t *isaac_mac_layer_peek(const char *key, uint32_t *len) {
    if (g_mac_layer_off || !key) return NULL;
    const char *rel = strncmp(key, "c:/isaac/", 9) == 0 ? key + 9 : key;
    const char *name = NULL;
    if (strncmp(rel, "mods/repentogon/", 16) == 0) name = rel + 16;
    else if (strncmp(rel, USER_MODS "repentogon/", sizeof USER_MODS + 10) == 0) name = rel + sizeof USER_MODS + 10;
    if (!name || !*name || strchr(name, '/') || strlen(name) >= sizeof g_layer[0].name) return NULL;
    for (unsigned i = 0; i < g_nlayer; ++i)
        if (strcmp(g_layer[i].name, name) == 0) { if (len) *len = g_layer[i].len; return g_layer[i].data ? g_layer[i].data : (const uint8_t *)""; }
    char path[4096];
    if (strcmp(name, "modscan.lua") == 0) {
        if (!g_mac_modscan_path) return NULL;
        snprintf(path, sizeof path, "%s", g_mac_modscan_path);
    } else {
        if (!g_mac_layer_dir) return NULL;
        snprintf(path, sizeof path, "%s/%s", g_mac_layer_dir, name);
    }
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(n > 0 ? (size_t)n : 1);
    if (!b || (n > 0 && fread(b, 1, (size_t)n, f) != (size_t)n)) { free(b); fclose(f); return NULL; }
    fclose(f);
    if (g_nlayer < sizeof g_layer / sizeof *g_layer) {
        snprintf(g_layer[g_nlayer].name, sizeof g_layer[0].name, "%s", name);
        g_layer[g_nlayer].data = b;
        g_layer[g_nlayer].len = (uint32_t)n;
        ++g_nlayer;
    }
    if (len) *len = (uint32_t)n;
    return b;
}
