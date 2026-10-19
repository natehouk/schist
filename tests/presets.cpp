// Play a drum-and-bass test mix through every factory preset; report level change and peaks.
#include "../plugins/Schist/ParamDefs.hpp"
#include "../plugins/Schist/Presets.hpp"
#include <cstdio>
#include <memory>
#include <vector>
using namespace schist;
int main() {
    const int SR = 48000, N = SR * 3; int bad = 0;
    std::vector<float> x(N), sc(N); uint32_t r = 3; float lp = 0;
    for (int i = 0; i < N; ++i) { r = r * 1664525u + 1013904223u; const float w = (int32_t)r * (1.f / 2147483648.f); lp += 0.2f * (w - lp);
        const float t = std::fmod(i / (float)SR, 0.5f), hit = std::exp(-t * 25.f);
        sc[i] = 0.5f * hit * std::sin(2 * kPi * 60 * t);
        x[i] = 0.25f * hit * std::sin(2 * kPi * (50 + 80 * std::exp(-t * 40)) * t) + 0.12f * lp * (std::fmod(i / (float)SR, 0.25f) < 0.06f) + 0.12f * std::sin(2 * kPi * 55 * i / (float)SR) + 0.05f * std::sin(2 * kPi * 440 * i / (float)SR); }
    double si = 0; for (float v : x) si += v * v; const double inR = std::sqrt(si / N);
    for (uint32_t p = 0; p < kNumPresets; ++p) {
        std::unique_ptr<Engine> e(new Engine()); for (uint32_t k = 0; k < kOutLearn; ++k) e->p[k] = presets()[p].v[k]; e->setSampleRate(SR);
        std::vector<float> L(N), R(N); for (int i = 0; i < N; i += 512) e->process(x.data() + i, x.data() + i, sc.data() + i, sc.data() + i, L.data() + i, R.data() + i, std::min(512, N - i));
        double s = 0; float pk = 0; bool nf = false;
        for (int i = SR / 2; i < N; ++i) { s += L[i] * L[i] + R[i] * R[i]; pk = std::max(pk, std::max(std::fabs(L[i]), std::fabs(R[i]))); nf |= !std::isfinite(L[i]); }
        const double rel = 20 * std::log10(std::sqrt(s / (2.0 * (N - SR / 2))) / inR + 1e-9);
        const bool ok = !nf && rel > -9 && rel < 6 && pk < 1.5f; bad += !ok;
        printf("%-22s %-12s level %+5.1f dB  peak %.2f  latency %4u %s\n", presets()[p].name, presets()[p].category, rel, pk, e->latency(), ok ? "" : "<-- CHECK");
    }
    printf("%u presets, %d flagged\n", kNumPresets, bad); return bad;
}
