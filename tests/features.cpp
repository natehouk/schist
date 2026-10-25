// Feature tests for Schist: transparency in all three processing modes, compression and expansion in both
// directions, range, knee, ratio, band isolation, attack and release, look-ahead, latency, mid/side bands,
// external sidechain, custom detection range, stereo link, solo, bypass, mix, band gain, open (shelf) bands,
// and the display response against measurement.
#include "../plugins/Schist/ParamDefs.hpp"
#include "../plugins/Schist/Presets.hpp"
#include <cstdio>
#include <functional>
#include <memory>
#include <vector>
using namespace schist;

static int fails = 0;
static void check(bool ok, const char* fmt, double a = 0, double b = 0, double c = 0) { printf("%s  ", ok ? "PASS" : "FAIL"); printf(fmt, a, b, c); printf("\n"); fails += !ok; }
static const float fs = 48000;
typedef std::unique_ptr<Engine> EP;
typedef std::vector<float> V;
static EP make(int mode = 0, int os = 0) { EP e(new Engine()); e->p[kProcMode] = (float)mode; e->p[kOversampling] = (float)os; return e; }
static void band(Engine& e, int b, float lo, float hi, float thr, float ratio, float range, float knee = 0, int mode = kCompress) {
    e.setBand(b, lo, hi); e.p[bp(b, kBThreshold)] = thr; e.p[bp(b, kBRatio)] = ratio; e.p[bp(b, kBRange)] = range; e.p[bp(b, kBKnee)] = knee; e.p[bp(b, kBMode)] = (float)mode;
}
static void run(Engine& e, const V& l, const V& r, V& L, V& R, const V* scl = nullptr, const V* scr = nullptr) {
    L.assign(l.size(), 0); R.assign(l.size(), 0);
    for (size_t i = 0; i < l.size(); i += 256) { const uint32_t n = (uint32_t)std::min<size_t>(256, l.size() - i);
        e.process(l.data() + i, r.data() + i, scl ? scl->data() + i : nullptr, scr ? scr->data() + i : nullptr, L.data() + i, R.data() + i, n); }
}
static V sine(float hz, float db, int n) { V v(n); const float a = dbToGain(db); for (int i = 0; i < n; ++i) v[i] = a * std::sin(2 * kPi * hz * i / fs); return v; }
static V add(const V& a, const V& b) { V r(a.size()); for (size_t i = 0; i < a.size(); ++i) r[i] = a[i] + b[i]; return r; }
static double peakDb(const V& v, size_t a, size_t b) { float m = 0; for (size_t i = a; i < b; ++i) m = std::max(m, std::fabs(v[i])); return gainToDb(m); }
static double toneDb(const V& v, float hz, size_t a, size_t b) {   // level of one frequency, as a sine peak in dB
    double c = 0, s = 0; for (size_t i = a; i < b; ++i) { c += v[i] * std::cos(2 * kPi * hz * i / fs); s += v[i] * std::sin(2 * kPi * hz * i / fs); }
    return 20 * std::log10(2 * std::sqrt(c * c + s * s) / (double)(b - a) + 1e-30);
}
static const char* modeName(int m) { return m == 0 ? "dynamic" : (m == 1 ? "linear" : "minimum"); }
// gain applied to a steady tone (dB), measured over the second half
static double gainAt(Engine& e, float hz, float db, int n = 96000) { V in = sine(hz, db, n), L, R; e.setSampleRate(fs); run(e, in, in, L, R); return toneDb(L, hz, n / 2, n) - db; }

