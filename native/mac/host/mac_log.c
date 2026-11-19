/* mac_log.c -- the Host's log writer (docs/mac/RUNBOOK.md, "Logging").
 *
 * Every log line of the process goes through stdio: printf/fprintf(stderr) from the Host, isaac_log, the game's own
 * logger (OutputDebugStringA -> "[odsa] ..."), the Layer's lines. Before this file stderr was unbuffered (a write(2)
 * per call, two for some) and stdout line-buffered (a write(2) per line). Now:
 *
 *   - stderr is made the same FILE as stdout when both fds are the same pipe/file (run.sh's `> log 2>&1`, the
 *     Launcher's pipe), so the order between the two is exact; they share one big buffer (ISAAC_LOG_BUF_KB, 1024).
 *   - a flusher thread writes the buffer out every ISAAC_LOG_FLUSH_MS (100) ms;
 *   - flushed at once: [mac][boot] status/ready lines (mac_boot_status), exit(), the frame-limit exit and the "game did
 *     not close" _exit (they call fflush(NULL)), and every fatal or terminating signal (SEGV BUS ILL FPE ABRT TRAP
 *     SYS TERM INT HUP QUIT ALRM): the handler flushes and then re-raises the signal with the default action,
 *     so the crash report / exit status stay what they were. A crash or kill therefore loses nothing that was logged
 *     before the signal. (A SIGKILL or a machine loss still loses up to 100 ms.)
 *   - a terminal on fd 1 keeps line buffering. ISAAC_LOG_UNBUFFERED=1 restores the old behaviour exactly.
 *
 * Repeat suppression (isaac_mac_log_repeat, called by the OutputDebugStringA shim) is described at that function.
 */
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int g_buffered;
static int g_merged;           /* stderr is stdout */

static int env_on(const char *n, int dflt) {
    const char *e = getenv(n);
    if (!e || !*e) return dflt;
    return *e != '0';
}
static long env_num(const char *n, long dflt) {
    const char *e = getenv(n);
    return (e && *e) ? atol(e) : dflt;
}

/* Flush without risking a deadlock in a signal handler: another thread may hold the stream lock mid-write. */
static void flush_try(void) {
    FILE *fs[2] = { stdout, g_merged ? NULL : stderr };
    for (int k = 0; k < 2; ++k) {
        FILE *f = fs[k];
        if (!f) continue;
        for (int i = 0; i < 300; ++i) {
            if (ftrylockfile(f) == 0) { fflush(f); funlockfile(f); break; }
            usleep(1000);
        }
    }
}

void isaac_mac_log_flush(void) { if (g_buffered) flush_try(); }

static void *flusher(void *arg) {
    long ms = (long)(intptr_t)arg;
    for (;;) {
        usleep((useconds_t)ms * 1000);
        fflush(stdout);
        if (!g_merged) fflush(stderr);
    }
    return NULL;
}

static void on_signal(int sig) {
    flush_try();
    signal(sig, SIG_DFL);
    raise(sig);
}

/* ------------------------------------------------------------ repeats --- */
#define REP_SLOTS 4096
typedef struct { uint64_t h; uint32_t n; char head[72]; } rep_slot;
static rep_slot g_rep[REP_SLOTS];
static unsigned g_rep_used;
static pthread_mutex_t g_rep_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_dedup = 1;
static int g_dedup_keep = 3;
static uint64_t g_rep_suppressed;

static uint64_t fnv(const char *s) {
    uint64_t h = 1469598103934665603ull;
    for (; *s; ++s) { h ^= (unsigned char)*s; h *= 1099511628211ull; }
    return h ? h : 1;
}

/* Only lines that are noise by nature are candidates: the engine's "[warn]" lines and the "... failed: <Lua error>"
 * lines (a mod callback that raises an error every frame). Everything else is never suppressed. */
static int candidate(const char *line) {
    return strstr(line, "[warn]") || strstr(line, "\" failed: ") || strstr(line, "[ERROR]");
}

/* Returns 1 when `line` (one message of the game's logger, without the "[odsa] " prefix) must not be written.
 * The first ISAAC_LOG_DEDUP_KEEP (3) occurrences of an exact message are written verbatim. After that it is
 * silent, except a note at the 10th, 100th, 1000th ... occurrence ("[mac][log] repeated 100x so far: <head>") and the
 * totals that the exit summary prints. ISAAC_LOG_DEDUP=0 writes every line. */
