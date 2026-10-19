// CPU: 1, 3 and 6 bands in each processing mode, with and without oversampling.
#include "../plugins/Schist/ParamDefs.hpp"
#include <chrono>
#include <cstdio>
#include <memory>
using namespace schist;
int main() {
    const int SR = 48000; static float in[SR], L[SR], R[SR]; uint32_t r = 1; for (float& x : in) { r = r * 1664525u + 1013904223u; x = 0.3f * ((int32_t)r * (1.f / 2147483648.f)); }
    static const char* const mn[3] = { "dynamic", "linear ", "minimum" };
    for (int m = 0; m < 3; ++m) for (int os = 0; os < 3; ++os) {
        if (m == 1 && os > 0) continue;                     // linear phase runs at the base rate
        for (int nb : { 1, 3, 6 }) {
            std::unique_ptr<Engine> e(new Engine()); e->p[kProcMode] = (float)m; e->p[kOversampling] = (float)os;
            for (int b = 0; b < nb; ++b) { const float lo = 30.f * std::pow(600.f, b / (float)nb), hi = 30.f * std::pow(600.f, (b + 1) / (float)nb) * 0.9f;
                e->setBand(b, lo, hi); e->p[bp(b, kBThreshold)] = -30; e->p[bp(b, kBRatio)] = 4; e->p[bp(b, kBRange)] = -12; e->p[bp(b, kBSlope)] = 3 + b % 5; }
            e->setSampleRate(SR);
            const int secs = 4; auto t0 = std::chrono::steady_clock::now();
            for (int k = 0; k < secs; ++k) for (int b = 0; b < SR; b += 256) e->process(in + b, in + b, nullptr, nullptr, L + b, R + b, 256);
            const double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            printf("%s %s %d band%s: %.1f%% of one core\n", mn[m], os == 0 ? "1x" : (os == 1 ? "2x" : "4x"), nb, nb == 1 ? " " : "s", 100 * el / secs);
        }
    }
    return 0;
}
