/* PS4LM memprobe: the Phase 0 probe.
 *
 * Measures what a payload process on a PS4 can use for CPU inference: the
 * firmware, how many CPUs it sees and may use, the largest single malloc and
 * the total malloc in 256 MiB blocks (pages touched), whether a 1 GiB file in
 * /data/PS4LM maps, memcpy bandwidth, AVX fp32 throughput, the cpuid flags and
 * the free space under /data.
 *
 * Written in C++17 on purpose: it is also the first check that the SDK's
 * libc++ works, and std::thread::hardware_concurrency() is one of the numbers.
 *
 * Every line goes to stdout (elfldr streams it back to scripts/send.sh) and to
 * /data/PS4LM/memprobe.txt, synced line by line, so a probe the system kills
 * still leaves its last good step on disk.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <cpuid.h>
#include <errno.h>
#include <fcntl.h>
#include <immintrin.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* FreeBSD 9 headers: sys/types.h and sys/param.h come before sys/cpuset.h. */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/cpuset.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <ps4/kernel.h>

constexpr unsigned long long MiB = 1024ULL * 1024ULL;
constexpr unsigned long long GiB = 1024ULL * MiB;

#define OUT_DIR  "/data/PS4LM"
#define OUT_FILE OUT_DIR "/memprobe.txt"
#define PROBE_FILE OUT_DIR "/memprobe-mmap.bin"

constexpr unsigned long long SINGLE_START = 6 * GiB; /* binary search starts here */
constexpr unsigned long long SINGLE_STEP = 64 * MiB; /* and stops at this resolution */
constexpr unsigned long long BLOCK = 256 * MiB;
constexpr int MAX_BLOCKS = 32; /* 8 GiB: all the console has */
constexpr unsigned long long MAP_SIZE = 1 * GiB;
constexpr size_t TOUCH_STEP = 4096;

/* libkernel, not declared by the SDK's headers. */
extern "C" {
struct sce_sw_version {
    size_t size;
    char version_string[0x1c];
    uint32_t version;
};
int sceKernelGetSystemSwVersion(struct sce_sw_version *out);
int sceKernelGetHwModelName(char *name);
long sceKernelGetCpuFrequency(void);
int sceKernelGetCpuTemperature(int *celsius);
size_t sceKernelGetDirectMemorySize(void);
int sceKernelAvailableFlexibleMemorySize(size_t *size_out);
int sceKernelConfiguredFlexibleMemorySize(size_t *size_out);
}

static FILE *g_log;

static void out(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void out(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);
    if (g_log) {
        va_start(ap, fmt);
        vfprintf(g_log, fmt, ap);
        va_end(ap);
        fflush(g_log);
        fsync(fileno(g_log));
    }
}

static double now_s() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static double gib(unsigned long long bytes) { return (double)bytes / (double)GiB; }

static void touch(void *p, size_t len) {
    volatile unsigned char *b = static_cast<unsigned char *>(p);
    for (size_t i = 0; i < len; i += TOUCH_STEP)
        b[i] = (unsigned char)i;
}

/* Results for the summary line. */
static struct {
    int allowed;
    unsigned long long single_max;
    unsigned long long total_max;
    bool mmap_ok;
    double memcpy_1, memcpy_n;
    double gflops_1, gflops_n;
    double data_free;
} g_sum;

/* ---- console and CPU --------------------------------------------------- */