int isaac_mac_log_repeat(const char *line) {
    if (!g_dedup || !g_buffered || !candidate(line)) return 0;
    uint64_t h = fnv(line);
    int suppress = 0;
    uint32_t note = 0;
    char head[72] = "";
    pthread_mutex_lock(&g_rep_lock);
    unsigned i = (unsigned)(h % REP_SLOTS), probes = 0;
    while (g_rep[i].h && g_rep[i].h != h && probes < REP_SLOTS) { i = (i + 1) % REP_SLOTS; ++probes; }
    rep_slot *s = &g_rep[i];
    if (probes < REP_SLOTS) {
        if (!s->h) {
            s->h = h; ++g_rep_used;
            size_t n = strcspn(line, "\n"); if (n > sizeof s->head - 1) n = sizeof s->head - 1;
            memcpy(s->head, line, n); s->head[n] = 0;
        }
        ++s->n;
        if ((int)s->n > g_dedup_keep) {
            suppress = 1; ++g_rep_suppressed;
            uint32_t c = s->n;
            if (c == 10 || c == 100 || c == 1000 || c == 10000 || c == 100000 || c == 1000000) { note = c; memcpy(head, s->head, sizeof head); }
        }
    }
    pthread_mutex_unlock(&g_rep_lock);
    if (note) fprintf(stderr, "[mac][log] repeated %ux so far (more are not written; ISAAC_LOG_DEDUP=0 writes all): %s\n", note, head);
    return suppress;
}

/* The totals of the suppressed lines, then a flush. Idempotent; atexit and the _exit paths (frame limit) call it. */
void isaac_mac_log_finish(void) {
    static int done;
    if (!g_buffered) return;
    if (done || !g_rep_suppressed) { flush_try(); return; }
    done = 1;
    pthread_mutex_lock(&g_rep_lock);
    fprintf(stderr, "[mac][log] %llu repeated line(s) were not written; counts per distinct line (kept first %d each):\n",
            (unsigned long long)g_rep_suppressed, g_dedup_keep);
    for (unsigned i = 0; i < REP_SLOTS; ++i)
        if (g_rep[i].h && (int)g_rep[i].n > g_dedup_keep)
            fprintf(stderr, "[mac][log]   %ux: %s\n", g_rep[i].n, g_rep[i].head);
    pthread_mutex_unlock(&g_rep_lock);
    flush_try();
}

/* Called first thing in main() (and again later: idempotent). */
void isaac_mac_log_setup(void) {
    static int done;
    if (done) return;
    done = 1;
    if (env_on("ISAAC_LOG_UNBUFFERED", 0) || isatty(1)) {      /* the old behaviour: stdout per line, stderr unbuffered */
        setvbuf(stdout, NULL, _IOLBF, 0);
        return;
    }
    long kb = env_num("ISAAC_LOG_BUF_KB", 1024);
    if (kb < 16) kb = 16;
    char *buf = malloc((size_t)kb * 1024);
    if (!buf) { setvbuf(stdout, NULL, _IOLBF, 0); return; }
    struct stat a, b;
    g_merged = fstat(1, &a) == 0 && fstat(2, &b) == 0 && a.st_dev == b.st_dev && a.st_ino == b.st_ino
               && (S_ISFIFO(a.st_mode) || S_ISREG(a.st_mode));
    setvbuf(stdout, buf, _IOFBF, (size_t)kb * 1024);
    if (g_merged) stderr = stdout;
    else {
        char *eb = malloc(256 * 1024);
        if (eb) setvbuf(stderr, eb, _IOFBF, 256 * 1024);
    }
    g_buffered = 1;
    g_dedup = env_on("ISAAC_LOG_DEDUP", 1);
    g_dedup_keep = (int)env_num("ISAAC_LOG_DEDUP_KEEP", 3);

    stack_t ss = { .ss_sp = malloc(1 << 16), .ss_size = 1 << 16, .ss_flags = 0 };
    if (ss.ss_sp) sigaltstack(&ss, NULL);
    static const int sigs[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGTRAP, SIGSYS, SIGTERM, SIGINT, SIGHUP, SIGQUIT, SIGALRM };
    for (unsigned i = 0; i < sizeof sigs / sizeof *sigs; ++i) {
        struct sigaction old;
        if (sigaction(sigs[i], NULL, &old) == 0 && old.sa_handler != SIG_DFL) continue;  /* someone else's handler stays */
        struct sigaction sa;
        memset(&sa, 0, sizeof sa);
        sa.sa_handler = on_signal;
        sa.sa_flags = SA_ONSTACK | SA_NODEFER;
        sigemptyset(&sa.sa_mask);
        sigaction(sigs[i], &sa, NULL);
    }
    atexit(isaac_mac_log_finish);
    pthread_t th;
    pthread_attr_t at;
    pthread_attr_init(&at);
    pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
    long ms = env_num("ISAAC_LOG_FLUSH_MS", 100);
    if (ms < 5) ms = 5;
    pthread_create(&th, &at, flusher, (void *)(intptr_t)ms);
}
