/* mac_audio.c -- the Mac Build's audio backend: a software mixer behind the
 * OpenAL object model of host_audio.c, played through an SDL3 audio stream.
 *
 * host_audio.c keeps the OpenAL state (what is playing, queued, processed) and
 * calls the weak isaac_audio_backend_* hooks; the Browser Build's strong
 * versions drive WebAudio (host_audio_web.c). The Mac Build has no WebAudio, so
 * this file provides the strong versions and does the mixing itself:
 *
 *   buffers   PCM (8/16-bit, mono/stereo) converted once to interleaved
 *             float stereo at the buffer's own rate
 *   sources   a list of buffer ids (a stream's queue, or the one static
 *             buffer), a head entry and a fractional position, gain, pitch,
 *             looping; resampled to the device rate with linear
 *             interpolation (the pitch multiplies the step)
 *
 * Pump model: the mixer is ON DEMAND. SDL's audio thread asks for N frames
 * through the stream's get-callback and the mixer renders exactly that many,
 * so the device never waits on the 60 fps game loop and there is no queue to
 * keep topped up (a per-frame push would crackle whenever a game frame ran
 * long). The game thread only edits source state, under one mutex that the
 * callback holds while mixing (about 10 voices x one device period).
 * Streams that run dry simply idle until the game queues more; the model in
 * host_audio.c retires buffers on the wall clock, the mixer on the device
 * clock, and the two stay within a few ms because both are real time.
 *
 * Device format: 48000 Hz, 2 channels, float32. SDL converts to whatever the
 * hardware wants. Headless (no window) or no device: a null sink drains the
 * mixer on the wall clock from the game thread, so counters and the dump still
 * work. A default-device change is handled by SDL itself: a stream opened on
 * SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK follows the default device.
 *
 * The game's music and sound volume options reach the mixer as AL_GAIN on
 * each source (the engine sets them per source); nothing else is applied.
 *
 * Environment:
 *   ISAAC_MAC_AUDIO=0           no audio at all (hooks do nothing)
 *   ISAAC_MAC_AUDIO_NULL=1      null sink even when windowed (no device)
 *   ISAAC_MAC_AUDIO_DUMP=path   write exactly what is sent to SDL as a
 *                               float32 stereo WAV (header patched each second)
 *   ISAAC_MAC_AUDIO_LOG=0       silence the once-a-second counter line
 */
#include <SDL3/SDL.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int isaac_mac_headless(void);
void isaac_mac_audio_tick(void);

#define DEV_RATE 48000
#define DEV_CH 2
#define MAX_ENTRIES 80

typedef struct {
    uint32_t id;
    float *pcm;          /* interleaved stereo float */
    uint32_t frames;
    int freq;
} mbuf;

typedef struct {
    uint32_t id;
    float gain, pitch;
    int looping, stream, playing;
    uint32_t entries[MAX_ENTRIES];
    int n;               /* entries in use */
    int head;            /* entry being played */
    double pos;          /* frame position in the head entry */
} msrc;

static SDL_Mutex *g_mu;
static mbuf *g_bufs; static int g_nbufs, g_cbufs;
static msrc *g_srcs; static int g_nsrcs, g_csrcs;

static int g_state;                  /* 0 untried, 1 ready, -1 off */
static SDL_AudioStream *g_stream;    /* NULL = null sink */
static int g_null_sink;
static FILE *g_dump; static uint64_t g_dump_frames;

/* counters (under g_mu unless noted) */
static uint64_t g_frames_mixed, g_cb_count, g_underruns, g_overruns, g_late_cb;
static double g_max_cb_us;
static double g_w_peak, g_w_sumsq; static uint64_t g_w_frames;     /* current window */
static double g_all_peak;
static uint64_t g_active_frames;       /* frames with any voice playing */
static uint64_t g_last_cb_end_ns, g_last_cb_supply_ns;
static int g_voices_last;

static mbuf *buf_find(uint32_t id) {
    for (int i = 0; i < g_nbufs; ++i) if (g_bufs[i].id == id) return &g_bufs[i];
    return NULL;
}
static msrc *src_find(uint32_t id) {
    for (int i = 0; i < g_nsrcs; ++i) if (g_srcs[i].id == id) return &g_srcs[i];
    return NULL;
}
static msrc *src_get(uint32_t id) {
    msrc *s = src_find(id);
    if (s) return s;
    if (g_nsrcs == g_csrcs) {
        int c = g_csrcs ? g_csrcs * 2 : 64;
        msrc *p = (msrc *)realloc(g_srcs, (size_t)c * sizeof *p);
        if (!p) return NULL;
        g_srcs = p; g_csrcs = c;
    }
    s = &g_srcs[g_nsrcs++];
    memset(s, 0, sizeof *s);
    s->id = id; s->gain = 1.0f; s->pitch = 1.0f;
    return s;
}