static void probe_system() {
    out("== system\n");
    out("fw (kernel)   0x%08x\n", kernel_get_fw_version());

    struct sce_sw_version sw;
    memset(&sw, 0, sizeof(sw));
    sw.size = sizeof(sw);
    int rc = sceKernelGetSystemSwVersion(&sw);
    if (rc == 0)
        out("fw (system)   %s (0x%08x)\n", sw.version_string, sw.version);
    else
        out("fw (system)   unavailable (rc=0x%x)\n", rc);

    char model[256] = {0};
    if (sceKernelGetHwModelName(model) == 0)
        out("hw model      %s\n", model);
    out("cpu freq      %ld MHz\n", sceKernelGetCpuFrequency() / 1000000);
    int temp = 0;
    if (sceKernelGetCpuTemperature(&temp) == 0)
        out("cpu temp      %d C\n", temp);
    out("pid/uid       %d/%d\n", (int)getpid(), (int)getuid());

    size_t flex_cfg = 0, flex_free = 0;
    out("direct mem    %.2f GiB (whole console)\n", gib(sceKernelGetDirectMemorySize()));
    rc = sceKernelConfiguredFlexibleMemorySize(&flex_cfg);
    out("flexible cfg  %.2f GiB (rc=0x%x)\n", gib(flex_cfg), rc);
    rc = sceKernelAvailableFlexibleMemorySize(&flex_free);
    out("flexible free %.2f GiB (rc=0x%x)\n", gib(flex_free), rc);
}

static void probe_cpuid() {
    unsigned a, b, c, d;
    char vendor[13] = {0};
    char brand[49] = {0};

    out("== cpuid\n");
    __get_cpuid(0, &a, &b, &c, &d);
    memcpy(vendor + 0, &b, 4);
    memcpy(vendor + 4, &d, 4);
    memcpy(vendor + 8, &c, 4);
    for (unsigned leaf = 0; leaf < 3; leaf++) {
        __get_cpuid(0x80000002 + leaf, &a, &b, &c, &d);
        memcpy(brand + leaf * 16 + 0, &a, 4);
        memcpy(brand + leaf * 16 + 4, &b, 4);
        memcpy(brand + leaf * 16 + 8, &c, 4);
        memcpy(brand + leaf * 16 + 12, &d, 4);
    }
    out("cpu           %s (%s)\n", brand, vendor);

    __get_cpuid(1, &a, &b, &c, &d);
    const int sse42 = (c >> 20) & 1, avx = (c >> 28) & 1, fma = (c >> 12) & 1, f16c = (c >> 29) & 1;
    __get_cpuid_count(7, 0, &a, &b, &c, &d);
    const int avx2 = (b >> 5) & 1, bmi1 = (b >> 3) & 1, bmi2 = (b >> 8) & 1, avx512f = (b >> 16) & 1;
    out("features      sse4.2=%d avx=%d f16c=%d bmi1=%d avx2=%d fma=%d bmi2=%d avx512f=%d\n", sse42, avx,
        f16c, bmi1, avx2, fma, bmi2, avx512f);
    out("expected      sse4.2=1 avx=1 f16c=1 bmi1=1 avx2=0 fma=0 bmi2=0 avx512f=0\n");
}

static void probe_cpus() {
    out("== cpus\n");
    int ncpu = 0;
    size_t len = sizeof(ncpu);
    if (sysctlbyname("hw.ncpu", &ncpu, &len, nullptr, 0) == 0)
        out("sysctl hw.ncpu         %d\n", ncpu);
    else
        out("sysctl hw.ncpu         unavailable (errno %d)\n", errno);
    out("hardware_concurrency   %u\n", std::thread::hardware_concurrency());
    out("sysconf online         %ld\n", sysconf(_SC_NPROCESSORS_ONLN));

    cpuset_t mask;
    CPU_ZERO(&mask);
    int allowed = -1;
    if (cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID, -1, sizeof(mask), &mask) == 0) {
        allowed = 0;
        std::string list;
        for (int i = 0; i < CPU_SETSIZE; i++) {
            if (!CPU_ISSET(i, &mask))
                continue;
            allowed++;
            list += (list.empty() ? "" : ",") + std::to_string(i);
        }
        out("affinity mask          %d allowed: %s\n", allowed, list.c_str());
    } else {
        out("affinity mask          unavailable (errno %d)\n", errno);
    }
    g_sum.allowed = allowed > 0 ? allowed : 1;
}

