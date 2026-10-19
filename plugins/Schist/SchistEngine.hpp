// Schist — multiband dynamics engine: up to six freely placed bands, each a compressor or expander
// working downward or upward, in three processing modes (dynamic phase, linear phase, minimum phase).
// Clean-room implementation from public, generic techniques: Butterworth higher-order shelving filters
// built from TPT state-variable sections (Zavalishin; shelf form after Simper's published SVF formulas),
// Linkwitz-Riley crossovers with allpass phase compensation, overlap-save FFT convolution with
// window-designed zero-phase FIR band filters, RMS level detection with soft-knee gain computers,
// windowed-sinc halfband oversampling. No third-party product code, assets or disassembly were used.
// License: MIT (see LICENSE).

#pragma once
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <vector>

namespace schist {

static constexpr float kPi = 3.14159265358979f;
inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float dbToGain(float db) { return std::exp(db * 0.11512925465f); }
inline float gainToDb(float g) { return 20.f * std::log10(std::max(g, 1e-9f)); }

static constexpr int kMaxBands = 6, kMaxEdges = 2 * kMaxBands;
static constexpr int kCtl = 16;            // band layout and smoothing targets update every kCtl samples
static constexpr int kCoef = 4;            // dynamic-phase filter gains update every kCoef samples
static constexpr float kFreqMin = 10.f, kFreqMax = 30000.f, kLookaheadMs = 20.f;

// ------------------------------------------------------------------ params (append-only)
enum BandField { kBUsed, kBBypass, kBSolo, kBLo, kBHi, kBSlope, kBMode, kBThreshold, kBRange, kBRatio, kBKnee,
                 kBAttack, kBRelease, kBLookahead, kBGain, kBLink, kBStereo, kBSidechain, kBDetCustom, kBDetLo, kBDetHi, kBandFields };
enum Param : uint32_t {
    kProcMode, kOversampling, kMix, kOutGain, kOutPan, kExpert, kAnaMode, kAnaFreeze, kDisplayRange, kMidiOn,
    kBandFirst,
    kOutLearn = kBandFirst + kMaxBands * kBandFields,
    kParamCount
};
inline uint32_t bp(int b, int f) { return kBandFirst + b * kBandFields + f; }
enum ProcMode { kDynamicPhase, kLinearPhase, kMinimumPhase };
enum BandStereo { kStereo, kMid, kSide };
enum BandMode { kCompress, kExpand };

struct Range { float min, max, def; bool log, integer; };
inline Range paramRange(uint32_t p) {
    if (p < kBandFirst) {
        static const Range g[kBandFirst] = {
            { 0, 2, 0, false, true }, { 0, 2, 0, false, true }, { 0, 2, 1, false, false }, { -36, 36, 0, false, false },
            { -1, 1, 0, false, false }, { 0, 1, 0, false, true }, { 0, 3, 3, false, true }, { 0, 1, 0, false, true },
            { 0, 2, 1, false, true }, { 0, 1, 1, false, true } };
        return g[p];
    }
    if (p < kOutLearn) {
        static const Range b[kBandFields] = {
            { 0, 1, 0, false, true }, { 0, 1, 0, false, true }, { 0, 1, 0, false, true },
            { kFreqMin, kFreqMax, 200, true, false }, { kFreqMin, kFreqMax, 2000, true, false }, { 0, 7, 3, false, true },
            { 0, 1, 0, false, true }, { -80, 0, -24, false, false }, { -48, 48, -12, false, false }, { 1, 100, 3, true, false },
            { 0, 30, 10, false, false }, { 0.01f, 250, 10, true, false }, { 5, 5000, 120, true, false }, { 0, kLookaheadMs, 0, false, false },
            { -30, 30, 0, false, false }, { 0, 1, 1, false, false }, { 0, 2, 0, false, true }, { 0, 1, 0, false, true },
            { 0, 1, 0, false, true }, { kFreqMin, kFreqMax, 200, true, false }, { kFreqMin, kFreqMax, 2000, true, false } };
        return b[(p - kBandFirst) % kBandFields];
    }
    return { 0, 10000000, 0, false, true };
}
inline float normOf(uint32_t p, float v) {
    const Range r = paramRange(p);
    if (r.log) return std::log(clampf(v, r.min, r.max) / r.min) / std::log(r.max / r.min);
    return (clampf(v, r.min, r.max) - r.min) / (r.max - r.min);
}
inline float valueOf(uint32_t p, float n) {
    const Range r = paramRange(p); n = clampf(n, 0.f, 1.f);
    float v = r.log ? r.min * std::pow(r.max / r.min, n) : r.min + n * (r.max - r.min);
    if (r.integer) v = std::round(v);
    return v;
}

// slopes are stored as 0..7 = 6..48 dB/oct; the order T is the slope in 6 dB steps
inline int slopeOrder(float v) { return (int)clampf(std::round(v), 0, 7) + 1; }
// a band edge at the very bottom or top of the range is "open": the band then works as a shelf
inline bool openLo(float lo) { return lo <= kFreqMin * 1.05f; }
inline bool openHi(float hi) { return hi >= 28000.f; }
// the band's centre (geometric), used for timing and detection
inline float bandCentre(float lo, float hi) {
    const bool ol = openLo(lo), oh = openHi(hi);
    if (ol && oh) return 1000.f;
    if (ol) return std::sqrt(20.f * hi);
    if (oh) return std::sqrt(lo * 20000.f);
    return std::sqrt(lo * hi);
}

// ------------------------------------------------------------------ response helpers (display and tests)
// order-T Butterworth low shelf at fe, gain gDb at DC, unity above (analog prototype, symmetric around fe)
inline float shelfDb(float f, float fe, float gDb, int T) {
    const float w2 = std::pow(f / fe, 2.f * T), G = dbToGain(gDb);
    return 10.f * std::log10((w2 + G) / (w2 + 1.f / G));
}
// dynamic phase: a band is a low shelf at its top edge times an inverse low shelf at its bottom edge
inline float bandDynDb(float f, float lo, float hi, int T, float gDb) {
    float db = 0;
    if (!openHi(hi)) db += shelfDb(f, hi, gDb, T); else db += gDb;
    if (!openLo(lo)) db += shelfDb(f, lo, -gDb, T);
    return db;
}
// linear and minimum phase: amplitude-complementary (Linkwitz-Riley style) band shape, 0..1
inline float lrLow(float f, float fc, int T) { return 1.f / (1.f + std::pow(f / fc, (float)T)); }
inline float bandMag(float f, float lo, float hi, int T) {
    float m = 1;
    if (!openLo(lo)) m *= 1.f - lrLow(f, lo, T);
    if (!openHi(hi)) m *= lrLow(f, hi, T);
    return m;
}
inline float bandSplitDb(float f, float lo, float hi, int T, float gDb) { return gainToDb(std::max(1e-6f, 1.f + (dbToGain(gDb) - 1.f) * bandMag(f, lo, hi, T))); }

// soft-knee gain computer: how far the level is past a point (quadratic knee of width K)
inline float pastKnee(float d, float K) {
    if (K < 0.01f) return d > 0 ? d : 0.f;
    if (d <= -0.5f * K) return 0.f;
    if (d >= 0.5f * K) return d;
    const float t = d + 0.5f * K; return t * t / (2.f * K);
}
// gain change in dB for a detected level. range < 0 works downward (cuts), range > 0 upward (boosts)
inline float gainChange(float lvl, float thr, float ratio, float knee, int mode, float range) {
    if (std::fabs(range) < 1e-4f) return 0.f;
    ratio = std::max(ratio, 1.f);
    if (mode == kCompress) {
        if (range < 0) return std::max(-pastKnee(lvl - thr, knee) * (1.f - 1.f / ratio), range);   // loud parts down
        return std::min(pastKnee(thr - lvl, knee) * (1.f - 1.f / ratio), range);                  // quiet parts up
    }
    if (range < 0) return std::max(-pastKnee(thr - lvl, knee) * (ratio - 1.f), range);            // quiet parts down (gate)
    return std::min(pastKnee(lvl - thr, knee) * (ratio - 1.f), range);                            // loud parts up
}
// the static curve the UI draws: output level for an input level
inline float staticOut(float lvl, float thr, float ratio, float knee, int mode, float range) { return lvl + gainChange(lvl, thr, ratio, knee, mode, range); }

// ------------------------------------------------------------------ filters
// TPT state-variable filter step: v1 = band, v2 = low
struct SvfS { float ic1 = 0, ic2 = 0; };
inline void svfTick(SvfS& s, float x, float a1, float a2, float a3, float& v1, float& v2) {
    const float v3 = x - s.ic2;
    v1 = a1 * s.ic1 + a2 * v3; v2 = s.ic2 + a2 * s.ic1 + a3 * v3;
    s.ic1 = 2.f * v1 - s.ic1; s.ic2 = 2.f * v2 - s.ic2;
}
inline float prewarp(float fc, float fs) { return std::tan(kPi * clampf(fc, 5.f, 0.47f * fs) / fs); }
// Butterworth damping of section m in an order-N filter (second-order sections; odd orders add a one-pole)
inline float bwDamping(int N, int m) { return (N & 1) ? 2.f * std::cos((m + 1) * kPi / N) : 2.f * std::cos((2 * m + 1) * kPi / (2 * N)); }

// order-N Butterworth (N 1..4) low/high/allpass sections
struct BwCoef {
    int n2 = 0; bool odd = false; float a1[2] = {}, a2[2] = {}, a3[2] = {}, k[2] = {}, g1 = 0;
    void design(float fc, float fs, int N) {
        const float g = prewarp(fc, fs); n2 = N / 2; odd = (N & 1) != 0;
        for (int m = 0; m < n2; ++m) { k[m] = bwDamping(N, m); a1[m] = 1.f / (1.f + g * (g + k[m])); a2[m] = g * a1[m]; a3[m] = g * a2[m]; }
        g1 = g / (1.f + g);
    }
};
struct BwState { SvfS s[2]; float z = 0; void reset() { s[0] = s[1] = SvfS(); z = 0; } };
inline float bwLow(const BwCoef& c, BwState& st, float x) {
    float v1, v2;
    for (int m = 0; m < c.n2; ++m) { svfTick(st.s[m], x, c.a1[m], c.a2[m], c.a3[m], v1, v2); x = v2; }
    if (c.odd) { const float v = (x - st.z) * c.g1, lp = v + st.z; st.z = lp + v; x = lp; }
    return x;
}
inline float bwHigh(const BwCoef& c, BwState& st, float x) {
    float v1, v2;
    for (int m = 0; m < c.n2; ++m) { svfTick(st.s[m], x, c.a1[m], c.a2[m], c.a3[m], v1, v2); x = x - c.k[m] * v1 - v2; }
    if (c.odd) { const float v = (x - st.z) * c.g1, lp = v + st.z; st.z = lp + v; x = x - lp; }
    return x;
}
inline float bwAllpass(const BwCoef& c, BwState& st, float x) {
    float v1, v2;
    for (int m = 0; m < c.n2; ++m) { svfTick(st.s[m], x, c.a1[m], c.a2[m], c.a3[m], v1, v2); x = x - 2.f * c.k[m] * v1; }
    if (c.odd) { const float v = (x - st.z) * c.g1, lp = v + st.z; st.z = lp + v; x = 2.f * lp - x; }
    return x;
}

// order-T Butterworth low shelf (T 1..8) whose gain changes at run time: T/2 second-order shelf
// sections plus a first-order one for odd T. At 0 dB every section is an exact wire.
struct ShelfCoef {
    int T = 1, n2 = 0; bool odd = true; float w = 0.1f, k[4] = {};
    float a1[4] = {}, a2[4] = {}, a3[4] = {}, m1[4] = {}, m2[4] = {}, g1 = 0, m1o = 0;
    void design(float fc, float fs, int order) {
        T = order; n2 = T / 2; odd = (T & 1) != 0; w = prewarp(fc, fs);
        for (int m = 0; m < n2; ++m) k[m] = bwDamping(T, m);
    }
    void setGain(float gDb) {
        const float a = dbToGain(gDb / T), sa = std::sqrt(a), g = w / sa;     // per-pole gain a; poles at w/sqrt(a), zeros at w*sqrt(a)
        for (int m = 0; m < n2; ++m) { a1[m] = 1.f / (1.f + g * (g + k[m])); a2[m] = g * a1[m]; a3[m] = g * a2[m]; m1[m] = k[m] * (a - 1.f); m2[m] = a * a - 1.f; }
        g1 = g / (1.f + g); m1o = a - 1.f;
    }
};
struct ShelfState { SvfS s[4]; float z = 0; void reset() { for (auto& x : s) x = SvfS(); z = 0; } };
inline float runShelf(const ShelfCoef& c, ShelfState& st, float x) {
    float v1, v2;
    for (int m = 0; m < c.n2; ++m) { svfTick(st.s[m], x, c.a1[m], c.a2[m], c.a3[m], v1, v2); x += c.m1[m] * v1 + c.m2[m] * v2; }
    if (c.odd) { const float v = (x - st.z) * c.g1, lp = v + st.z; st.z = lp + v; x += c.m1o * lp; }
    return x;
}

// ------------------------------------------------------------------ halfband oversampling (65 taps, Kaiser)
static constexpr int kHbHalf = 16, kHbC = 2 * kHbHalf;
inline const float* halfbandTaps() {
    static float t[kHbHalf]; static bool done = false;
    if (!done) {
        done = true;
        auto bessel0 = [](double x) { double s = 1, term = 1; for (int k = 1; k < 30; ++k) { term *= (x / (2 * k)) * (x / (2 * k)); s += term; } return s; };
        const double beta = 8.0, N = 2 * kHbC, PI = 3.14159265358979323846; double sum = 0.5;
        for (int j = 0; j < kHbHalf; ++j) {
            const int k = 2 * j + 1; const double sinc = std::sin(PI * k / 2.0) / (PI * k / 2.0);
            const double r = (double)(k + kHbC) / N * 2 - 1, w = bessel0(beta * std::sqrt(std::max(0.0, 1 - r * r))) / bessel0(beta);
            t[j] = (float)(0.5 * sinc * w); sum += 2 * t[j];
        }
        for (int j = 0; j < kHbHalf; ++j) t[j] = (float)(t[j] / sum);
    }
    return t;
}
struct HbUp {
    float buf[128] = {}; int pos = 0;
    void reset() { std::memset(buf, 0, sizeof buf); pos = 0; }
    inline void process(float x, float& y0, float& y1) {
        pos = (pos + 1) & 63; buf[pos] = buf[pos + 64] = x;
        const float* h = halfbandTaps(); const float* b = buf + pos + 64;
        y0 = b[-kHbHalf]; float s = 0;
        for (int j = 0; j < kHbHalf; ++j) s += h[j] * (b[-kHbHalf - j] + b[-kHbHalf + j + 1]);
        y1 = 2.f * s;
    }
};
struct HbDown {
    float buf[256] = {}; int pos = 0;
    void reset() { std::memset(buf, 0, sizeof buf); pos = 0; }
    inline float process(float x0, float x1) {
        pos = (pos + 1) & 127; buf[pos] = buf[pos + 128] = x0;
        pos = (pos + 1) & 127; buf[pos] = buf[pos + 128] = x1;
        const float* h = halfbandTaps(); const float* c = buf + pos + 128 - kHbC - 1;
        float s = 0.5f * c[0];
        for (int j = 0; j < kHbHalf; ++j) s += h[j] * (c[-(2 * j + 1)] + c[2 * j + 1]);
        return s;
    }
};
// off, 2x or 4x; latency in base-rate samples: total 0 / 32 / 48, of which the up-sampler is half
struct Oversampler {
    HbUp u[2]; HbDown d[2];
    void reset() { for (int i = 0; i < 2; ++i) { u[i].reset(); d[i].reset(); } }
    static int latency(int os) { return os == 1 ? 0 : (os == 2 ? 32 : 48); }
    static int upLatency(int os) { return latency(os) / 2; }
    inline void up(float x, int os, float* y) {
        if (os == 1) { y[0] = x; return; }
        float a, b; u[0].process(x, a, b);
        if (os == 2) { y[0] = a; y[1] = b; return; }
        u[1].process(a, y[0], y[1]); u[1].process(b, y[2], y[3]);
    }
    inline float down(const float* y, int os) {
        if (os == 1) return y[0];
        if (os == 2) return d[0].process(y[0], y[1]);
        const float a = d[1].process(y[0], y[1]), b = d[1].process(y[2], y[3]);
        return d[0].process(a, b);
    }
};

// ------------------------------------------------------------------ FFT (radix-2, complex, in place)
typedef std::complex<float> cpx;
class Fft {
public:
    void init(int n) {
        if (n == size) return;
        size = n; twr.resize(n / 2); twi.resize(n / 2); rev.resize(n);
        for (int i = 0; i < n / 2; ++i) { const double a = -2.0 * 3.14159265358979323846 * i / n; twr[i] = (float)std::cos(a); twi[i] = (float)std::sin(a); }
        int bits = 0; while ((1 << bits) < n) ++bits;
        for (int i = 0; i < n; ++i) { int r = 0; for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b); rev[i] = r; }
    }
    int n() const { return size; }
    // inverse: conjugate twiddles, scaled by 1/n
    void run(cpx* xc, bool inverse) const {
        float* x = reinterpret_cast<float*>(xc);
        for (int i = 0; i < size; ++i) if (rev[i] > i) std::swap(xc[i], xc[rev[i]]);
        const float sg = inverse ? -1.f : 1.f;
        for (int len = 2; len <= size; len <<= 1) {
            const int half = len >> 1, step = size / len;
            for (int i = 0; i < size; i += len)
                for (int j = 0; j < half; ++j) {
                    const float wr = twr[j * step], wi = sg * twi[j * step];
                    float* u = x + 2 * (i + j); float* v = x + 2 * (i + j + half);
                    const float vr = v[0] * wr - v[1] * wi, vi = v[0] * wi + v[1] * wr;
                    v[0] = u[0] - vr; v[1] = u[1] - vi; u[0] += vr; u[1] += vi;
                }
        }
        if (inverse) { const float s = 1.f / size; for (int i = 0; i < 2 * size; ++i) x[i] *= s; }
    }
private:
    int size = 0;
    std::vector<float> twr, twi;
    std::vector<int> rev;
};

