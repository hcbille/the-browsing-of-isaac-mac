/* mac_fs.c -- the file callbacks host_shims_fs.c makes, as real file I/O.
 *
 * In the Browser Build these are JavaScript (Module.isaacLazyRead and friends,
 * set up by scripts/recomp/lift/boot_integration.mjs or the page). Here they
 * read the game folder (g_mac_instance_dir) and write saves to
 * g_mac_save_dir, with the same path rules as the Node driver.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

const char *g_mac_instance_dir = ".";
const char *g_mac_save_dir;          /* NULL: nothing the game writes persists */

static void join(char *out, size_t cap, const char *dir, const char *rel) {
    snprintf(out, cap, "%s/%s", dir, rel);
    for (char *p = out; *p; ++p) if (*p == '\\') *p = '/';
}

int isaac_fs_lazy_read_js(const char *src, uint8_t *dst, uint32_t len) {
    char path[4096];
    join(path, sizeof path, g_mac_instance_dir, src);
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
        join(path, sizeof path, g_mac_instance_dir, src);
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

int isaac_fs_persist_js(const char *key, const char *src, const uint8_t *data, uint32_t len) {
    if (!g_mac_save_dir) return 0;
    char path[4096];
    join(path, sizeof path, g_mac_save_dir, store_rel(key, src));
    mkdirs_for(path);
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "[isaac][mac] save write FAILED for %s: %s\n", key, strerror(errno)); return 0; }
    size_t w = fwrite(data, 1, len, f);
    fclose(f);
    return w == len;
}

int isaac_fs_unlink_js(const char *key, const char *src) {
    if (!g_mac_save_dir) return 0;
    char path[4096];
    join(path, sizeof path, g_mac_save_dir, store_rel(key, src));
    return unlink(path) == 0 || errno == ENOENT;
}
