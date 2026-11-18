/* mac_png.c -- a tiny PNG writer for the Mac Build's screenshots.
 *
 * 8-bit RGB, filter 0, stored (uncompressed) deflate blocks: no zlib needed,
 * and every PNG reader accepts it. A 960x540 frame is 1.5 MB. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t g_crc_table[256];

static void crc_init(void) {
    if (g_crc_table[1]) return;
    for (uint32_t n = 0; n < 256; ++n) {
        uint32_t c = n;
        for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        g_crc_table[n] = c;
    }
}
static uint32_t crc_update(uint32_t crc, const uint8_t *p, size_t n) {
    for (size_t i = 0; i < n; ++i) crc = g_crc_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return crc;
}
static void be32(uint8_t *d, uint32_t v) { d[0] = v >> 24; d[1] = v >> 16; d[2] = v >> 8; d[3] = v; }

static int chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len) {
    uint8_t hdr[8], tail[4];
    be32(hdr, len);
    memcpy(hdr + 4, type, 4);
    uint32_t crc = crc_update(0xFFFFFFFFu, hdr + 4, 4);
    crc = crc_update(crc, data, len) ^ 0xFFFFFFFFu;
    be32(tail, crc);
    return fwrite(hdr, 1, 8, f) == 8 && (len == 0 || fwrite(data, 1, len, f) == len) && fwrite(tail, 1, 4, f) == 4;
}

int isaac_mac_png_write_rgb(const char *path, const uint8_t *rgb, int w, int h) {
    crc_init();
    size_t row = (size_t)w * 3 + 1;
    size_t raw_len = row * (size_t)h;
    uint8_t *raw = malloc(raw_len);
    if (!raw) return 0;
    for (int y = 0; y < h; ++y) {
        raw[y * row] = 0;
        memcpy(raw + y * row + 1, rgb + (size_t)y * w * 3, (size_t)w * 3);
    }
    /* zlib stream: header, stored blocks of at most 65535 bytes, Adler-32 */
    size_t nblocks = (raw_len + 65534) / 65535;
    size_t z_len = 2 + raw_len + nblocks * 5 + 4;
    uint8_t *z = malloc(z_len);
    if (!z) { free(raw); return 0; }
    size_t o = 0;
    z[o++] = 0x78; z[o++] = 0x01;
    uint32_t a = 1, b = 0;
    for (size_t off = 0; off < raw_len;) {
        size_t n = raw_len - off > 65535 ? 65535 : raw_len - off;
        z[o++] = (off + n == raw_len) ? 1 : 0;
        z[o++] = n & 0xFF; z[o++] = n >> 8;
        z[o++] = ~n & 0xFF; z[o++] = (~n >> 8) & 0xFF;
        memcpy(z + o, raw + off, n);
        for (size_t i = 0; i < n; ++i) { a = (a + raw[off + i]) % 65521u; b = (b + a) % 65521u; }
        o += n; off += n;
    }
    be32(z + o, (b << 16) | a);
    o += 4;
    free(raw);

    FILE *f = fopen(path, "wb");
    int ok = 0;
    if (f) {
        static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
        uint8_t ihdr[13];
        be32(ihdr, (uint32_t)w); be32(ihdr + 4, (uint32_t)h);
        ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
        ok = fwrite(sig, 1, 8, f) == 8 && chunk(f, "IHDR", ihdr, 13) && chunk(f, "IDAT", z, (uint32_t)o) &&
             chunk(f, "IEND", NULL, 0);
        ok = (fclose(f) == 0) && ok;
    }
    free(z);
    return ok;
}