// ------------------------------------------------------------------ engine
class Engine {
public:
    float p[kParamCount];
    // read by the UI through direct access
    float gcNow[kMaxBands] = {};                 // current dynamic gain change per band, dB (the larger of the two channels)
    float levelNow[kMaxBands] = {};              // detected level per band, dB
    float meterIn[2] = {}, meterOut[2] = {};     // peak, linear, decaying
    static constexpr int kAna = 8192;
    std::vector<float> anaIn, anaOut;
    struct AtomicIndex {
        std::atomic<uint32_t> v { 0 };
        AtomicIndex() {}
        AtomicIndex(const AtomicIndex& o) : v(o.v.load()) {}
        AtomicIndex& operator=(const AtomicIndex& o) { v.store(o.v.load()); return *this; }
        uint32_t load() const { return v.load(std::memory_order_acquire); }
        void store(uint32_t x) { v.store(x, std::memory_order_release); }
    } anaWrite;

    Engine() {
        for (uint32_t i = 0; i < kParamCount; ++i) p[i] = paramRange(i).def;
        p[kOutLearn] = 0;
        anaIn.assign(kAna, 0.f); anaOut.assign(kAna, 0.f);
    }
    // a band with sensible defaults, for presets and the UI
    void setBand(int b, float lo, float hi) { p[bp(b, kBUsed)] = 1; p[bp(b, kBLo)] = lo; p[bp(b, kBHi)] = hi; p[bp(b, kBDetLo)] = lo; p[bp(b, kBDetHi)] = hi; }