/* ---- mixing ------------------------------------------------------------- */

/* sample (frame i, channel c) of the head entry or the continuation after it */
static inline void fetch(const msrc *s, const mbuf *b, uint32_t i, float *l, float *r) {
    if (i < b->frames) { *l = b->pcm[2 * i]; *r = b->pcm[2 * i + 1]; return; }
    if (s->stream && s->head + 1 < s->n) {
        mbuf *nb = buf_find(s->entries[s->head + 1]);
        if (nb && nb->frames) { *l = nb->pcm[0]; *r = nb->pcm[1]; return; }
    }
    if (!s->stream && s->looping && b->frames) { *l = b->pcm[0]; *r = b->pcm[1]; return; }
    *l = b->frames ? b->pcm[2 * (b->frames - 1)] : 0.0f;
    *r = b->frames ? b->pcm[2 * (b->frames - 1) + 1] : 0.0f;
}

static void mix_src(msrc *s, float *out, int frames) {
    int o = 0;
    while (o < frames && s->playing) {
        if (s->head >= s->n) {                   /* nothing (more) to play */
            if (!s->stream) s->playing = 0;      /* a static sound ended; a stream idles */
            return;
        }
        mbuf *b = buf_find(s->entries[s->head]);
        if (!b || !b->frames || !b->freq) { s->head++; s->pos = 0; continue; }
        double step = (double)s->pitch * (double)b->freq / (double)DEV_RATE;
        if (step <= 0.0) step = 1.0;
        float g = s->gain;
        while (o < frames) {
            uint32_t i = (uint32_t)s->pos;
            if (i >= b->frames) break;
            float f = (float)(s->pos - (double)i);
            float l0 = b->pcm[2 * i], r0 = b->pcm[2 * i + 1], l1, r1;
            fetch(s, b, i + 1, &l1, &r1);
            out[2 * o]     += g * (l0 + (l1 - l0) * f);
            out[2 * o + 1] += g * (r0 + (r1 - r0) * f);
            s->pos += step;
            ++o;
        }
        if ((uint32_t)s->pos >= b->frames) {
            s->pos -= (double)b->frames;
            if (!s->stream && s->looping) { /* wrap on the same buffer */ }
            else if (s->stream) { s->head++; }
            else { s->playing = 0; s->head = s->n; s->pos = 0; return; }
        }
    }
}

/* mix `frames` device frames into out (zeroed here); caller holds g_mu */
static void mix_locked(float *out, int frames) {
    memset(out, 0, (size_t)frames * DEV_CH * sizeof(float));
    int voices = 0;
    for (int k = 0; k < g_nsrcs; ++k) {
        msrc *s = &g_srcs[k];
        if (!s->playing || s->head >= s->n) continue;
        ++voices;
        mix_src(s, out, frames);
    }
    g_voices_last = voices;
    double pk = 0.0, sq = 0.0;
    for (int i = 0; i < frames * DEV_CH; ++i) {
        float v = out[i];
        if (v > 1.0f) v = 1.0f; else if (v < -1.0f) v = -1.0f;
        out[i] = v;
        double a = fabs((double)v);
        if (a > pk) pk = a;
        sq += (double)v * (double)v;
    }
    if (pk > g_w_peak) g_w_peak = pk;
    if (pk > g_all_peak) g_all_peak = pk;
    g_w_sumsq += sq;
    g_w_frames += (uint64_t)frames * DEV_CH;
    g_frames_mixed += (uint64_t)frames;
    if (voices) g_active_frames += (uint64_t)frames;
    if (g_dump) { fwrite(out, sizeof(float), (size_t)frames * DEV_CH, g_dump); g_dump_frames += (uint64_t)frames; }
}

