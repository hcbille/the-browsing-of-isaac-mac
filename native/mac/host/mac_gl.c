/* mac_gl.c -- GL pieces the Mac Build needs before it has a renderer.
 *
 * upstream's GL backend (scripts/recomp/host/src/host_gl_webgl.c) is compiled
 * only for the browser (ISAAC_WEB). Its calls are GLES already, so the Mac
 * Build will compile it against ANGLE once SDL3 provides the context; until
 * then the headless boot needs the counters it exports. Weak, so the real
 * backend replaces them as soon as it links.
 */
#include <stdint.h>

/* RGBA bytes handed to glTexImage2D (host_img_keep.c sizes images by it). */
__attribute__((weak)) uint64_t isaac_gl_tex_image_bytes(void) { return 0; }
