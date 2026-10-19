// Schist — DPF plugin wrapper: parameters, programs, MIDI Learn, latency, external sidechain,
// and direct access for the UI (analyser, band levels and gain change). License: MIT.
#pragma once
#include "DistrhoPlugin.hpp"
#include "ParamDefs.hpp"
#include "Presets.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

START_NAMESPACE_DISTRHO

using namespace schist;

class SchistPlugin : public Plugin {
public:
    SchistPlugin() : Plugin(kParamCount, kNumPresets, 2) {
        for (int& m : ccMap) m = -1;
        engine.setSampleRate((float)getSampleRate());
    }
    Engine engine;                 // public: the UI reads the analyser and live band state directly

protected:
    const char* getLabel() const override { return "Schist"; }
    const char* getDescription() const override {
        return "Multiband dynamics: up to six freely placed bands, each compressing or expanding, downward or upward, "
               "in dynamic, linear or minimum phase, with look-ahead, mid/side, external and custom-range sidechains and 4x oversampling.";
    }
    const char* getMaker() const override { return "Epsilon Audio"; }
    const char* getHomePage() const override { return "https://epsilonrecords.com"; }
    const char* getLicense() const override { return "MIT"; }
    uint32_t getVersion() const override { return d_version(1, 0, 0); }

    void initAudioPort(bool input, uint32_t index, AudioPort& port) override {
        if (input && index >= 2) {                 // inputs 3-4: external sidechain for the bands
            port.hints = kAudioPortIsSidechain;
            port.name = index == 2 ? "Sidechain Left" : "Sidechain Right";
            port.symbol = index == 2 ? "sc_left" : "sc_right";
            port.groupId = kPortGroupNone;
            return;
        }
        Plugin::initAudioPort(input, index, port);
        port.groupId = kPortGroupNone;
    }

    void initParameter(uint32_t i, Parameter& prm) override {
        const ParamDef& d = paramDef(i);
        prm.hints = (d.hints & kHintOutput) ? kParameterIsOutput
                  : (((d.hints & kHintNoAuto) ? 0 : kParameterIsAutomatable)
                     | ((d.hints & kHintInteger) ? kParameterIsInteger : 0)
                     | ((d.hints & kHintBoolean) ? kParameterIsBoolean : 0)
                     | ((d.hints & kHintLog) ? kParameterIsLogarithmic : 0));
        prm.name = d.name; prm.symbol = d.sym; prm.unit = d.unit;
        prm.ranges.min = d.min; prm.ranges.max = d.max;
        prm.ranges.def = std::min(d.max, std::max(d.min, defaults()[i]));
        int n = 0;
        if (const char* const* labels = enumLabels(i, n)) {
            ParameterEnumerationValue* ev = new ParameterEnumerationValue[n];
            for (int k = 0; k < n; ++k) { ev[k].value = (float)k; ev[k].label = labels[k]; }
            prm.enumValues.count = (uint8_t)n; prm.enumValues.restrictedMode = true; prm.enumValues.values = ev;
        }
    }
    static const float* defaults() { return presets()[0].v; }    // the Init preset is the engine's defaults

    void initState(uint32_t index, State& state) override {
        if (index == 0) { state.key = "midimap"; state.defaultValue = ""; state.label = "MIDI Learn map"; state.hints = kStateIsHostReadable; }
        else            { state.key = "cmd";     state.defaultValue = ""; state.label = "UI command";     state.hints = kStateIsOnlyForDSP; }
    }
    String getState(const char* key) const override {
        if (std::strcmp(key, "midimap") == 0) return String(serializeMap().c_str());
        return String();
    }
    void setState(const char* key, const char* value) override {
        if (std::strcmp(key, "midimap") == 0) { parseMap(value); return; }
        if (std::strcmp(key, "cmd") == 0 && value && *value) {
            if (std::strncmp(value, "learn:", 6) == 0)      learnTarget = std::atoi(value + 6);
            else if (std::strcmp(value, "cancel") == 0)     learnTarget = -1;
            else if (std::strncmp(value, "unmap:", 6) == 0) { const int p = std::atoi(value + 6); for (int& m : ccMap) if (m == p) m = -1; }
            else if (std::strcmp(value, "clear") == 0)      { for (int& m : ccMap) m = -1; }
        }
    }