static void SDLCALL stream_cb(void *ud, SDL_AudioStream *st, int additional, int total) {
    (void)ud; (void)total;
    uint64_t t0 = SDL_GetTicksNS();
    int queued = SDL_GetAudioStreamQueued(st);
    int bytes = additional;
    if (bytes <= 0) return;
    int maxb = DEV_RATE / 2 * DEV_CH * (int)sizeof(float);
    int over = 0;
    if (bytes > maxb) { bytes = maxb; over = 1; }          /* never render more than 0.5 s at once */
    int frames = bytes / (DEV_CH * (int)sizeof(float));
    static float buf[DEV_RATE / 2 * DEV_CH];
    SDL_LockMutex(g_mu);
    ++g_cb_count;
    if (over) ++g_overruns;
    /* the device came back for more after everything we supplied last time
     * should have played out, and the stream was empty: it ran dry */
    if (g_last_cb_end_ns && queued == 0 &&
        t0 > g_last_cb_end_ns + g_last_cb_supply_ns + 5000000ull)
        ++g_underruns;
    mix_locked(buf, frames);
    SDL_UnlockMutex(g_mu);
    SDL_PutAudioStreamData(st, buf, frames * DEV_CH * (int)sizeof(float));
    uint64_t t1 = SDL_GetTicksNS();
    double us = (double)(t1 - t0) / 1000.0;
    SDL_LockMutex(g_mu);
    if (us > g_max_cb_us) g_max_cb_us = us;
    if (us > 1000.0 * 1000.0 * (double)frames / DEV_RATE) ++g_late_cb;    /* slower than real time */
    g_last_cb_end_ns = t1;
    g_last_cb_supply_ns = (uint64_t)((double)frames * 1e9 / DEV_RATE);
    SDL_UnlockMutex(g_mu);
}

/* ---- WAV dump ------------------------------------------------------------ */
static void wav_header(FILE *f, uint64_t frames) {
    uint32_t data = (uint32_t)(frames * DEV_CH * 4u), v;
    uint16_t h;
    fseek(f, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, f); v = 36u + data; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    h = 3; fwrite(&h, 2, 1, f);                   /* IEEE float */
    h = DEV_CH; fwrite(&h, 2, 1, f);
    v = DEV_RATE; fwrite(&v, 4, 1, f);
    v = DEV_RATE * DEV_CH * 4; fwrite(&v, 4, 1, f);
    h = DEV_CH * 4; fwrite(&h, 2, 1, f);
    h = 32; fwrite(&h, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
}
static void dump_patch_locked(void) {
    if (!g_dump) return;
    wav_header(g_dump, g_dump_frames);
    fseek(g_dump, 0, SEEK_END);
    fflush(g_dump);
}

/* ---- init, tick, report ---------------------------------------------------- */
static int log_on(void) {
    static int on = -1;
    if (on < 0) { const char *e = getenv("ISAAC_MAC_AUDIO_LOG"); on = !(e && *e == '0'); }
    return on;
}

static void final_report(void) {
    if (g_state != 1) return;
    SDL_LockMutex(g_mu);
    dump_patch_locked();
    printf("[mac][audio] final: %llu frames mixed (%.1f s, %.1f s with voices), %llu callbacks, "
           "underruns %llu, overruns %llu, late callbacks %llu, max callback %.0f us, peak %.3f, sink %s%s\n",
           (unsigned long long)g_frames_mixed, (double)g_frames_mixed / DEV_RATE,
           (double)g_active_frames / DEV_RATE, (unsigned long long)g_cb_count,
           (unsigned long long)g_underruns, (unsigned long long)g_overruns, (unsigned long long)g_late_cb,
           g_max_cb_us, g_all_peak, g_null_sink ? "null (wall clock)" : "SDL device", g_dump ? ", dump written" : "");
    SDL_UnlockMutex(g_mu);
}

static int audio_init(void) {
    if (g_state) return g_state > 0;
    const char *off = getenv("ISAAC_MAC_AUDIO");
    if (off && *off == '0') { g_state = -1; return 0; }
    g_mu = SDL_CreateMutex();
    const char *nul = getenv("ISAAC_MAC_AUDIO_NULL");
    g_null_sink = isaac_mac_headless() || (nul && *nul == '1');
    const char *dump = getenv("ISAAC_MAC_AUDIO_DUMP");
    if (dump && *dump) {
        g_dump = fopen(dump, "wb");
        if (g_dump) wav_header(g_dump, 0);
        else fprintf(stderr, "[mac][audio] cannot open dump %s\n", dump);
    }
    if (!g_null_sink) {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            fprintf(stderr, "[mac][audio] SDL audio init failed: %s; using the null sink\n", SDL_GetError());
            g_null_sink = 1;
        } else {
            SDL_AudioSpec spec = { SDL_AUDIO_F32, DEV_CH, DEV_RATE };
            g_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, stream_cb, NULL);
            if (!g_stream) {
                fprintf(stderr, "[mac][audio] no playback device (%s); using the null sink\n", SDL_GetError());
                g_null_sink = 1;
            } else {
                SDL_ResumeAudioStreamDevice(g_stream);
                SDL_AudioDeviceID d = SDL_GetAudioStreamDevice(g_stream);
                SDL_AudioSpec hw; int period = 0;
                SDL_GetAudioDeviceFormat(d, &hw, &period);
                printf("[mac][audio] device \"%s\": %d Hz, %d ch, %d-frame period; stream %d Hz stereo float32, pump = on-demand get-callback\n",
                       SDL_GetAudioDeviceName(d), hw.freq, hw.channels, period, DEV_RATE);
            }
        }
    }
    if (g_null_sink)
        printf("[mac][audio] null sink: the mixer is drained on the wall clock (%s)\n",
               isaac_mac_headless() ? "headless" : "no device");
    g_state = 1;
    atexit(final_report);
    return 1;
}

