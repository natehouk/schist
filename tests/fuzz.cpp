// Random parameters, band layouts and modes at 44.1-192 kHz with loud noise and a sidechain:
// output always finite and bounded.
#include "../plugins/Schist/ParamDefs.hpp"
#include <cstdio>
#include <memory>
#include <random>
using namespace schist;
int main() {
    std::mt19937 g(31); std::uniform_real_distribution<float> u(0, 1); int bad = 0; float pk = 0;
    for (int sr : { 44100, 48000, 96000, 192000 }) {
        std::unique_ptr<Engine> e(new Engine()); e->setSampleRate((float)sr);
        for (int it = 0; it < 300; ++it) {
            for (uint32_t i = 0; i < kOutLearn; ++i) {
                if (u(g) < 0.85f) continue;
                const ParamDef& d = paramDef(i);
                float v = (u(g) < .3f) ? (u(g) < .5f ? d.min : d.max) : (d.hints & kHintLog) ? d.min * std::pow(d.max / d.min, u(g)) : d.min + (d.max - d.min) * u(g);
                if (d.hints & kHintInteger) v = std::round(v);
                e->p[i] = v;
            }
            float in[512], sc[512], L[512], R[512]; const float amp = u(g) < .2f ? 0 : (u(g) < .5f ? 4.f : 0.5f);
            for (int k = 0; k < 512; ++k) { in[k] = amp * (2 * u(g) - 1); sc[k] = 2 * u(g) - 1; }
            e->process(in, in, sc, sc, L, R, 512);
            for (int k = 0; k < 512; ++k) { if (!std::isfinite(L[k]) || !std::isfinite(R[k])) bad++; pk = std::max(pk, std::max(std::fabs(L[k]), std::fabs(R[k]))); }
        }
    }
    printf("fuzz nonfinite=%d peak=%.2f\n", bad, pk); return bad || pk > 64.f;
}