/* ---- memory ------------------------------------------------------------ */

/* malloc only, no touching: a block the allocator hands out but the system
 * cannot back would kill the probe mid-search. */
static bool can_malloc(unsigned long long size) {
    void *p = malloc((size_t)size);
    if (!p)
        return false;
    free(p);
    return true;
}

static void probe_single() {
    out("== largest single malloc (binary search down from %.0f GiB, %llu MiB resolution)\n",
        gib(SINGLE_START), SINGLE_STEP / MiB);
    unsigned long long lo = 0, hi = SINGLE_START + SINGLE_STEP;
    if (can_malloc(SINGLE_START)) {
        lo = SINGLE_START;
    } else {
        while (hi - lo > SINGLE_STEP) {
            const unsigned long long mid = (lo + (hi - lo) / 2) & ~(SINGLE_STEP - 1);
            if (mid == lo)
                break;
            if (can_malloc(mid))
                lo = mid;
            else
                hi = mid;
        }
    }
    out("malloc says   %.2f GiB\n", gib(lo));

    /* Now touch every page of it. If this line is the last one in the log, the
     * allocator promised more than the system backs. */
    while (lo >= SINGLE_STEP) {
        out("touching      %.2f GiB ...\n", gib(lo));
        void *p = malloc((size_t)lo);
        if (p) {
            const double t0 = now_s();
            touch(p, (size_t)lo);
            out("touched       %.2f GiB in %.2f s\n", gib(lo), now_s() - t0);
            free(p);
            break;
        }
        lo -= SINGLE_STEP;
    }
    g_sum.single_max = lo;
    out("single max    %.2f GiB\n", gib(lo));
}

static void probe_total() {
    void *blocks[MAX_BLOCKS] = {};
    int n = 0;

    out("== total malloc in %llu MiB blocks (pages touched)\n", BLOCK / MiB);
    for (; n < MAX_BLOCKS; n++) {
        blocks[n] = malloc(BLOCK);
        if (!blocks[n]) {
            out("  stop at %.2f GiB: malloc errno=%d\n", gib((unsigned long long)n * BLOCK), errno);
            break;
        }
        touch(blocks[n], BLOCK);
        out("  %.2f GiB allocated and touched\n", gib((unsigned long long)(n + 1) * BLOCK));
    }
    g_sum.total_max = (unsigned long long)n * BLOCK;
    out("total max     %.2f GiB\n", gib(g_sum.total_max));
    for (int i = 0; i < n; i++)
        free(blocks[i]);
}

/* ---- storage ----------------------------------------------------------- */

static double free_gib(const char *path) {
    struct statfs fs;
    if (statfs(path, &fs) != 0)
        return -1;
    return gib((unsigned long long)fs.f_bavail * fs.f_bsize);
}

static void probe_storage() {
    out("== storage\n");
    for (const char *path : {"/data", "/user", "/mnt/usb0", "/mnt/ext0"}) {
        struct statfs fs;
        if (statfs(path, &fs) != 0)
            continue;
        out("%-13s %s, %.1f GiB free of %.1f GiB\n", path, fs.f_fstypename,
            gib((unsigned long long)fs.f_bavail * fs.f_bsize), gib((unsigned long long)fs.f_blocks * fs.f_bsize));
    }
    g_sum.data_free = free_gib("/data");
}

/* llama.cpp maps the model file by default: write a 1 GiB file, map it, read
 * every page back and check the pattern, then delete it. */