/* Once a second from the game thread: drain the null sink, log counters. */
void isaac_mac_audio_tick(void) {
    static uint64_t last_ns, last_log_ns;
    if (g_state <= 0) return;
    uint64_t now = SDL_GetTicksNS();
    if (!last_ns) { last_ns = last_log_ns = now; return; }
    if (g_null_sink) {
        double dt = (double)(now - last_ns) / 1e9;
        int frames = (int)(dt * DEV_RATE);
        if (frames > 0) {
            if (frames > DEV_RATE / 2) { frames = DEV_RATE / 2; SDL_LockMutex(g_mu); ++g_overruns; SDL_UnlockMutex(g_mu); }
            static float buf[DEV_RATE / 2 * DEV_CH];
            SDL_LockMutex(g_mu);
            ++g_cb_count;
            mix_locked(buf, frames);
            SDL_UnlockMutex(g_mu);
            last_ns += (uint64_t)((double)frames * 1e9 / DEV_RATE);
        }
    } else last_ns = now;
    if (now - last_log_ns >= 1000000000ull) {
        last_log_ns = now;
        /* never call an SDL audio stream function with g_mu held: the audio thread holds the stream's lock
         * while it waits for g_mu in stream_cb (a lock-order inversion that hung the game at frame 871) */
        int queued = g_stream ? SDL_GetAudioStreamQueued(g_stream) : 0;
        SDL_LockMutex(g_mu);
        double rms = g_w_frames ? sqrt(g_w_sumsq / (double)g_w_frames) : 0.0;
        if (log_on())
            printf("[mac][audio] mixed %llu frames, %llu cb, queued %.1f ms, voices %d, underruns %llu, overruns %llu, late %llu, last-second peak %.3f rms %.4f\n",
                   (unsigned long long)g_frames_mixed, (unsigned long long)g_cb_count,
                   1000.0 * queued / (DEV_RATE * DEV_CH * 4.0), g_voices_last,
                   (unsigned long long)g_underruns, (unsigned long long)g_overruns, (unsigned long long)g_late_cb,
                   g_w_peak, rms);
        g_w_peak = g_w_sumsq = 0.0; g_w_frames = 0;
        dump_patch_locked();
        SDL_UnlockMutex(g_mu);
    }
}

/* ---- the hooks host_audio.c calls (strong here, weak there) --------------- */

void isaac_audio_backend_buffer(uint32_t id, const void *pcm, uint32_t bytes, int channels, int bits, int freq) {
    if (!audio_init()) return;
    if (channels < 1 || channels > 2 || !freq) return;
    uint32_t frame = (uint32_t)channels * (uint32_t)(bits / 8);
    uint32_t frames = frame ? bytes / frame : 0;
    if (!frames) return;
    float *out = (float *)malloc((size_t)frames * 2 * sizeof(float));
    if (!out) return;
    const uint8_t *p = (const uint8_t *)pcm;
    for (uint32_t i = 0; i < frames; ++i) {
        float v[2];
        for (int c = 0; c < channels; ++c) {
            if (bits == 16) {
                const uint8_t *q = p + ((size_t)i * channels + c) * 2;
                v[c] = (float)(int16_t)(q[0] | (q[1] << 8)) / 32768.0f;
            } else {
                v[c] = ((float)p[(size_t)i * channels + c] - 128.0f) / 128.0f;
            }
        }
        out[2 * i] = v[0];
        out[2 * i + 1] = channels == 2 ? v[1] : v[0];
    }
    SDL_LockMutex(g_mu);
    mbuf *b = buf_find(id);
    float *old = NULL;
    if (!b) {
        if (g_nbufs == g_cbufs) {
            int c = g_cbufs ? g_cbufs * 2 : 128;
            mbuf *np = (mbuf *)realloc(g_bufs, (size_t)c * sizeof *np);
            if (!np) { SDL_UnlockMutex(g_mu); free(out); return; }
            g_bufs = np; g_cbufs = c;
        }
        b = &g_bufs[g_nbufs++];
        b->id = id; b->pcm = NULL;
    }
    old = b->pcm;
    b->pcm = out; b->frames = frames; b->freq = freq;
    SDL_UnlockMutex(g_mu);
    free(old);
}