    void setSampleRate(float sr) {
        fs = sr;
        const int scale = sr <= 50000.f ? 1 : (sr <= 100000.f ? 2 : 4);
        linL = 4096 * scale; linB = linL / 2; linN = 2 * linL;
        fftN.init(linN); fftL.init(linL);
        int hn = 1; while (hn < (int)(kLookaheadMs * 0.001f * sr) + linL + linB + 512) hn <<= 1;
        histN = hn; for (auto& h : hist) h.assign(histN, 0.f);
        ring.assign(linN, cpx(0, 0)); work.assign(linN, cpx(0, 0)); work2.assign(linN, cpx(0, 0)); ms.assign(linN, cpx(0, 0));
        for (int b = 0; b < kMaxBands; ++b) { kern[b].assign(linN, cpx(0, 0)); kernOld[b].assign(linN, cpx(0, 0)); fifo[b].assign(linB, cpx(0, 0)); kernOk[b] = false; }
        design.assign(linL, cpx(0, 0));
        configure();
    }
    void reset() { configure(); }
    uint32_t latency() const { return (uint32_t)(laSamples + (mode == kLinearPhase ? linB + linL / 2 : Oversampler::latency(os))); }
    float sampleRate() const { return fs; }
    int activeMode() const { return mode; }
    int activeOs() const { return os; }

