/* Verification harness for the DIT change (not part of the PR).
   1. Samples the interrupted PSTATE with SIGPROF while scalar
      multiplications run, and counts how often DIT was set.
   2. Times the public API against direct backend calls. */
#define _GNU_SOURCE
#include <mx25519.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#else
#include <sys/auxv.h>
#include <ucontext.h>
#endif

void mx25519_scalarmult_portable(uint8_t*, const uint8_t*, const uint8_t*);
#ifndef NO_ARM64
void mx25519_scalarmult_arm64(uint8_t*, const uint8_t*, const uint8_t*);
#endif
typedef void fn_t(uint8_t*, const uint8_t*, const uint8_t*);

static volatile sig_atomic_t busy;
static volatile long samples[2][2]; /* [busy][dit] */
static volatile uint64_t offpc[64]; static volatile int noffpc;
static volatile long live[2][2]; /* DIT read inside the handler: [busy][dit] */
static int has_dit_global;

static void handler(int sig, siginfo_t* si, void* ctx) {
    (void)sig; (void)si;
    ucontext_t* uc = (ucontext_t*)ctx;
#ifdef __APPLE__
    uint64_t ps = uc->uc_mcontext->__ss.__cpsr;
#else
    uint64_t ps = uc->uc_mcontext.pstate;
#endif
    samples[busy ? 1 : 0][(ps >> 24) & 1]++;
    if (has_dit_global) {
        uint64_t v; __asm__ volatile("mrs %0, s3_3_c4_c2_5" : "=r"(v));
        live[busy ? 1 : 0][(v >> 24) & 1]++;
    }
#ifndef __APPLE__
    if (busy && !((ps >> 24) & 1) && noffpc < 64) offpc[noffpc++] = uc->uc_mcontext.pc;
#endif
}