void isaac_audio_backend_drop_buffer(uint32_t id) {
    if (g_state <= 0) return;
    SDL_LockMutex(g_mu);
    mbuf *b = buf_find(id);
    float *old = NULL;
    if (b) { old = b->pcm; *b = g_bufs[--g_nbufs]; }
    SDL_UnlockMutex(g_mu);
    free(old);
}

void isaac_audio_backend_queue(uint32_t src, uint32_t buffer) {
    if (!audio_init()) return;
    SDL_LockMutex(g_mu);
    msrc *s = src_get(src);
    if (s) {
        s->stream = 1;
        if (s->n < MAX_ENTRIES) s->entries[s->n++] = buffer;
    }
    SDL_UnlockMutex(g_mu);
    isaac_mac_audio_tick();     /* headless: the hooks are the only per-frame call */
}

void isaac_audio_backend_unqueue(uint32_t src, uint32_t n) {
    if (g_state <= 0) return;
    SDL_LockMutex(g_mu);
    msrc *s = src_find(src);
    if (s && n) {
        if ((int)n > s->n) n = (uint32_t)s->n;
        memmove(s->entries, s->entries + n, (size_t)(s->n - (int)n) * sizeof s->entries[0]);
        s->n -= (int)n;
        if (s->head >= (int)n) s->head -= (int)n; else { s->head = 0; s->pos = 0; }
    }
    SDL_UnlockMutex(g_mu);
}

void isaac_audio_backend_play(uint32_t src, uint32_t buffer, float gain, float pitch,
                              int looping, int streaming, uint32_t head, float offset_sec) {
    if (!audio_init()) return;
    SDL_LockMutex(g_mu);
    msrc *s = src_get(src);
    if (s) {
        s->gain = gain;
        s->pitch = pitch > 0.01f ? pitch : 1.0f;
        s->looping = looping;
        s->playing = 1;
        if (streaming) {
            s->stream = 1;
            s->head = (int)head;
        } else {
            s->stream = 0;
            s->entries[0] = buffer; s->n = 1; s->head = 0;
        }
        mbuf *b = (s->head < s->n) ? buf_find(s->entries[s->head]) : NULL;
        s->pos = (b && offset_sec > 0.0f) ? (double)offset_sec * (double)b->freq : 0.0;
    }
    SDL_UnlockMutex(g_mu);
}

static void src_halt(uint32_t src, int clear) {
    if (g_state <= 0) return;
    SDL_LockMutex(g_mu);
    msrc *s = src_find(src);
    if (s) {
        s->playing = 0;
        if (clear) { s->n = 0; s->head = 0; s->pos = 0; s->stream = 0; }
    }
    SDL_UnlockMutex(g_mu);
}
void isaac_audio_backend_stop(uint32_t src) { src_halt(src, 0); }
void isaac_audio_backend_pause(uint32_t src) { src_halt(src, 0); }
void isaac_audio_backend_clear(uint32_t src) { src_halt(src, 1); }

void isaac_audio_backend_delete(uint32_t src) {
    if (g_state <= 0) return;
    SDL_LockMutex(g_mu);
    msrc *s = src_find(src);
    if (s) *s = g_srcs[--g_nsrcs];
    SDL_UnlockMutex(g_mu);
}

void isaac_audio_backend_gain(uint32_t src, float gain) {
    if (!audio_init()) return;
    SDL_LockMutex(g_mu);
    msrc *s = src_get(src);
    if (s) s->gain = gain;
    SDL_UnlockMutex(g_mu);
}

void isaac_audio_backend_pitch(uint32_t src, float pitch) {
    if (!audio_init()) return;
    SDL_LockMutex(g_mu);
    msrc *s = src_get(src);
    if (s) s->pitch = pitch > 0.01f ? pitch : 1.0f;
    SDL_UnlockMutex(g_mu);
}