    // ---------------------------------------------------------------- render
    void process(const float* inL, const float* inR, const float* scL, const float* scR, float* outL, float* outR, uint32_t n) {
        const bool haveSc = scL && scR;
        for (uint32_t i = 0; i < n; ++i) {
            if (--ctl <= 0) { ctl = kCtl; control(); }
            // history: main input and sidechain, read back with the delays below
            hist[0][hp] = inL[i]; hist[1][hp] = inR[i]; hist[2][hp] = haveSc ? scL[i] : 0.f; hist[3][hp] = haveSc ? scR[i] : 0.f;
            for (int c = 0; c < 2; ++c) meterIn[c] = std::max(std::fabs(c ? inR[i] : inL[i]), meterIn[c] * meterDecay);

            detect();
            if (--coefCount <= 0) { coefCount = kCoef; if (mode == kDynamicPhase) updateShelves(); }

            // the signal to process, delayed by the look-ahead
            const int mainD = laSamples;
            const float u[2] = { hist[0][(hp - mainD) & histMask], hist[1][(hp - mainD) & histMask] };
            float wet[2];
            if (mode == kLinearPhase) linearSample(u, wet);
            else {
                float up[2][4], y[2][4];
                ovs[0].up(u[0], os, up[0]); ovs[1].up(u[1], os, up[1]);
                for (int k = 0; k < os; ++k) { float x[2] = { up[0][k], up[1][k] }; if (mode == kDynamicPhase) dynamicOs(x); else minimumOs(x); y[0][k] = x[0]; y[1][k] = x[1]; }
                wet[0] = ovs[0].down(y[0], os); wet[1] = ovs[1].down(y[1], os);
            }
            // solo: listen to the soloed bands only
            const float soloT = anySolo ? 1.f : 0.f; sSolo += (soloT - sSolo) * smooth;
            if (sSolo > 1e-4f) { float s[2]; soloFilter(wet, s); wet[0] += (s[0] - wet[0]) * sSolo; wet[1] += (s[1] - wet[1]) * sSolo; }
            // global mix against the dry signal, aligned with the latency; output gain and balance
            const int lat = (int)latency();
            const float dry[2] = { hist[0][(hp - lat) & histMask], hist[1][(hp - lat) & histMask] };
            sMix += (tMix - sMix) * smooth; sOut += (tOut - sOut) * smooth; sPanL += (tPanL - sPanL) * smooth; sPanR += (tPanR - sPanR) * smooth;
            const float mixNow = sMix * (1.f - sSolo) + sSolo;      // solo listens to the processed band, not the mix
            for (int c = 0; c < 2; ++c) {
                float v = (dry[c] + (wet[c] - dry[c]) * mixNow) * sOut * (c ? sPanR : sPanL) * fadeGain;
                if (!(std::fabs(v) < 64.f)) v = std::isfinite(v) ? std::copysign(64.f, v) : 0.f;
                (c ? outR : outL)[i] = v;
                meterOut[c] = std::max(std::fabs(v), meterOut[c] * meterDecay);
            }
            const uint32_t w = anaWrite.load() % kAna;
            anaIn[w] = 0.5f * (inL[i] + inR[i]); anaOut[w] = 0.5f * (outL[i] + outR[i]); anaWrite.store(anaWrite.load() + 1);
            hp = (hp + 1) & histMask;
            if (fadeState == 1) { fadeGain -= 1.f / 96.f; if (fadeGain <= 0) { configure(); fadeGain = 0; fadeState = 2; } }
            else if (fadeState == 2) { fadeGain += 1.f / 96.f; if (fadeGain >= 1) { fadeGain = 1; fadeState = 0; } }
        }
        for (int b = 0; b < kMaxBands; ++b) {
            if (!(std::fabs(gc[b][0]) < 1e3f) || !(std::fabs(gc[b][1]) < 1e3f)) { gc[b][0] = gc[b][1] = 0; det[b][0] = det[b][1] = DetState(); }
            const float g0 = gc[b][0], g1 = gc[b][1]; gcNow[b] = std::fabs(g0) >= std::fabs(g1) ? g0 : g1;
            levelNow[b] = std::max(lvlDb[b][0], lvlDb[b][1]);
        }
    }