static int cpu_has_dit(void) {
#ifdef __APPLE__
    int v = 0; size_t sz = sizeof(v);
    return sysctlbyname("hw.optional.arm.FEAT_DIT", &v, &sz, NULL, 0) == 0 && v;
#else
    return (getauxval(AT_HWCAP) & (1UL << 24)) != 0;
#endif
}
static uint64_t get_dit(void) {
    uint64_t v; __asm__ volatile("mrs %0, s3_3_c4_c2_5" : "=r"(v) : : "memory"); return v;
}
static void set_dit(uint64_t v) {
    __asm__ volatile("msr s3_3_c4_c2_5, %0\n\tisb" : : "r"(v) : "memory");
}
static double now(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static mx25519_privkey key;
static mx25519_pubkey pt, out;

enum { API, DIRECT, DIRECT_DIT };
static void call(int how, const mx25519_impl* impl, fn_t* fn) {
    if (how == API) mx25519_scmul_key_unclamped(impl, &out, &key, &pt);
    else fn(out.data, key.data, pt.data);
}

static void sample(const char* name, int how, const mx25519_impl* impl, fn_t* fn, double secs, int has_dit) {
    memset((void*)samples, 0, sizeof(samples)); noffpc = 0;
    memset((void*)live, 0, sizeof(live));
    if (how == DIRECT_DIT) set_dit(1UL << 24);
    double t0 = now();
    long calls = 0;
    while (now() - t0 < secs) {
        busy = 1; call(how, impl, fn); busy = 0; calls++;
    }
    if (how == DIRECT_DIT) set_dit(0);
    long on = samples[1][1], off = samples[1][0];
    printf("sample %-22s calls=%-7ld busy_samples=%-6ld dit_on=%-6ld dit_off=%-6ld (%.2f%% on) idle_on=%ld caller_dit_after=%llu\n",
        name, calls, on + off, on, off, (on + off) ? 100.0 * on / (on + off) : 0.0, samples[0][1],
        has_dit ? (unsigned long long)(get_dit() >> 24) : 0ULL);
    if (has_dit) printf("       %-22s live DIT in handler while busy: on=%ld off=%ld\n", "", live[1][1], live[1][0]);
    if (getenv("OFFPC") && how == API) for (int i = 0; i < noffpc; ++i) printf("  offpc %#llx\n", (unsigned long long)offpc[i]);
}

static int cmp(const void* a, const void* b) {
    double x = *(const double*)a, y = *(const double*)b; return (x > y) - (x < y);
}
static double bench(int how, const mx25519_impl* impl, fn_t* fn, int n) {
    if (how == DIRECT_DIT) set_dit(1UL << 24);
    double t0 = now();
    for (int i = 0; i < n; ++i) call(how, impl, fn);
    double t = (now() - t0) / n;
    if (how == DIRECT_DIT) set_dit(0);
    return t;
}

int main(int argc, char** argv) {
    double secs = argc > 1 ? atof(argv[1]) : 2.0;
    int rounds = argc > 2 ? atoi(argv[2]) : 15;
    int n = argc > 3 ? atoi(argv[3]) : 2000;
    for (int i = 0; i < 32; ++i) key.data[i] = (uint8_t)(0x5a ^ (i * 37));
    pt.data[0] = 9;
    int has_dit = cpu_has_dit();
    has_dit_global = has_dit;
    printf("FEAT_DIT reported by the OS: %d\n", has_dit);
    if (has_dit) printf("DIT at start: %llu\n", (unsigned long long)(get_dit() >> 24));

    struct { const char* name; mx25519_type type; fn_t* fn; } be[] = {
        { "portable", MX25519_TYPE_PORTABLE, mx25519_scalarmult_portable },
#ifndef NO_ARM64
        { "arm64", MX25519_TYPE_ARM64, mx25519_scalarmult_arm64 },
#endif
    };
    int nbe = sizeof(be) / sizeof(be[0]);

    struct sigaction sa; memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = handler; sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(SIGPROF, &sa, NULL);
    struct itimerval it = { { 0, 1000 }, { 0, 1000 } };
    setitimer(ITIMER_PROF, &it, NULL);
    for (int b = 0; b < nbe; ++b) {
        const mx25519_impl* impl = mx25519_select_impl(be[b].type);
        char name[64];
        snprintf(name, sizeof(name), "%s/api", be[b].name);
        sample(name, API, impl, be[b].fn, secs, has_dit);
        snprintf(name, sizeof(name), "%s/direct", be[b].name);
        sample(name, DIRECT, impl, be[b].fn, secs, has_dit);
        if (has_dit) {
            /* caller already has DIT on: must stay on */
            set_dit(1UL << 24);
            snprintf(name, sizeof(name), "%s/api,caller_on", be[b].name);
            sample(name, API, impl, be[b].fn, secs, has_dit);
            set_dit(0);
        }
    }
    struct itimerval stop; memset(&stop, 0, sizeof(stop));
    setitimer(ITIMER_PROF, &stop, NULL);

    for (int b = 0; b < nbe && rounds > 0; ++b) {
        const mx25519_impl* impl = mx25519_select_impl(be[b].type);
        double* t[3];
        for (int k = 0; k < 3; ++k) t[k] = calloc(rounds, sizeof(double));
        for (int r = 0; r < rounds; ++r) {
            t[API][r] = bench(API, impl, be[b].fn, n);
            t[DIRECT][r] = bench(DIRECT, impl, be[b].fn, n);
            if (has_dit) t[DIRECT_DIT][r] = bench(DIRECT_DIT, impl, be[b].fn, n);
        }
        for (int k = 0; k < 3; ++k) qsort(t[k], rounds, sizeof(double), cmp);
        double api = t[API][rounds / 2], dir = t[DIRECT][rounds / 2], dd = t[DIRECT_DIT][rounds / 2];
        printf("bench %-8s median us/call: api=%.3f direct(DIT off)=%.3f", be[b].name, api * 1e6, dir * 1e6);
        if (has_dit) printf(" direct(DIT on)=%.3f  api/direct_off=%.4f  direct_on/direct_off=%.4f", dd * 1e6, api / dir, dd / dir);
        printf("  (min api=%.3f off=%.3f)\n", t[API][0] * 1e6, t[DIRECT][0] * 1e6);
    }
    return 0;
}