int main() {
    const int N = 96000;
    // 1. transparency: bands present but doing nothing; every mode and oversampling setting
    { double worst = 0; int wm = 0;
      for (int m = 0; m < 3; ++m) for (int os = 0; os < 3; ++os) for (float hz : { 40.f, 150.f, 900.f, 3000.f, 9000.f, 16000.f }) {
          EP e = make(m, os); band(*e, 0, 60, 300, -10, 4, -24); band(*e, 1, 300, 2500, -10, 4, -24); band(*e, 2, 4000, 12000, -10, 4, -24); band(*e, 3, 10, 50, -10, 4, -24);
          e->p[bp(1, kBSlope)] = 7; e->p[bp(2, kBSlope)] = 0;
          const double g = gainAt(*e, hz, -30); if (std::fabs(g) > worst) { worst = std::fabs(g); wm = m; } }
      check(worst < 0.05, "4 idle bands, all modes and oversampling: flat within %.3f dB (worst mode %.0f)", worst, wm); }
    // 2. dynamic phase is an exact wire when idle (no oversampling)
    { EP e = make(0, 0); band(*e, 0, 200, 2000, -10, 4, -24); e->setSampleRate(fs);
      V in(N); uint32_t r = 7; for (float& x : in) { r = r * 1664525u + 1013904223u; x = 0.05f * ((int32_t)r * (1.f / 2147483648.f)); }
      V L, R; run(*e, in, in, L, R); double err = 0; for (int i = 0; i < N; ++i) err = std::max(err, (double)std::fabs(L[i] - in[i]));
      check(err < 1e-6, "dynamic phase, idle band: output equals input sample for sample (max error %.1e)", err); }
    // 3. latency per mode
    { EP a = make(0, 0), b = make(0, 1), c = make(0, 2), d = make(1, 0), f = make(2, 2), g = make(0, 0);
      band(*g, 0, 100, 1000, -20, 4, -12); g->p[bp(0, kBLookahead)] = 5;
      for (Engine* x : { a.get(), b.get(), c.get(), d.get(), f.get(), g.get() }) x->setSampleRate(fs);
      check(a->latency() == 0 && b->latency() == 32 && c->latency() == 48, "latency, dynamic phase: off %.0f, 2x %.0f, 4x %.0f samples", a->latency(), b->latency(), c->latency());
      check(d->latency() == 4096 && f->latency() == 48 && g->latency() == 960, "latency: linear %.0f, minimum 4x %.0f, any look-ahead adds 20 ms (%.0f)", d->latency(), f->latency(), g->latency());
      // the reported latency is the real one: a click comes out exactly that late (linear, with look-ahead)
      EP h = make(1, 0); band(*h, 0, 300, 3000, -60, 2, -6); h->p[bp(0, kBLookahead)] = 3; h->setSampleRate(fs);
      V in(16384, 0.f), L, R; in[1000] = 0.5f; run(*h, in, in, L, R); int pk = 0; for (int i = 0; i < 16384; ++i) if (std::fabs(L[i]) > std::fabs(L[pk])) pk = i;
      check(pk - 1000 == (int)h->latency(), "linear phase + look-ahead: impulse arrives %.0f samples late, reported %.0f", pk - 1000, h->latency()); }
    // 4. downward compression: -10 dBFS tone, threshold -30, ratio 4, hard knee -> 20 dB over -> -15 dB
    for (int m = 0; m < 3; ++m) { EP e = make(m); band(*e, 0, 150, 6000, -30, 4, -48); const double g = gainAt(*e, 1000, -10);
      check(std::fabs(g + 15) < 0.6, "downward compression 4:1, 20 dB over: %.2f dB (expect -15)", g); printf("        (%s)\n", modeName(m)); }
    // 5. range limits it
    { EP e = make(); band(*e, 0, 300, 3000, -30, 4, -6); const double g = gainAt(*e, 1000, -10);
      check(std::fabs(g + 6) < 0.3, "range -6 dB caps the gain reduction: %.2f dB", g); }
    // 6. upward compression: quiet tone at -50, threshold -20, ratio 4, range +12 -> +12 (capped)
    { EP e = make(); band(*e, 0, 300, 3000, -20, 4, 12); const double g = gainAt(*e, 1000, -50);
      EP f = make(); band(*f, 0, 300, 3000, -20, 2, 24); const double g2 = gainAt(*f, 1000, -30);
      check(std::fabs(g - 12) < 0.4 && std::fabs(g2 - 5) < 0.4, "upward compression: +%.2f dB (range +12), +%.2f dB (2:1, 10 dB under: expect +5)", g, g2); }
    // 7. downward expansion (gate) and upward expansion
    { EP e = make(); band(*e, 0, 300, 3000, -30, 4, -30, 0, kExpand); const double g = gainAt(*e, 1000, -40);
      EP f = make(); band(*f, 0, 300, 3000, -30, 2, 6, 0, kExpand); const double g2 = gainAt(*f, 1000, -27);
      EP h = make(); band(*h, 0, 300, 3000, -30, 2, 12, 0, kExpand); const double g3 = gainAt(*h, 1000, -10);
      check(std::fabs(g + 30) < 0.5, "downward expansion 4:1, 10 dB under the threshold: %.2f dB (expect -30, capped by range)", g);
      check(std::fabs(g2 - 3) < 0.4 && std::fabs(g3 - 12) < 0.4, "upward expansion 2:1: +%.2f dB at 3 dB over, +%.2f dB at 20 dB over (range +12)", g2, g3); }
    // 8. soft knee: at the threshold, a 12 dB knee gives (6^2/24)*(1-1/4) = 1.125 dB
    { EP e = make(); band(*e, 0, 300, 3000, -20, 4, -48, 12); const double g = gainAt(*e, 1000, -20);
      check(std::fabs(g + 1.125) < 0.25, "12 dB soft knee, at the threshold: %.2f dB (expect -1.13)", g); }
    // 9. band isolation: a tone outside the band is untouched while the band compresses hard
    for (int m = 0; m < 3; ++m) { EP e = make(m); band(*e, 0, 1000, 5000, -40, 8, -24);
      V a = sine(2500, -10, N), b = sine(100, -10, N), in = add(a, b), L, R; e->setSampleRate(fs); run(*e, in, in, L, R);
      const double in100 = toneDb(L, 100, N / 2, N) + 10, in2k5 = toneDb(L, 2500, N / 2, N) + 10;
      check(std::fabs(in100) < 0.3 && in2k5 < -15, "band 1-5 kHz compressing %.1f dB; 100 Hz tone changes %.2f dB", in2k5, in100); printf("        (%s)\n", modeName(m)); }
    // 10. attack and release
    { EP e = make(); band(*e, 0, 300, 3000, -40, 100, -20); e->p[bp(0, kBAttack)] = 10; e->p[bp(0, kBRelease)] = 200; e->setSampleRate(fs);
      V in(N); for (int i = 0; i < N; ++i) in[i] = (i >= 24000 && i < 48000 ? 0.3f : 0.003f) * std::sin(2 * kPi * 1000 * i / fs);
      V L, R; run(*e, in, in, L, R);
      auto grAt = [&](int i) { return peakDb(L, i - 240, i) - peakDb(in, i - 240, i); };
      const double a5 = grAt(24000 + 480), a40 = grAt(24000 + 1920), r100 = grAt(48000 + 4800), r600 = grAt(48000 + 28800);
      check(a5 > -18 && a40 < -17, "attack 10 ms: %.1f dB after 10 ms, %.1f dB after 40 ms", a5, a40);
      check(r100 < -3 && r600 > -1.5, "release 200 ms: %.1f dB after 100 ms, %.1f dB after 600 ms", r100, r600); }
    // 11. look-ahead catches a sudden onset
    { auto over = [&](float la) { EP e = make(); band(*e, 0, 300, 3000, -30, 100, -30); e->p[bp(0, kBAttack)] = 1; e->p[bp(0, kBLookahead)] = la; e->setSampleRate(fs);
          V in(N); for (int i = 0; i < N; ++i) in[i] = (i >= 30000 ? 0.5f : 0.f) * std::sin(2 * kPi * 1000 * i / fs);
          V L, R; run(*e, in, in, L, R); const int lat = (int)e->latency(); return peakDb(L, 30000 + lat, 30000 + lat + 480); };
      const double no = over(0), yes = over(5);
      check(yes < no - 5, "look-ahead: first 10 ms peak %.1f dB with 5 ms look-ahead, %.1f dB without", yes, no); }
    // 12. mid and side bands
    { EP e = make(); band(*e, 0, 300, 3000, -40, 10, -20); e->p[bp(0, kBStereo)] = kMid; e->setSampleRate(fs);
      V s = sine(1000, -10, N), ns(N), L, R; for (int i = 0; i < N; ++i) ns[i] = -s[i];
      run(*e, s, s, L, R); const double mid = toneDb(L, 1000, N / 2, N) + 10;
      e->setSampleRate(fs); run(*e, s, ns, L, R); const double side = toneDb(L, 1000, N / 2, N) + 10;
      check(mid < -15 && std::fabs(side) < 0.2, "mid-only band: mid signal %.1f dB, side signal %.2f dB", mid, side);
      for (int m = 1; m < 3; ++m) { EP f = make(m); band(*f, 0, 300, 3000, -40, 10, -20); f->p[bp(0, kBStereo)] = kSide; f->setSampleRate(fs);
          run(*f, s, s, L, R); const double md = toneDb(L, 1000, N / 2, N) + 10; f->setSampleRate(fs); run(*f, s, ns, L, R); const double sd = toneDb(L, 1000, N / 2, N) + 10;
          check(std::fabs(md) < 0.2 && sd < -15, "side-only band: mid %.2f dB, side %.1f dB", md, sd); printf("        (%s)\n", modeName(m)); } }
    // 13. external sidechain
    { EP e = make(); band(*e, 0, 300, 3000, -30, 10, -20); e->p[bp(0, kBSidechain)] = 1; e->setSampleRate(fs);
      V s = sine(1000, -40, N), k = sine(1000, -6, N), z(N, 0.f), L, R;
      run(*e, s, s, L, R, &z, &z); const double quiet = toneDb(L, 1000, N / 2, N) + 40;
      e->setSampleRate(fs); run(*e, s, s, L, R, &k, &k); const double ducked = toneDb(L, 1000, N / 2, N) + 40;
      check(std::fabs(quiet) < 0.2 && ducked < -15, "external sidechain: silent key %.2f dB, loud key ducks the band %.1f dB", quiet, ducked); }
    // 14. custom detection range: a 100 Hz kick drives compression of the 2-8 kHz band
    { EP e = make(); band(*e, 0, 2000, 8000, -30, 10, -20); e->p[bp(0, kBDetCustom)] = 1; e->p[bp(0, kBDetLo)] = 50; e->p[bp(0, kBDetHi)] = 200; e->setSampleRate(fs);
      V hat = sine(4000, -30, N), kick = sine(100, -6, N), both = add(hat, kick), L, R;
      run(*e, hat, hat, L, R); const double alone = toneDb(L, 4000, N / 2, N) + 30;
      e->setSampleRate(fs); run(*e, both, both, L, R); const double withKick = toneDb(L, 4000, N / 2, N) + 30;
      check(std::fabs(alone) < 0.3 && withKick < -15, "custom detection 50-200 Hz: 4 kHz alone %.2f dB, with a loud 100 Hz %.1f dB", alone, withKick); }
    // 15. stereo link
    { auto rgr = [&](float link) { EP e = make(); band(*e, 0, 300, 3000, -40, 10, -20); e->p[bp(0, kBLink)] = link; e->setSampleRate(fs);
          V l = sine(1000, -6, N), r = sine(1000, -50, N), L, R; run(*e, l, r, L, R); return toneDb(R, 1000, N / 2, N) + 50; };
      const double unl = rgr(0), lnk = rgr(1);
      check(std::fabs(unl) < 0.2 && lnk < -15, "stereo link: quiet right channel %.2f dB unlinked, %.1f dB linked", unl, lnk); }
    // 16. solo, bypass, mix, band gain
    { EP e = make(); band(*e, 0, 1000, 5000, -60, 2, -6); e->p[bp(0, kBSolo)] = 1; e->setSampleRate(fs);
      V in = add(sine(100, -10, N), sine(2500, -10, N)), L, R; run(*e, in, in, L, R);
      const double lowS = toneDb(L, 100, N / 2, N) + 10, inS = toneDb(L, 2500, N / 2, N) + 10;
      check(lowS < -40 && inS > -8, "solo band 1-5 kHz: 100 Hz %.0f dB, 2.5 kHz %.1f dB (compressed)", lowS, inS);
      EP f = make(); band(*f, 0, 300, 3000, -40, 10, -20); f->p[bp(0, kBBypass)] = 1; const double byp = gainAt(*f, 1000, -10);
      EP g = make(); band(*g, 0, 300, 3000, -30, 4, -48); g->p[kMix] = 0.5f; const double half = gainAt(*g, 1000, -10);
      EP h = make(); band(*h, 0, 300, 3000, -30, 4, -48); h->p[kMix] = 2; const double dbl = gainAt(*h, 1000, -10);
      check(std::fabs(byp) < 0.05, "bypassed band: %.3f dB", byp);
      const double w = dbToGain(-15);
      check(std::fabs(half - gainToDb((float)(0.5 + 0.5 * w))) < 0.3 && std::fabs(dbl - gainToDb((float)std::fabs(1 - 2 * (1 - w)))) < 0.3, "mix 50%%: %.2f dB (parallel); mix 200%%: %.2f dB (twice the change)", half, dbl);
      for (int m = 0; m < 3; ++m) { EP k = make(m); band(*k, 0, 300, 3000, 0, 1, 0); k->p[bp(0, kBGain)] = 6; const double gg = gainAt(*k, 950, -20);
          check(std::fabs(gg - 6) < 0.25, "band gain +6 dB, centre of a 300-3000 Hz band: %+.2f dB", gg); printf("        (%s)\n", modeName(m)); } }
    // 17. open bands work as shelves
    { EP e = make(); band(*e, 0, 10, 300, -40, 10, -12); const double low = gainAt(*e, 60, -10);
      EP f = make(); band(*f, 0, 3000, 30000, -40, 10, -12); const double high = gainAt(*f, 12000, -10);
      check(low < -11 && high < -11, "open bands: low shelf band at 60 Hz %.1f dB, high shelf band at 12 kHz %.1f dB", low, high); }
    // 18. the display response matches what the band does (static gain, all modes, several frequencies)
    { double worst = 0;
      for (int m = 0; m < 3; ++m) for (float hz : { 200.f, 600.f, 1500.f, 5000.f }) {
          EP e = make(m); band(*e, 0, 500, 2000, 0, 1, 0); e->p[bp(0, kBGain)] = -9; e->p[bp(0, kBSlope)] = 1; const double g = gainAt(*e, hz, -20);
          worst = std::max(worst, std::fabs(g - e->responseDb(hz))); }
      check(worst < 1.0, "display response vs measured, -9 dB band, 12 dB/oct, all modes: within %.2f dB", worst); }
    // 19. minimum phase: six contiguous bands at mixed slopes sum flat when idle
    { double worst = 0;
      for (int sl : { 0, 1, 3, 7 }) for (float hz : { 30.f, 120.f, 500.f, 2000.f, 7000.f, 15000.f }) {
          EP e = make(2); const float x[7] = { 10, 80, 250, 800, 2500, 8000, 30000 };
          for (int b = 0; b < 6; ++b) { band(*e, b, x[b], x[b + 1], -10, 4, -24); e->p[bp(b, kBSlope)] = (float)sl; }
          worst = std::max(worst, std::fabs(gainAt(*e, hz, -30))); }
      check(worst < 0.05, "minimum phase, six contiguous idle bands, slopes 6-48 dB/oct: flat within %.3f dB", worst); }
    // 20. limiting: compress at Inf:1 with look-ahead holds the band at the threshold (smooth 2 ms onset, so the energy is in the band)
    for (int m = 0; m < 3; ++m) {
        EP e = make(m); band(*e, 0, 300, 3000, -20, 100, -48); e->p[bp(0, kBAttack)] = 0.01f; e->p[bp(0, kBLookahead)] = 5; e->p[bp(0, kBRelease)] = 100; e->setSampleRate(fs);
        V in(N); for (int i = 0; i < N; ++i) in[i] = (i >= 48000 ? dbToGain(-6) * std::min(1.f, (i - 48000) / 96.f) : 0.f) * std::sin(2 * kPi * 1000 * i / fs);
        V L, R; run(*e, in, in, L, R); const int lat = (int)e->latency();
        const double onset = peakDb(L, 48000 + lat, 48000 + lat + 960), steady = toneDb(L, 1000, 72000, N);
        check(onset < -18.5 && std::fabs(steady + 20) < 1.0, "limiting, Inf:1, 5 ms look-ahead, 14 dB over: first 20 ms peak %.1f dB, steady %.2f dB (threshold -20)", onset, steady);
        printf("        (%s)\n", modeName(m)); }
    { EP a = make(); band(*a, 0, 300, 3000, -20, 100, -48); const double inf = gainAt(*a, 1000, 0);
      EP b = make(); band(*b, 0, 300, 3000, -20, 50, -48); const double fifty = gainAt(*b, 1000, 0);
      check(std::fabs(inf + 20) < 0.3 && std::fabs(fifty + 19.6) < 0.3, "Inf:1 holds a tone 20 dB over the threshold to it (%.2f dB); 50:1 lets 0.4 dB through (%.2f dB)", inf, fifty); }
    // 21. gating: expand at Inf:1 turns a band under the threshold down by the full range and leaves it alone above
    for (int m = 0; m < 3; ++m) {
        auto gate = [&](float db) { EP e = make(m); band(*e, 0, 300, 3000, -30, 100, -48, 0, kExpand); e->p[bp(0, kBSlope)] = 7; e->p[bp(0, kBAttack)] = 0.01f; return gainAt(*e, 1000, db); };
        const double under = gate(-40), over = gate(-25);
        check(under < -45 && std::fabs(over) < 0.1, "gating, Inf:1, range -48 dB, 48 dB/oct band: 10 dB under %.1f dB, 5 dB over %.2f dB", under, over);
        printf("        (%s)\n", modeName(m)); }
    // 22. presets load and stay finite
    { bool ok = true; for (uint32_t i = 0; i < kNumPresets; ++i) { EP e = make(); std::copy(presets()[i].v, presets()[i].v + kParamCount, e->p); const double g = gainAt(*e, 1000, -12, 24000); ok &= std::isfinite(g); }
      check(ok, "all %.0f presets produce finite output", kNumPresets); }
    printf("\n%s (%d failure%s)\n", fails ? "FAILED" : "ALL PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