    // display: the gain (dB) the whole processor applies at f right now (including band gains)
    float responseDb(float f) const {
        if (mode == kDynamicPhase) {
            float db = 0;
            for (int b = 0; b < kMaxBands; ++b) if (bandOn[b]) db += bandDynDb(f, lo[b], hi[b], order[b], curDb[b]);
            return db;
        }
        float lin = 1;
        for (int b = 0; b < kMaxBands; ++b) if (bandOn[b]) lin += (dbToGain(curDb[b]) - 1.f) * bandMag(f, lo[b], hi[b], order[b]);
        return gainToDb(std::max(lin, 1e-6f));
    }
    float bandDb(int b) const { return curDb[b]; }       // dynamic + static gain currently applied by a band

private:
    struct DetState { BwState hpS[2], lpS[2]; float msq = 0; };
    struct EdgeCfg { float f; int T; };

    // ---- configuration: applied at start and whenever the structure changes (behind a short fade) ----
    void configure() {
        control();                                   // work out the wanted structure from the parameters
        mode = desMode; os = mode == kLinearPhase ? 1 : desOs; laSamples = desLa;
        numEdges = desEdges; for (int e = 0; e < numEdges; ++e) edges[e] = desEdge[e];
        for (int b = 0; b < kMaxBands; ++b) { used[b] = desUsed[b]; stereo[b] = desStereo[b]; }
        fsOs = fs * os;
        smooth = 1.f - std::exp(-1.f / (0.01f * fs));
        meterDecay = std::exp(-1.f / (0.3f * fs));
        histMask = histN - 1;
        for (auto& o : ovs) o.reset();
        for (int b = 0; b < kMaxBands; ++b) for (int c = 0; c < 2; ++c) { shS[b][c][0].reset(); shS[b][c][1].reset(); soloS[b][c][0].reset(); soloS[b][c][1].reset(); soloS[b][c][2].reset(); soloS[b][c][3].reset(); }
        for (int c = 0; c < 2; ++c) for (int e = 0; e < kMaxEdges; ++e) { for (int k = 0; k < 2; ++k) { spL[c][e][k].reset(); spH[c][e][k].reset(); } spZ[c][e] = 0; apS[c][e][0].reset(); }
        std::fill(ring.begin(), ring.end(), cpx(0, 0)); ringPos = 0; linPos = 0;
        for (int b = 0; b < kMaxBands; ++b) { std::fill(fifo[b].begin(), fifo[b].end(), cpx(0, 0)); kernOk[b] = false; }
        ctl = 0; coefCount = 0;
        control();
        sMix = tMix; sOut = tOut; sPanL = tPanL; sPanR = tPanR;
        for (int b = 0; b < kMaxBands; ++b) { sGain[b] = tGain[b]; sByp[b] = tByp[b]; }
        if (mode == kDynamicPhase) updateShelves();
        fadeState = 0; fadeGain = 1;
    }

    // ---- control rate: band layout, timing, smoothing targets ------------------------------
    void control() {
        desMode = (int)clampf(std::round(p[kProcMode]), 0, 2);
        const int o = (int)clampf(std::round(p[kOversampling]), 0, 2); desOs = o == 0 ? 1 : (o == 1 ? 2 : 4);
        bool la = false;
        anySolo = false;
        for (int b = 0; b < kMaxBands; ++b) {
            desUsed[b] = p[bp(b, kBUsed)] > 0.5f;
            desStereo[b] = (int)clampf(std::round(p[bp(b, kBStereo)]), 0, 2);
            if (desUsed[b] && p[bp(b, kBLookahead)] > 0.001f) la = true;
            if (desUsed[b] && p[bp(b, kBSolo)] > 0.5f) anySolo = true;
        }
        desLa = la ? (int)std::round(kLookaheadMs * 0.001f * fs) : 0;
        // band edges (sanitised: inside the range, at least a little apart)
        const float fsW = desMode == kLinearPhase ? fs : fs * desOs;
        for (int b = 0; b < kMaxBands; ++b) {
            float l = clampf(p[bp(b, kBLo)], kFreqMin, kFreqMax), h = clampf(p[bp(b, kBHi)], kFreqMin, kFreqMax);
            if (h < l * 1.05f) { if (openHi(l)) l = h / 1.05f; else h = l * 1.05f; }
            if (!openHi(h) && h > 0.45f * fsW) h = 28000.f;                  // above Nyquist: open
            lo[b] = l; hi[b] = h; order[b] = slopeOrder(p[bp(b, kBSlope)]);
            float dl = l, dh = h;
            if (p[bp(b, kBDetCustom)] > 0.5f) { dl = clampf(p[bp(b, kBDetLo)], kFreqMin, kFreqMax); dh = clampf(p[bp(b, kBDetHi)], kFreqMin, kFreqMax); if (dh < dl * 1.05f) dh = dl * 1.05f; }
            if (!openHi(dh) && dh > 0.45f * fs) dh = 28000.f;
            dLo[b] = dl; dHi[b] = dh;
        }
        if (desMode == kMinimumPhase) buildEdges(); else desEdges = 0;
        // anything that changes the processing structure fades out, switches, and fades back in
        bool change = desMode != mode || (desMode != kLinearPhase && desOs != os) || desLa != laSamples || desEdges != numEdges;
        for (int e = 0; e < desEdges && !change; ++e) change = desEdge[e].T != edges[e].T;
        for (int b = 0; b < kMaxBands && !change; ++b) change = desUsed[b] != used[b] || (used[b] && desStereo[b] != stereo[b]);
        if (change && fadeState == 0) fadeState = 1;
        if (desMode == kMinimumPhase && !change) for (int e = 0; e < numEdges; ++e) edges[e].f = desEdge[e].f;   // edges glide freely

        // per-band timing and smoothing targets
        for (int b = 0; b < kMaxBands; ++b) {
            bandOn[b] = used[b];
            if (!used[b]) continue;
            const float fc = bandCentre(dLo[b], dHi[b]);
            // frequency-dependent timing: low bands a little slower, never faster than a quarter cycle
            const float fdep = clampf(std::pow(400.f / fc, 0.25f), 0.75f, 1.5f);
            const float att = std::max(p[bp(b, kBAttack)] * fdep, 250.f / fc);
            attC[b] = 1.f - std::exp(-1.f / (att * 0.001f * fs));
            relMs[b] = p[bp(b, kBRelease)] * fdep;
            rmsC[b] = 1.f - std::exp(-1.f / (clampf(1.5f / fc, 0.0003f, 0.02f) * fs));
            for (int c = 0; c < 2; ++c) {
                // program-dependent release: brief overshoots recover quickly, sustained gain change slowly
                const float sus = clampf(std::fabs(gcSlow[b][c]) / std::max(std::fabs(gc[b][c]), 0.1f), 0.f, 1.f);
                relC[b][c] = 1.f - std::exp(-1.f / (relMs[b] * (0.35f + 0.65f * sus) * 0.001f * fs));
            }
            detHp[b].design(dLo[b], fs, 2); detLp[b].design(dHi[b], fs, 2); soloHp[b].design(lo[b], fs, 2); soloLp[b].design(hi[b], fs, 2);
            tGain[b] = p[bp(b, kBGain)]; tByp[b] = p[bp(b, kBBypass)] > 0.5f ? 1.f : 0.f;
            laOff[b] = laSamples - (int)std::round(clampf(p[bp(b, kBLookahead)], 0.f, kLookaheadMs) * 0.001f * fs);
            if (laOff[b] < 0) laOff[b] = 0;
            if (mode == kDynamicPhase) for (int c = 0; c < 2; ++c) { sh[b][c][0].design(hi[b], fsOs, order[b]); sh[b][c][1].design(lo[b], fsOs, order[b]); }
        }
        slowC = 1.f - std::exp(-1.f / (0.3f * fs));
        if (mode == kMinimumPhase) designEdges();
        tMix = clampf(p[kMix], 0.f, 2.f); tOut = dbToGain(p[kOutGain]);
        const float pan = clampf(p[kOutPan], -1.f, 1.f); tPanL = pan > 0 ? 1.f - pan : 1.f; tPanR = pan < 0 ? 1.f + pan : 1.f;
    }

