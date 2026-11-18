/* mac_guest_mem.c -- the Guest's 4 GiB on Apple Silicon (docs/adr/0001).
 *
 * The Browser Build maps Guest address N at WebAssembly address N. macOS on
 * arm64 reserves the low 4 GiB of every process, so the Mac Build reserves
 * one 4 GiB region anywhere and every Guest access adds its base
 * (recomp_mem_base; RECOMP_MEM_IDENTITY=0 in the lifted code, isaac_g /
 * isaac_va in the Host).
 *
 * The region is mapped read-write up front without reserving swap; pages cost
 * nothing until touched. Below ISAAC_HOST_BASE_VA is the Guest map the Browser
 * Build uses (image, stack, TEB/PEB, shim slots, heap arena, guard). Above
 * MAC_OVF_BASE, Host code that must hand the Guest memory (the heap's overflow
 * segments) takes it from a bump allocator.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "isaac_host.h"

#define MAC_REGION_BYTES (1ull << 32)
#define MAC_OVF_BASE     0x40000000u   /* well above the Browser Build's map */
#define MAC_OVF_LIMIT    0xFFFF0000u

uint8_t *recomp_mem_base;

static uint32_t g_ovf_next = MAC_OVF_BASE;

int isaac_native_reserve(void) {
    if (recomp_mem_base) return 1;
    void *p = mmap(NULL, MAC_REGION_BYTES, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0);
    if (p == MAP_FAILED) {
        perror("[isaac][mac] mmap of the 4 GiB Guest region");
        return 0;
    }
    recomp_mem_base = (uint8_t *)p;
    /* Guest address 0 must fault, as it does on Windows. */
    mprotect(p, 0x10000, PROT_NONE);
    return 1;
}

/* 64 KiB-aligned bump allocation in the Guest region; never freed. */
uint32_t isaac_native_region_alloc(uint32_t bytes) {
    uint64_t size = ((uint64_t)bytes + 0xFFFFu) & ~0xFFFFull;
    if ((uint64_t)g_ovf_next + size > MAC_OVF_LIMIT) return 0;
    uint32_t va = g_ovf_next;
    g_ovf_next += (uint32_t)size;
    return va;
}

void isaac_native_bad_va(const void *p) {
    fprintf(stderr,
            "[isaac][mac] FATAL: Host pointer %p handed to the Guest is outside "
            "the Guest region [%p, %p). A Host allocation leaked across; it "
            "must come from the Guest heap or isaac_native_region_alloc.\n",
            p, (void *)recomp_mem_base, (void *)(recomp_mem_base + MAC_REGION_BYTES));
    abort();
}