static void probe_mmap() {
    out("== mmap of a %.0f GiB file in %s\n", gib(MAP_SIZE), OUT_DIR);
    if (g_sum.data_free >= 0 && g_sum.data_free < gib(MAP_SIZE) + 1.0) {
        out("skipped: only %.1f GiB free under /data\n", g_sum.data_free);
        return;
    }

    const size_t chunk = 16 * MiB;
    std::vector<uint32_t> buf(chunk / sizeof(uint32_t));
    int fd = open(PROBE_FILE, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        out("open failed, errno=%d\n", errno);
        return;
    }
    double t0 = now_s();
    bool ok = true;
    for (unsigned long long off = 0; off < MAP_SIZE && ok; off += chunk) {
        for (size_t i = 0; i < buf.size(); i++)
            buf[i] = (uint32_t)((off / sizeof(uint32_t)) + i);
        ok = write(fd, buf.data(), chunk) == (ssize_t)chunk;
    }
    fsync(fd);
    double dt = now_s() - t0;
    if (!ok) {
        out("write failed, errno=%d\n", errno);
        close(fd);
        unlink(PROBE_FILE);
        return;
    }
    out("write         %.2f GiB in %.2f s: %.0f MB/s\n", gib(MAP_SIZE), dt, (double)MAP_SIZE / dt / 1e6);

    void *map = mmap(nullptr, (size_t)MAP_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        out("file mmap     FAILED, errno=%d (llama.cpp needs --no-mmap here)\n", errno);
    } else {
        const uint32_t *p = static_cast<const uint32_t *>(map);
        const size_t words = MAP_SIZE / sizeof(uint32_t);
        size_t bad = 0;
        t0 = now_s();
        for (size_t i = 0; i < words; i += TOUCH_STEP / sizeof(uint32_t))
            bad += p[i] != (uint32_t)i;
        dt = now_s() - t0;
        g_sum.mmap_ok = bad == 0;
        out("file mmap     %s, %.2f GiB faulted in %.2f s: %.0f MB/s, %zu bad pages\n", bad ? "WRONG DATA" : "ok",
            gib(MAP_SIZE), dt, (double)MAP_SIZE / dt / 1e6, bad);
        munmap(map, (size_t)MAP_SIZE);
    }
    close(fd);
    unlink(PROBE_FILE);
}

/* ---- bandwidth and compute --------------------------------------------- */

/* memcpy over a buffer split across t threads; counts bytes copied. */
static double memcpy_gbs(unsigned char *src, unsigned char *dst, size_t len, int t) {
    const int passes = 4;
    const size_t slice = (len / (size_t)t) & ~(size_t)4095;
    std::vector<std::thread> th;
    const double t0 = now_s();
    for (int i = 0; i < t; i++)
        th.emplace_back([=] {
            for (int p = 0; p < passes; p++)
                memcpy(dst + (size_t)i * slice, src + (size_t)i * slice, slice);
        });
    for (auto &x : th)
        x.join();
    return (double)slice * t * passes / (now_s() - t0) / 1e9;
}

static void probe_memcpy() {
    out("== memcpy bandwidth\n");
    size_t len = 512 * MiB;
    unsigned char *src = nullptr, *dst = nullptr;
    while (len >= 64 * MiB) {
        src = static_cast<unsigned char *>(malloc(len));
        dst = static_cast<unsigned char *>(malloc(len));
        if (src && dst)
            break;
        free(src);
        free(dst);
        src = dst = nullptr;
        len /= 2;
    }
    if (!src) {
        out("no buffers\n");
        return;
    }
    memset(src, 1, len);
    memset(dst, 2, len);
    out("buffer        2 x %llu MiB\n", (unsigned long long)len / MiB);

    std::vector<int> counts = {1, std::max(1, g_sum.allowed - 2), g_sum.allowed};
    counts.erase(std::unique(counts.begin(), counts.end()), counts.end());
    for (int t : counts) {
        const double gbs = memcpy_gbs(src, dst, len, t);
        out("memcpy        %d threads: %.2f GB/s\n", t, gbs);
        if (t == 1)
            g_sum.memcpy_1 = gbs;
        g_sum.memcpy_n = gbs;
    }
    free(src);
    free(dst);
}