    // ---- detection and gain computers (base rate) ----------------------------------------
    inline void detect() {
        for (int b = 0; b < kMaxBands; ++b) {
            if (!used[b]) { curDb[b] = 0; gainLin[b][0] = gainLin[b][1] = 1; continue; }
            // detector input, earlier than the audio by this band's look-ahead
            const int dd = laOff[b] + (mode == kLinearPhase ? linB + linL / 2 : Oversampler::upLatency(os));
            const int base = p[bp(b, kBSidechain)] > 0.5f ? 2 : 0;
            const int idx = (hp - dd) & histMask;
            float x[2] = { hist[base][idx], hist[base + 1][idx] };
            int nch = 2;
            if (stereo[b] == kMid) { x[0] = 0.5f * (x[0] + x[1]); nch = 1; }
            else if (stereo[b] == kSide) { x[0] = 0.5f * (x[0] - x[1]); nch = 1; }
            const float thr = p[bp(b, kBThreshold)], ratio = clampf(p[bp(b, kBRatio)], 1.f, 100.f), knee = clampf(p[bp(b, kBKnee)], 0.f, 30.f);
            const float range = clampf(p[bp(b, kBRange)], -48.f, 48.f); const int md = p[bp(b, kBMode)] > 0.5f ? kExpand : kCompress;
            const bool ol = openLo(dLo[b]), oh = openHi(dHi[b]);
            float target[2];
            for (int c = 0; c < nch; ++c) {
                DetState& d = det[b][c]; float s = x[c];
                if (!ol) { s = bwHigh(detHp[b], d.hpS[0], s); s = bwHigh(detHp[b], d.hpS[1], s); }
                if (!oh) { s = bwLow(detLp[b], d.lpS[0], s); s = bwLow(detLp[b], d.lpS[1], s); }
                d.msq += (s * s - d.msq) * rmsC[b];
                lvlDb[b][c] = 10.f * std::log10(std::max(2.f * d.msq, 1e-15f));        // RMS, scaled so a steady sine reads its peak
                target[c] = gainChange(lvlDb[b][c], thr, ratio, knee, md, range);
            }
            if (nch == 2) {
                const float link = clampf(p[bp(b, kBLink)], 0.f, 1.f);
                if (link > 0.f) { const float tl = gainChange(std::max(lvlDb[b][0], lvlDb[b][1]), thr, ratio, knee, md, range);
                    for (int c = 0; c < 2; ++c) target[c] += (tl - target[c]) * link; }
            } else { lvlDb[b][1] = lvlDb[b][0]; }
            sByp[b] += (tByp[b] - sByp[b]) * smooth; sGain[b] += (tGain[b] - sGain[b]) * smooth;
            for (int c = 0; c < nch; ++c) {
                float& g = gc[b][c];
                const bool away = std::fabs(target[c]) > std::fabs(g) && target[c] * g >= 0.f;
                g += (target[c] - g) * (away ? attC[b] : relC[b][c]);
                gcSlow[b][c] += (g - gcSlow[b][c]) * slowC;
            }
            if (nch == 1) { gc[b][1] = gc[b][0]; gcSlow[b][1] = gcSlow[b][0]; }
            const float k = 1.f - sByp[b];
            for (int c = 0; c < 2; ++c) { totDb[b][c] = (gc[b][c] + sGain[b]) * k; gainLin[b][c] = dbToGain(totDb[b][c]); }
            curDb[b] = std::fabs(totDb[b][0]) >= std::fabs(totDb[b][1]) ? totDb[b][0] : totDb[b][1];
        }
    }

    // ---- dynamic phase: shelving filters whose gain follows the gain computer ---------------
    void updateShelves() {
        for (int b = 0; b < kMaxBands; ++b) {
            if (!used[b]) continue;
            const int nch = stereo[b] == kStereo ? 2 : 1;
            for (int c = 0; c < nch; ++c) { sh[b][c][0].setGain(totDb[b][c]); sh[b][c][1].setGain(-totDb[b][c]); }
        }
    }
    inline float dynBand(int b, int c, float x) {
        const bool oh = openHi(hi[b]), ol = openLo(lo[b]);
        if (!oh) x = runShelf(sh[b][c][0], shS[b][c][0], x);
        if (!ol) x = runShelf(sh[b][c][1], shS[b][c][1], x);
        if (oh) x *= gainLin[b][c];
        return x;
    }
    inline void dynamicOs(float* x) {
        bool anyMs = false;
        for (int b = 0; b < kMaxBands; ++b) {
            if (!used[b]) continue;
            if (stereo[b] != kStereo) { anyMs = true; continue; }
            x[0] = dynBand(b, 0, x[0]); x[1] = dynBand(b, 1, x[1]);
        }
        if (!anyMs) return;
        float m = 0.5f * (x[0] + x[1]), s = 0.5f * (x[0] - x[1]);
        for (int b = 0; b < kMaxBands; ++b) {
            if (!used[b] || stereo[b] == kStereo) continue;
            if (stereo[b] == kMid) m = dynBand(b, 0, m); else s = dynBand(b, 0, s);
        }
        x[0] = m + s; x[1] = m - s;
    }

