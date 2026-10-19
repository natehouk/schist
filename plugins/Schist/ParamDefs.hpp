// Parameter metadata shared by the DSP wrapper and the UI. License: MIT.
#pragma once
#include "SchistEngine.hpp"
#include <cstdio>
#include <string>

namespace schist {

static constexpr uint32_t kHintInteger = 1, kHintBoolean = 2, kHintLog = 4, kHintOutput = 8, kHintBipolar = 16, kHintNoAuto = 32;
struct ParamDef { const char* name; const char* sym; const char* unit; float min, max; uint32_t hints; };

inline const char* bandFieldName(int f) {
    static const char* const n[kBandFields] = { "Enabled", "Bypass", "Solo", "Low Frequency", "High Frequency", "Slope", "Mode", "Threshold", "Range",
        "Ratio", "Knee", "Attack", "Release", "Lookahead", "Gain", "Stereo Link", "Stereo Mode", "Sidechain", "Custom Detection", "Detection Low", "Detection High" };
    return n[f];
}

inline const ParamDef& paramDef(uint32_t i) {
    static ParamDef t[kParamCount];
    static std::string nm[kParamCount], sy[kParamCount];
    static bool done = false;
    if (!done) {
        done = true;
        auto set = [](uint32_t i, const std::string& n, const std::string& s, const char* u, uint32_t extra) {
            const Range r = paramRange(i); nm[i] = n; sy[i] = s;
            uint32_t h = extra | (r.log ? kHintLog : 0) | (r.integer ? kHintInteger : 0) | (r.min < 0 && r.max > 0 ? kHintBipolar : 0);
            t[i] = { nm[i].c_str(), sy[i].c_str(), u, r.min, r.max, h };
        };
        const uint32_t B = kHintBoolean, NA = kHintNoAuto;
        set(kProcMode, "Processing Mode", "procmode", "", 0);
        set(kOversampling, "Oversampling", "oversamp", "", 0);
        set(kMix, "Mix", "mix", "", 0);
        set(kOutGain, "Output Level", "outgain", "dB", 0);
        set(kOutPan, "Output Pan", "outpan", "", 0);
        set(kExpert, "Expert Mode", "expert", "", B | NA);
        set(kAnaMode, "Analyzer", "analyzer", "", NA);
        set(kAnaFreeze, "Analyzer Freeze", "freeze", "", B | NA);
        set(kDisplayRange, "Display Range", "disprange", "", NA);
        set(kMidiOn, "MIDI", "midion", "", B);
        static const char* const bs[kBandFields] = { "on", "byp", "solo", "lo", "hi", "slope", "mode", "thr", "range", "ratio", "knee", "att", "rel", "la", "gain", "link", "stereo", "sc", "dcust", "dlo", "dhi" };
        static const char* const bu[kBandFields] = { "", "", "", "Hz", "Hz", "", "", "dB", "dB", "", "dB", "ms", "ms", "ms", "dB", "", "", "", "", "Hz", "Hz" };
        for (int b = 0; b < kMaxBands; ++b) for (int f = 0; f < kBandFields; ++f) {
            const bool boolean = f == kBUsed || f == kBBypass || f == kBSolo || f == kBDetCustom;
            set(bp(b, f), "Band " + std::to_string(b + 1) + " " + bandFieldName(f), "b" + std::to_string(b + 1) + bs[f], bu[f], boolean ? B : 0);
        }
        set(kOutLearn, "MIDI Learn", "o_learn", "", kHintOutput);
    }
    return t[i];
}

inline int bandOf(uint32_t p) { return (p >= kBandFirst && p < kOutLearn) ? (int)((p - kBandFirst) / kBandFields) : -1; }
inline int fieldOf(uint32_t p) { return (p >= kBandFirst && p < kOutLearn) ? (int)((p - kBandFirst) % kBandFields) : -1; }

inline const char* const* enumLabels(uint32_t p, int& count) {
    static const char* const pm[] = { "Dynamic Phase", "Linear Phase", "Minimum Phase" };
    static const char* const os[] = { "Off", "2x", "4x" };
    static const char* const ana[] = { "Off", "Pre", "Post", "Pre+Post" };
    static const char* const rng[] = { "6 dB", "12 dB", "30 dB" };
    static const char* const slope[] = { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "30 dB/oct", "36 dB/oct", "42 dB/oct", "48 dB/oct" };
    static const char* const mode[] = { "Compress", "Expand" };
    static const char* const st[] = { "Stereo", "Mid", "Side" };
    static const char* const sc[] = { "Internal", "External" };
    count = 0;
    switch (p) {
    case kProcMode: count = 3; return pm;
    case kOversampling: count = 3; return os;
    case kAnaMode: count = 4; return ana;
    case kDisplayRange: count = 3; return rng;
    default: break;
    }
    switch (fieldOf(p)) {
    case kBSlope: count = 8; return slope;
    case kBMode: count = 2; return mode;
    case kBStereo: count = 3; return st;
    case kBSidechain: count = 2; return sc;
    default: return nullptr;
    }
}

inline float toNorm(uint32_t p, float v) { return normOf(p, v); }
inline float fromNorm(uint32_t p, float n) { return valueOf(p, n); }
inline float displayRangeDb(float v) { const int k = (int)clampf(std::round(v), 0, 2); return k == 0 ? 6.f : (k == 1 ? 12.f : 30.f); }

} // namespace schist