/* An L1-resident dot product with AVX mul + add (Jaguar has no FMA, and runs
 * each 256-bit op as two 128-bit halves). Four accumulators hide the add
 * latency. Returns GFLOPS for one thread. */
static double dot_gflops(long reps) {
    constexpr int N = 2048; /* 2 x 8 KiB: fits Jaguar's 32 KiB L1D */
    alignas(32) static thread_local float a[N], b[N];
    for (int i = 0; i < N; i++) {
        a[i] = 1.0f + (float)i * 1e-6f;
        b[i] = 1.0f - (float)i * 1e-6f;
    }
    __m256 s0 = _mm256_setzero_ps(), s1 = s0, s2 = s0, s3 = s0;
    const double t0 = now_s();
    for (long r = 0; r < reps; r++) {
        for (int i = 0; i < N; i += 32) {
            s0 = _mm256_add_ps(s0, _mm256_mul_ps(_mm256_load_ps(a + i), _mm256_load_ps(b + i)));
            s1 = _mm256_add_ps(s1, _mm256_mul_ps(_mm256_load_ps(a + i + 8), _mm256_load_ps(b + i + 8)));
            s2 = _mm256_add_ps(s2, _mm256_mul_ps(_mm256_load_ps(a + i + 16), _mm256_load_ps(b + i + 16)));
            s3 = _mm256_add_ps(s3, _mm256_mul_ps(_mm256_load_ps(a + i + 24), _mm256_load_ps(b + i + 24)));
        }
        __asm__ volatile("" : "+x"(s0), "+x"(s1), "+x"(s2), "+x"(s3));
    }
    const double dt = now_s() - t0;
    float lanes[8];
    _mm256_storeu_ps(lanes, _mm256_add_ps(_mm256_add_ps(s0, s1), _mm256_add_ps(s2, s3)));
    volatile float sink = lanes[0];
    (void)sink;
    return (double)reps * N * 2 / dt / 1e9;
}

static void probe_compute() {
    out("== AVX fp32 dot product (mul + add, no FMA)\n");
    const long reps = 200000; /* about 0.4 G mul-adds a thread */
    g_sum.gflops_1 = dot_gflops(reps);
    out("dot           1 thread:  %.2f GFLOPS\n", g_sum.gflops_1);

    const int t = g_sum.allowed;
    if (t > 1) {
        std::vector<double> res(t);
        std::vector<std::thread> th;
        for (int i = 0; i < t; i++)
            th.emplace_back([&res, i] { res[i] = dot_gflops(reps); });
        for (auto &x : th)
            x.join();
        double sum = 0;
        for (double r : res)
            sum += r;
        g_sum.gflops_n = sum;
        out("dot           %d threads: %.2f GFLOPS total\n", t, sum);
    }
}

/* ---- main ---------------------------------------------------------------- */

int main() {
    mkdir(OUT_DIR, 0777);
    g_log = fopen(OUT_FILE, "w");

    out("PS4LM memprobe\n");
    probe_system();
    probe_cpuid();
    probe_cpus();
    probe_storage();
    probe_single();
    probe_total();
    probe_mmap();
    probe_memcpy();
    probe_compute();

    out("== summary\n");
    out("cpus allowed %d, single malloc %.2f GiB, total malloc %.2f GiB, 1 GiB file mmap %s\n", g_sum.allowed,
        gib(g_sum.single_max), gib(g_sum.total_max), g_sum.mmap_ok ? "ok" : "no");
    out("memcpy %.2f GB/s (1 thread), %.2f GB/s (%d threads); AVX dot %.2f GFLOPS (1 thread), %.2f (%d threads)\n",
        g_sum.memcpy_1, g_sum.memcpy_n, g_sum.allowed, g_sum.gflops_1, g_sum.gflops_n, g_sum.allowed);
    out("/data free %.1f GiB\n", g_sum.data_free);
    out("log: %s\n", g_log ? OUT_FILE : "(not written)");

    if (g_log)
        fclose(g_log);
    return 0;
}