    // ---- minimum phase: Linkwitz-Riley crossovers at every band edge -------------------------
    void buildEdges() {
        EdgeCfg tmp[kMaxEdges]; int n = 0;
        for (int b = 0; b < kMaxBands; ++b) {
            if (!desUsed[b]) continue;
            if (!openLo(lo[b])) tmp[n++] = { lo[b], order[b] };
            if (!openHi(hi[b])) tmp[n++] = { hi[b], order[b] };
        }
        for (int i = 1; i < n; ++i) for (int j = i; j > 0 && tmp[j].f < tmp[j - 1].f; --j) std::swap(tmp[j], tmp[j - 1]);
        int m = 0;
        for (int i = 0; i < n; ++i) {
            if (m > 0 && tmp[i].f < desEdge[m - 1].f * 1.03f) { desEdge[m - 1].T = std::max(desEdge[m - 1].T, tmp[i].T); continue; }   // shared edge
            desEdge[m++] = tmp[i];
        }
        // the crossover order: 6 dB/oct is a complementary first-order split, otherwise LR 12/24/36/48
        for (int e = 0; e < m; ++e) desEdge[e].T = desEdge[e].T <= 1 ? 1 : 2 * ((desEdge[e].T + 1) / 2);
        desEdges = m;
    }
    void designEdges() {
        for (int e = 0; e < numEdges; ++e) { const int T = edges[e].T; edgeN[e] = T == 1 ? 0 : T / 2; if (edgeN[e]) xc[e].design(edges[e].f, fsOs, edgeN[e]); else xc[e].g1 = prewarp(edges[e].f, fsOs) / (1.f + prewarp(edges[e].f, fsOs)); }
        // which bands cover each segment between edges
        const int S = numEdges + 1;
        for (int sgi = 0; sgi < S; ++sgi) {
            const float a = sgi == 0 ? 10.f : edges[sgi - 1].f, z = sgi == numEdges ? 40000.f : edges[sgi].f, ctr = std::sqrt(a * z);
            segMask[sgi] = 0;
            for (int b = 0; b < kMaxBands; ++b) if (used[b] && (openLo(lo[b]) || lo[b] <= ctr) && (openHi(hi[b]) || ctr <= hi[b])) segMask[sgi] |= 1 << b;
        }
    }
    // Splits at each edge from the bottom up. Each low part is weighted by its segment's gain and added to an
    // accumulator that then passes the next edge's allpass, so every segment ends up with the same phase:
    // out = AP_E-1(...AP_2(AP_1(g0 L0) + g1 L1) + g2 L2 ...) + gE H. Only E-1 allpasses per channel.
    inline void segGains(int sgi, float& l, float& r) const {
        const int mask = segMask[sgi]; if (!mask) return;
        float gm = 1, gs = 1; bool msx = false;
        for (int b = 0; b < kMaxBands; ++b) {
            if (!(mask & (1 << b))) continue;
            if (stereo[b] == kStereo) { l *= gainLin[b][0]; r *= gainLin[b][1]; }
            else { msx = true; if (stereo[b] == kMid) gm *= gainLin[b][0]; else gs *= gainLin[b][0]; }
        }
        if (msx) { const float m = 0.5f * (l + r) * gm, s = 0.5f * (l - r) * gs; l = m + s; r = m - s; }
    }
    inline void minimumOs(float* x) {
        const int E = numEdges;
        float rest[2] = { x[0], x[1] }, acc[2] = { 0, 0 };
        for (int e = 0; e < E; ++e) {
            float low[2];
            for (int c = 0; c < 2; ++c) {
                float l, h;
                if (edgeN[e] == 0) { const float v = (rest[c] - spZ[c][e]) * xc[e].g1, lp = v + spZ[c][e]; spZ[c][e] = lp + v; l = lp; h = rest[c] - lp; }
                else { l = bwLow(xc[e], spL[c][e][1], bwLow(xc[e], spL[c][e][0], rest[c])); h = bwHigh(xc[e], spH[c][e][1], bwHigh(xc[e], spH[c][e][0], rest[c])); if (edgeN[e] & 1) h = -h; }
                if (e > 0 && edgeN[e]) acc[c] = bwAllpass(xc[e], apS[c][e][0], acc[c]);
                low[c] = l; rest[c] = h;
            }
            segGains(e, low[0], low[1]);
            acc[0] += low[0]; acc[1] += low[1];
        }
        segGains(E, rest[0], rest[1]);
        x[0] = acc[0] + rest[0]; x[1] = acc[1] + rest[1];
    }

