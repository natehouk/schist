// Factory presets. Each preset is a small function that sets up bands on top of the engine defaults. License: MIT.
// Engine defaults: no bands (the plugin passes audio through untouched), dynamic phase, mix 100%.
#pragma once
#include "SchistEngine.hpp"
#include <algorithm>

namespace schist {

struct Preset { const char* name; const char* category; float v[kParamCount]; };
static const char* const kCategories[] = { "Mastering", "Mix Bus", "Drums", "Vocals", "Bass & Keys", "Repair", "Creative" };
static constexpr int kNumCategories = 7;

namespace pre {
typedef Engine E;
// a band: frequencies, threshold, ratio, range (negative cuts, positive boosts), attack/release (ms)
inline void B(E& e, int b, float lo, float hi, float thr, float ratio, float range, float att = 10, float rel = 120, float knee = 10) {
    e.setBand(b, lo, hi); e.p[bp(b, kBThreshold)] = thr; e.p[bp(b, kBRatio)] = ratio; e.p[bp(b, kBRange)] = range;
    e.p[bp(b, kBAttack)] = att; e.p[bp(b, kBRelease)] = rel; e.p[bp(b, kBKnee)] = knee;
}
inline void expand(E& e, int b) { e.p[bp(b, kBMode)] = kExpand; }
inline void slope(E& e, int b, int dbPerOct) { e.p[bp(b, kBSlope)] = (float)(dbPerOct / 6 - 1); }
inline void gain(E& e, int b, float db) { e.p[bp(b, kBGain)] = db; }
inline void ms(E& e, int b, int which) { e.p[bp(b, kBStereo)] = (float)which; }
inline void detect(E& e, int b, float lo, float hi) { e.p[bp(b, kBDetCustom)] = 1; e.p[bp(b, kBDetLo)] = lo; e.p[bp(b, kBDetHi)] = hi; }
inline void la(E& e, int b, float ms_) { e.p[bp(b, kBLookahead)] = ms_; }
inline void mode(E& e, int m) { e.p[kProcMode] = (float)m; }

// ---- Init
inline void init(E&) {}
// ---- Mastering
inline void masterFourBand(E& e) { B(e, 0, 10, 120, -22, 2, -6, 30, 250); B(e, 1, 120, 900, -24, 1.8f, -5, 20, 200); B(e, 2, 900, 6000, -26, 2, -5, 10, 150); B(e, 3, 6000, 30000, -30, 2, -6, 5, 120); mode(e, kLinearPhase); }
inline void gentleGlue(E& e) { B(e, 0, 10, 200, -20, 1.5f, -3, 40, 300, 18); B(e, 1, 200, 4000, -22, 1.4f, -3, 30, 250, 18); B(e, 2, 4000, 30000, -26, 1.5f, -3, 15, 200, 18); }
inline void tameHarshness(E& e) { B(e, 0, 2200, 5500, -30, 3, -6, 3, 80); slope(e, 0, 18); }
inline void lowEndControl(E& e) { B(e, 0, 10, 150, -24, 3, -6, 20, 180); la(e, 0, 5); }
inline void airLift(E& e) { B(e, 0, 9000, 30000, -40, 3, 4, 10, 200); }
inline void midSideMaster(E& e) { B(e, 0, 10, 180, -24, 2.5f, -6, 25, 220); ms(e, 0, kMid); B(e, 1, 180, 30000, -28, 2, -4, 15, 180); ms(e, 1, kSide); }
// ---- Mix Bus
inline void busPunch(E& e) { B(e, 0, 10, 250, -24, 3, -6, 25, 120); B(e, 1, 250, 30000, -22, 2, -4, 15, 100); }
inline void deMud(E& e) { B(e, 0, 200, 500, -30, 3, -6, 10, 140); slope(e, 0, 12); }
inline void upwardDetail(E& e) { B(e, 0, 1500, 12000, -36, 2, 6, 5, 180); }
inline void sideCleanup(E& e) { B(e, 0, 10, 160, -50, 4, -24, 5, 100); ms(e, 0, kSide); slope(e, 0, 24); }
inline void fullFourWay(E& e) { B(e, 0, 10, 100, -20, 3, -8); B(e, 1, 100, 800, -22, 2.5f, -6); B(e, 2, 800, 5000, -24, 2.5f, -6); B(e, 3, 5000, 30000, -26, 2, -6); mode(e, kMinimumPhase); }
// ---- Drums
inline void kickTight(E& e) { B(e, 0, 40, 120, -20, 4, -8, 15, 90); B(e, 1, 120, 400, -40, 3, -12, 1, 120); expand(e, 1); }
inline void snareSnap(E& e) { B(e, 0, 2000, 8000, -30, 3, 6, 1, 60); expand(e, 0); }
inline void roomTamer(E& e) { B(e, 0, 200, 3000, -35, 3, -12, 1, 150); expand(e, 0); }
inline void overheadsSmooth(E& e) { B(e, 0, 3000, 12000, -28, 3, -6, 2, 90); la(e, 0, 2); }
inline void drumBusWeight(E& e) { B(e, 0, 10, 120, -40, 2, 6, 15, 250); B(e, 1, 2500, 30000, -26, 3, -5, 3, 100); }
inline void kickDucksBass(E& e) { B(e, 0, 30, 150, -30, 6, -8, 1, 120); e.p[bp(0, kBSidechain)] = 1; }
// ---- Vocals
inline void deEss(E& e) { B(e, 0, 5500, 10000, -32, 6, -12, 0.5f, 60, 6); slope(e, 0, 24); la(e, 0, 2); }
inline void vocalBody(E& e) { B(e, 0, 120, 400, -28, 2.5f, -6, 15, 160); B(e, 1, 2500, 5000, -32, 3, -6, 4, 100); }
inline void breathControl(E& e) { B(e, 0, 1500, 12000, -48, 3, -10, 3, 150); expand(e, 0); }
inline void presenceUp(E& e) { B(e, 0, 2000, 6000, -38, 2.5f, 5, 8, 160); }
inline void plosiveGuard(E& e) { B(e, 0, 10, 140, -26, 8, -18, 0.5f, 90); la(e, 0, 3); }
// ---- Bass & Keys
inline void bassEven(E& e) { B(e, 0, 10, 200, -26, 4, -10, 15, 140); B(e, 1, 600, 2500, -30, 2, -6, 5, 120); }
inline void subFocus(E& e) { B(e, 0, 10, 70, -24, 3, -6, 20, 160); ms(e, 0, kMid); }
inline void pianoTamer(E& e) { B(e, 0, 150, 600, -28, 2.5f, -6, 15, 180); B(e, 1, 2500, 6000, -30, 2.5f, -5, 6, 140); }
inline void padWidener(E& e) { B(e, 0, 500, 30000, -40, 2, 6, 20, 300); ms(e, 0, kSide); }
inline void guitarFizz(E& e) { B(e, 0, 4000, 9000, -34, 3, -8, 2, 90); slope(e, 0, 30); }
// ---- Repair
// 2.0: limiting (compress at Inf:1, fast attack, look-ahead) and gating (expand at Inf:1)
inline void bandLimiter(E& e) { B(e, 0, 10, 120, -18, 100, -24, 0.5f, 80, 2); slope(e, 0, 24); la(e, 0, 5); B(e, 1, 120, 2500, -14, 100, -24, 0.5f, 60, 2); slope(e, 1, 24); la(e, 1, 5);
                                B(e, 2, 2500, 30000, -20, 100, -24, 0.2f, 40, 2); slope(e, 2, 24); la(e, 2, 5); }
inline void drumGate(E& e) { B(e, 0, 10, 200, -42, 100, -40, 0.1f, 90, 0); expand(e, 0); slope(e, 0, 48); B(e, 1, 200, 5000, -40, 100, -30, 0.1f, 70, 0); expand(e, 1); slope(e, 1, 48);
                             B(e, 2, 5000, 30000, -46, 100, -24, 0.1f, 50, 0); expand(e, 2); slope(e, 2, 48); }
inline void humGate(E& e) { B(e, 0, 40, 140, -55, 6, -24, 1, 80); expand(e, 0); slope(e, 0, 48); }
inline void hissGate(E& e) { B(e, 0, 5000, 30000, -60, 4, -18, 1, 120); expand(e, 0); }
inline void resonanceSnuff(E& e) { B(e, 0, 280, 340, -36, 6, -12, 2, 80); slope(e, 0, 48); }
inline void rumbleControl(E& e) { B(e, 0, 10, 50, -36, 4, -18, 20, 200); }
inline void sibilanceSide(E& e) { B(e, 0, 5000, 11000, -36, 5, -10, 0.5f, 70); ms(e, 0, kSide); }
// ---- Creative
inline void pumpingLows(E& e) { B(e, 0, 10, 200, -36, 10, -18, 2, 300); detect(e, 0, 40, 120); }
inline void transientShaper(E& e) { B(e, 0, 10, 30000, -30, 4, -6, 25, 60); gain(e, 0, 3); }
inline void reverseDynamics(E& e) { B(e, 0, 200, 8000, -30, 4, 12, 30, 400); }
inline void spectralGate(E& e) { B(e, 0, 10, 300, -45, 8, -30, 1, 90); expand(e, 0); B(e, 1, 300, 3000, -45, 8, -30, 1, 90); expand(e, 1); B(e, 2, 3000, 30000, -50, 8, -30, 1, 90); expand(e, 2); }
inline void sideBloom(E& e) { B(e, 0, 200, 30000, -40, 3, 9, 30, 500); ms(e, 0, kSide); B(e, 1, 10, 200, -26, 3, -6, 15, 200); ms(e, 1, kMid); }
inline void hatsFromKick(E& e) { B(e, 0, 6000, 16000, -30, 10, -14, 1, 140); detect(e, 0, 40, 120); }
} // namespace pre

struct PresetDef { const char* name; const char* category; void (*fn)(Engine&); };
static const PresetDef kPresetDefs[] = {
    { "Init", "Init", pre::init },
    { "Four Band Master", "Mastering", pre::masterFourBand }, { "Gentle Glue", "Mastering", pre::gentleGlue }, { "Tame Harshness", "Mastering", pre::tameHarshness },
    { "Low End Control", "Mastering", pre::lowEndControl }, { "Air Lift", "Mastering", pre::airLift }, { "Mid/Side Master", "Mastering", pre::midSideMaster },
    { "Bus Punch", "Mix Bus", pre::busPunch }, { "De-Mud", "Mix Bus", pre::deMud }, { "Upward Detail", "Mix Bus", pre::upwardDetail },
    { "Side Cleanup", "Mix Bus", pre::sideCleanup }, { "Full Four-Way", "Mix Bus", pre::fullFourWay },
    { "Kick Tight", "Drums", pre::kickTight }, { "Snare Snap", "Drums", pre::snareSnap }, { "Room Tamer", "Drums", pre::roomTamer },
    { "Overheads Smooth", "Drums", pre::overheadsSmooth }, { "Drum Bus Weight", "Drums", pre::drumBusWeight }, { "Kick Ducks Bass (SC)", "Drums", pre::kickDucksBass },
    { "De-Esser", "Vocals", pre::deEss }, { "Vocal Body", "Vocals", pre::vocalBody }, { "Breath Control", "Vocals", pre::breathControl },
    { "Presence Up", "Vocals", pre::presenceUp }, { "Plosive Guard", "Vocals", pre::plosiveGuard },
    { "Bass Even", "Bass & Keys", pre::bassEven }, { "Sub Focus", "Bass & Keys", pre::subFocus }, { "Piano Tamer", "Bass & Keys", pre::pianoTamer },
    { "Pad Widener", "Bass & Keys", pre::padWidener }, { "Guitar Fizz", "Bass & Keys", pre::guitarFizz },
    { "Hum Gate", "Repair", pre::humGate }, { "Hiss Gate", "Repair", pre::hissGate }, { "Resonance Snuff", "Repair", pre::resonanceSnuff },
    { "Rumble Control", "Repair", pre::rumbleControl }, { "Sibilance (Side)", "Repair", pre::sibilanceSide },
    { "Pumping Lows", "Creative", pre::pumpingLows }, { "Transient Shaper", "Creative", pre::transientShaper }, { "Reverse Dynamics", "Creative", pre::reverseDynamics },
    { "Spectral Gate", "Creative", pre::spectralGate }, { "Side Bloom", "Creative", pre::sideBloom }, { "Hats From Kick", "Creative", pre::hatsFromKick },
    // 2.0 (appended, so the program numbers of the presets above stay the same)
    { "Three-Band Limiter", "Mastering", pre::bandLimiter }, { "Multiband Drum Gate", "Drums", pre::drumGate },
};
static constexpr uint32_t kNumPresets = sizeof(kPresetDefs) / sizeof(kPresetDefs[0]);

inline const Preset* presets() {
    static Preset* table = nullptr;
    if (table) return table;
    table = new Preset[kNumPresets];
    for (uint32_t i = 0; i < kNumPresets; ++i) {
        Engine* e = new Engine();
        kPresetDefs[i].fn(*e);
        table[i].name = kPresetDefs[i].name; table[i].category = kPresetDefs[i].category;
        std::copy(e->p, e->p + kParamCount, table[i].v);
        delete e;
    }
    return table;
}

} // namespace schist