    void initProgramName(uint32_t i, String& name) override { name = presets()[i].name; }
    void loadProgram(uint32_t i) override { for (uint32_t k = 0; k < kOutLearn; ++k) if (!(paramDef(k).hints & kHintNoAuto)) engine.p[k] = presets()[i].v[k]; }   // view settings stay

    float getParameterValue(uint32_t i) const override { return i == kOutLearn ? learnEcho : engine.p[i]; }
    void setParameterValue(uint32_t i, float v) override { if (i < kOutLearn) engine.p[i] = v; }

    void activate() override { engine.setSampleRate((float)getSampleRate()); reportedLatency = engine.latency(); setLatency(reportedLatency); }
    void sampleRateChanged(double sr) override { engine.setSampleRate((float)sr); }

    void run(const float** inputs, float** outputs, uint32_t frames, const MidiEvent* ev, uint32_t evCount) override {
        if (engine.latency() != reportedLatency) { reportedLatency = engine.latency(); setLatency(reportedLatency); }

        const bool midiOn = engine.p[kMidiOn] > 0.5f;
        uint32_t pos = 0;
        for (uint32_t e = 0; e <= evCount; ++e) {
            const uint32_t until = e < evCount ? std::min(ev[e].frame, frames) : frames;
            if (until > pos) {
                engine.process(inputs[0] + pos, inputs[1] + pos, inputs[2] + pos, inputs[3] + pos, outputs[0] + pos, outputs[1] + pos, until - pos);
                pos = until;
            }
            if (e == evCount) break;
            if (!midiOn || ev[e].size > 3) continue;
            const uint8_t* d = ev[e].data;
            switch (d[0] & 0xF0) {
            case 0xB0: {
                const int cc = d[1] & 0x7F, val = d[2] & 0x7F;
                if (learnTarget >= 0 && learnTarget < (int)kOutLearn) {
                    for (int& m : ccMap) if (m == learnTarget) m = -1;
                    ccMap[cc] = learnTarget;
                    learnSerial = (learnSerial + 1) % 8;
                    learnEcho = (float)(learnSerial * 1000000 + learnTarget * 128 + cc + 1);
                    learnTarget = -1;
                }
                if (ccMap[cc] >= 0) {
                    const float v = fromNorm((uint32_t)ccMap[cc], val / 127.f);
                    engine.p[ccMap[cc]] = v;
                    requestParameterValueChange((uint32_t)ccMap[cc], v);
                }
                break; }
            default: break;
            }
        }
    }

private:
    std::string serializeMap() const {
        std::string s; char b[24];
        for (int cc = 0; cc < 128; ++cc) if (ccMap[cc] >= 0) { std::snprintf(b, sizeof b, "%d:%d;", cc, ccMap[cc]); s += b; }
        return s;
    }
    void parseMap(const char* v) {
        for (int& m : ccMap) m = -1;
        if (!v) return;
        const char* q = v;
        while (*q) {
            int cc = -1, prm = -1;
            if (std::sscanf(q, "%d:%d", &cc, &prm) == 2 && cc >= 0 && cc < 128 && prm >= 0 && prm < (int)kOutLearn) ccMap[cc] = prm;
            const char* semi = std::strchr(q, ';');
            if (!semi) break;
            q = semi + 1;
        }
    }

    int ccMap[128];
    int learnTarget = -1, learnSerial = 0;
    float learnEcho = 0;
    uint32_t reportedLatency = 0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SchistPlugin)
};

END_NAMESPACE_DISTRHO