    // ---- linear phase: zero-phase FIR band filters by overlap-save FFT convolution ----------
    // out = input (delayed) + sum over bands of (gain - 1) * band-filtered input
    inline void linearSample(const float* u, float* out) {
        ring[ringPos] = cpx(u[0], u[1]); ringPos = (ringPos + 1) % linN;
        const int lat = linB + linL / 2;
        const int idx = (hp - laSamples - lat) & histMask;
        float y[2] = { hist[0][idx], hist[1][idx] };
        for (int b = 0; b < kMaxBands; ++b) {
            if (!used[b] || !kernOk[b]) continue;
            const cpx v = fifo[b][linPos];
            if (stereo[b] == kStereo) { y[0] += (gainLin[b][0] - 1.f) * v.real(); y[1] += (gainLin[b][1] - 1.f) * v.imag(); }
            else { const float d = (gainLin[b][0] - 1.f) * (stereo[b] == kMid ? v.real() : v.imag()); y[0] += d; y[1] += stereo[b] == kMid ? d : -d; }
        }
        out[0] = y[0]; out[1] = y[1];
        if (++linPos == linB) { linPos = 0; linearBlock(); }
    }
    void buildKernel(int b, std::vector<cpx>& K) {
        const int L = linL;
        for (int k = 0; k <= L / 2; ++k) { const float f = std::max(k * fs / L, 1.f); design[k] = cpx(bandMag(f, lo[b], hi[b], order[b]), 0); }
        for (int k = 1; k < L / 2; ++k) design[L - k] = design[k];
        fftL.run(design.data(), true);                                      // zero-phase impulse response (real, circular)
        std::fill(K.begin(), K.end(), cpx(0, 0));
        for (int m = -(L / 2 - 1); m <= L / 2 - 1; ++m) {                    // centre it at L/2, window, zero-pad to the FFT size
            const float w = 0.5f + 0.5f * std::cos(kPi * m / (L / 2));
            K[L / 2 + m] = cpx(design[(m + L) % L].real() * w, 0);
        }
        fftN.run(K.data(), false);
        kLo[b] = lo[b]; kHi[b] = hi[b]; kT[b] = order[b];
    }
    void linearBlock() {
        const int N = linN, B = linB;
        for (int k = 0; k < N; ++k) work[k] = ring[(ringPos + k) % N];      // oldest first
        fftN.run(work.data(), false);
        bool needMs = false; for (int b = 0; b < kMaxBands; ++b) needMs |= used[b] && stereo[b] != kStereo;
        if (needMs) for (int k = 0; k < N; ++k) {                              // mid + j*side spectrum from the packed left + j*right one
            const cpx z = work[k], zc = std::conj(work[(N - k) % N]);
            const cpx xl = 0.5f * (z + zc), xr = cpx(0, -0.5f) * (z - zc);
            ms[k] = 0.5f * (xl + xr) + cpx(0, 1) * (0.5f * (xl - xr));
        }
        for (int b = 0; b < kMaxBands; ++b) {
            if (!used[b]) { kernOk[b] = false; continue; }
            const bool changed = !kernOk[b] || lo[b] != kLo[b] || hi[b] != kHi[b] || order[b] != kT[b];
            const bool fadeIn = !kernOk[b];
            if (changed) { std::swap(kern[b], kernOld[b]); buildKernel(b, kern[b]); }
            const std::vector<cpx>& X = stereo[b] == kStereo ? work : ms;
            for (int k = 0; k < N; ++k) work2[k] = X[k] * kern[b][k];
            fftN.run(work2.data(), true);
            if (!changed || fadeIn) { for (int j = 0; j < B; ++j) fifo[b][j] = work2[N - B + j]; }
            else {
                for (int j = 0; j < B; ++j) fifo[b][j] = work2[N - B + j];
                for (int k = 0; k < N; ++k) work2[k] = X[k] * kernOld[b][k];
                fftN.run(work2.data(), true);
                for (int j = 0; j < B; ++j) { const float t = (j + 0.5f) / B; fifo[b][j] = fifo[b][j] * t + work2[N - B + j] * (1.f - t); }
            }
            kernOk[b] = true;
        }
    }

    // ---- solo: band-pass the output to the soloed bands ---------------------------------------
    inline void soloFilter(const float* x, float* y) {
        y[0] = y[1] = 0;
        for (int b = 0; b < kMaxBands; ++b) {
            if (!used[b] || p[bp(b, kBSolo)] < 0.5f) continue;
            float in[2] = { x[0], x[1] }; int nch = 2;
            if (stereo[b] == kMid) { in[0] = 0.5f * (x[0] + x[1]); nch = 1; } else if (stereo[b] == kSide) { in[0] = 0.5f * (x[0] - x[1]); nch = 1; }
            for (int c = 0; c < nch; ++c) {
                float s = in[c];
                if (!openLo(lo[b])) { s = bwHigh(soloHp[b], soloS[b][c][0], s); s = bwHigh(soloHp[b], soloS[b][c][1], s); }
                if (!openHi(hi[b])) { s = bwLow(soloLp[b], soloS[b][c][2], s); s = bwLow(soloLp[b], soloS[b][c][3], s); }
                in[c] = s;
            }
            if (nch == 2) { y[0] += in[0]; y[1] += in[1]; }
            else if (stereo[b] == kMid) { y[0] += in[0]; y[1] += in[0]; }
            else { y[0] += in[0]; y[1] -= in[0]; }
        }
    }

    // state
    float fs = 48000.f, fsOs = 48000.f, smooth = 0.01f, meterDecay = 0.9999f, slowC = 0.0001f;
    int mode = 0, os = 1, laSamples = 0, ctl = 0, coefCount = 0, fadeState = 0; float fadeGain = 1;
    int desMode = 0, desOs = 1, desLa = 0;
    bool used[kMaxBands] = {}, desUsed[kMaxBands] = {}, bandOn[kMaxBands] = {}, anySolo = false;
    int stereo[kMaxBands] = {}, desStereo[kMaxBands] = {}, order[kMaxBands] = { 4, 4, 4, 4, 4, 4 };
    float lo[kMaxBands] = {}, hi[kMaxBands] = {}, dLo[kMaxBands] = {}, dHi[kMaxBands] = {};
    // history (main L/R, sidechain L/R)
    std::vector<float> hist[4]; int histN = 1, histMask = 0, hp = 0;
    // detection
    DetState det[kMaxBands][2]; BwCoef detHp[kMaxBands], detLp[kMaxBands];
    float rmsC[kMaxBands] = {}, attC[kMaxBands] = {}, relC[kMaxBands][2] = {}, relMs[kMaxBands] = {};
    float lvlDb[kMaxBands][2] = {}, gc[kMaxBands][2] = {}, gcSlow[kMaxBands][2] = {}, totDb[kMaxBands][2] = {}, gainLin[kMaxBands][2] = {}, curDb[kMaxBands] = {};
    float sGain[kMaxBands] = {}, tGain[kMaxBands] = {}, sByp[kMaxBands] = {}, tByp[kMaxBands] = {};
    int laOff[kMaxBands] = {};
    // dynamic phase
    ShelfCoef sh[kMaxBands][2][2]; ShelfState shS[kMaxBands][2][2];
    // minimum phase
    EdgeCfg edges[kMaxEdges] = {}, desEdge[kMaxEdges] = {}; int numEdges = 0, desEdges = 0, edgeN[kMaxEdges] = {}, segMask[kMaxEdges + 1] = {};
    BwCoef xc[kMaxEdges]; BwState spL[2][kMaxEdges][2], spH[2][kMaxEdges][2], apS[2][kMaxEdges][1]; float spZ[2][kMaxEdges] = {};
    // linear phase
    int linL = 4096, linB = 2048, linN = 8192, ringPos = 0, linPos = 0;
    Fft fftN, fftL;
    std::vector<cpx> ring, work, work2, ms, design, kern[kMaxBands], kernOld[kMaxBands], fifo[kMaxBands];
    bool kernOk[kMaxBands] = {}; float kLo[kMaxBands] = {}, kHi[kMaxBands] = {}; int kT[kMaxBands] = {};
    // solo
    BwCoef soloHp[kMaxBands], soloLp[kMaxBands]; BwState soloS[kMaxBands][2][4];
    // oversampling and output
    Oversampler ovs[2];
    float sMix = 1, tMix = 1, sOut = 1, tOut = 1, sPanL = 1, tPanL = 1, sPanR = 1, tPanR = 1, sSolo = 0;
};

} // namespace schist
