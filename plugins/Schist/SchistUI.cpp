// Schist — custom vector UI (DPF/NanoVG). Original design. License: MIT.
#include "DistrhoUI.hpp"
#include "SchistPlugin.hpp"
#include "UserPresets.hpp"
#include "ResizeHandle.hpp"
#include "Help.hpp"
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
extern "C" __declspec(dllimport) void* __stdcall ShellExecuteA(void*, const char*, const char*, const char*, const char*, int);
#endif

START_NAMESPACE_DISTRHO

static void openURL(const char* url) {
#if defined(_WIN32)
    ShellExecuteA(nullptr, "open", url, nullptr, nullptr, 1);
#elif defined(__APPLE__)
    const std::string c = std::string("open '") + url + "' &"; if (std::system(c.c_str())) {}
#else
    const std::string c = std::string("xdg-open '") + url + "' >/dev/null 2>&1 &"; if (std::system(c.c_str())) {}
#endif
}
static bool gTipsOn = true;
static double nowSec() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

using namespace schist;
using DGL_NAMESPACE::ResizeHandle;

static constexpr float W = 1180.f, H = 800.f;

#define C_BG        Color(0x10, 0x12, 0x11)
#define C_PANEL     Color(0x18, 0x1b, 0x1a)
#define C_PANEL_HI  Color(0x24, 0x28, 0x27)
#define C_LINE      Color(0x30, 0x35, 0x33)
#define C_TEXT      Color(0xe6, 0xea, 0xe7)
#define C_DIM       Color(0x8b, 0x93, 0x8f)
#define C_FAINT     Color(0x4c, 0x54, 0x51)
#define C_ACC       Color(0x8c, 0xd6, 0x9a)       // green schist
#define C_CUT       Color(0xef, 0x6a, 0x5a)
#define C_BOOST     Color(0x7f, 0xe0, 0x8e)
#define C_DISPLAY   Color(0x0a, 0x0c, 0x0b)
#define C_FAV       Color(0xff, 0xc8, 0x5a)       // favourite star

static Color withAlpha(const Color& c, float a) { Color r(c); r.alpha = a; return r; }
static Color bandColor(int b) {
    static const Color c[kMaxBands] = { Color(0x8c, 0xd6, 0x9a), Color(0x5c, 0xc8, 0xd0), Color(0x7a, 0x9c, 0xff), Color(0xc2, 0x8c, 0xff), Color(0xf0, 0xb4, 0x5a), Color(0xf2, 0x7f, 0x9c) };
    return c[b % kMaxBands];
}

// ---------------------------------------------------------------- layout
static constexpr float DX = 16, DY = 62, DW = 1068, DH = 358;          // display
static constexpr float MX = 1092, MY = 62, MW = 72, MH = 358;          // meters
static constexpr float BX = 16, BY = 430, BW = 800, BH = 354;          // band panel
static constexpr float GX = 824, GY = 430, GW = 340, GH = 354;         // global panel
static constexpr float SEG_H = 22;
static constexpr float TX = BX + 604, TY = BY + 52, TS = 180;          // transfer curve
static constexpr float BMX = BX + 18, BMY = BY + 60, BMH = 186;        // band meter
static constexpr float REM_X = BX + BW - 84, REM_Y = BY + 12, REM_W = 68, REM_H = 24;
static constexpr float GAIN_TOP = DY + 24, GAIN_BOT = DY + DH - 48, DET_Y = DY + DH - 31;

enum WType { Knob, Toggle, Segment, Stepper };
enum Group { GGlobal, GBand, GExpert };
struct Ctl { WType type; int group, field; float x, y, r; const char* label; };   // Segment/Stepper: r = width

static std::vector<Ctl> buildControls() {
    std::vector<Ctl> c;
    // band panel
    c.push_back({ Segment, GBand, kBMode, BX + 318, BY + 24, 168, "" });
    c.push_back({ Toggle, GBand, kBSolo, BX + 432, BY + 24, 0, "SOLO" });
    c.push_back({ Toggle, GBand, kBBypass, BX + 512, BY + 24, 0, "BYPASS" });
    c.push_back({ Knob, GBand, kBThreshold, BX + 128, BY + 112, 30, "THRESHOLD" });
    c.push_back({ Knob, GBand, kBRange, BX + 226, BY + 112, 30, "RANGE" });
    c.push_back({ Knob, GBand, kBRatio, BX + 314, BY + 112, 21, "RATIO" });
    c.push_back({ Knob, GBand, kBKnee, BX + 386, BY + 112, 21, "KNEE" });
    c.push_back({ Knob, GBand, kBLo, BX + 458, BY + 112, 21, "LOW" });
    c.push_back({ Knob, GBand, kBHi, BX + 530, BY + 112, 21, "HIGH" });
    c.push_back({ Knob, GBand, kBAttack, BX + 128, BY + 214, 21, "ATTACK" });
    c.push_back({ Knob, GBand, kBRelease, BX + 200, BY + 214, 21, "RELEASE" });
    c.push_back({ Knob, GBand, kBLookahead, BX + 272, BY + 214, 21, "LOOKAHEAD" });
    c.push_back({ Knob, GBand, kBGain, BX + 344, BY + 214, 21, "GAIN" });
    c.push_back({ Knob, GBand, kBLink, BX + 416, BY + 214, 21, "LINK" });
    c.push_back({ Stepper, GBand, kBSlope, BX + 516, BY + 214, 108, "" });
    // expert strip
    c.push_back({ Segment, GExpert, kBStereo, BX + 168, BY + 312, 192, "" });
    c.push_back({ Segment, GExpert, kBSidechain, BX + 352, BY + 312, 150, "" });
    c.push_back({ Toggle, GExpert, kBDetCustom, BX + 466, BY + 312, 0, "CUSTOM" });
    c.push_back({ Knob, GExpert, kBDetLo, BX + 640, BY + 304, 15, "DET LOW" });
    c.push_back({ Knob, GExpert, kBDetHi, BX + 724, BY + 304, 15, "DET HIGH" });
    // global panel
    c.push_back({ Segment, GGlobal, kProcMode, GX + 170, GY + 46, 304, "" });
    c.push_back({ Segment, GGlobal, kOversampling, GX + 96, GY + 106, 150, "" });
    c.push_back({ Toggle, GGlobal, kExpert, GX + 226, GY + 106, 0, "EXPERT" });
    c.push_back({ Knob, GGlobal, kMix, GX + 70, GY + 186, 26, "MIX" });
    c.push_back({ Knob, GGlobal, kOutGain, GX + 170, GY + 186, 26, "OUTPUT" });
    c.push_back({ Knob, GGlobal, kOutPan, GX + 270, GY + 186, 26, "PAN" });
    c.push_back({ Segment, GGlobal, kAnaMode, GX + 120, GY + 282, 212, "" });
    c.push_back({ Toggle, GGlobal, kAnaFreeze, GX + 250, GY + 282, 0, "FREEZE" });
    c.push_back({ Segment, GGlobal, kDisplayRange, GX + 206, GY + 326, 180, "" });
    return c;
}
static const std::vector<Ctl>& controls() { static std::vector<Ctl> c = buildControls(); return c; }

static const char* const* segLabels(int field, bool global, int& n) {
    static const char* const pm[] = { "DYNAMIC", "LINEAR", "MINIMUM" };
    static const char* const os[] = { "OFF", "2X", "4X" };
    static const char* const ana[] = { "OFF", "PRE", "POST", "BOTH" };
    static const char* const rng[] = { "6 DB", "12 DB", "30 DB" };
    static const char* const md[] = { "COMPRESS", "EXPAND" };
    static const char* const st[] = { "STEREO", "MID", "SIDE" };
    static const char* const sc[] = { "INTERNAL", "EXTERNAL" };
    n = 0;
    if (global) {
        switch (field) { case kProcMode: n = 3; return pm; case kOversampling: n = 3; return os; case kAnaMode: n = 4; return ana; case kDisplayRange: n = 3; return rng; default: return nullptr; }
    }
    switch (field) { case kBMode: n = 2; return md; case kBStereo: n = 3; return st; case kBSidechain: n = 2; return sc; default: return nullptr; }
}

static constexpr float PS_X = 400.f, PS_Y = 18.f, PS_W = 220.f, PS_H = 28.f, PS_ARROW = 28.f;
static constexpr float A_X = 628.f, AB_W = 30.f, COPY_X = 692.f, COPY_W = 46.f;
static constexpr float UNDO_X = 750.f, REDO_X = 802.f, UR_W = 48.f, LEARN_X = 862.f, LEARN_W = 66.f, MIDI_X = 932.f, MIDI_W = 48.f, HELP_X = 986.f, HELP_W = 48.f;
static constexpr float PM_COLW = 127.f, PM_ROW = 22.f, PM_HEAD = 22.f, PM_PAD = 8.f;
static constexpr int ANA_N = 4096, ANA_PTS = 320, CURVE_PTS = 240;

class SchistUI : public UI {
public:
    SchistUI() : UI((uint)W, (uint)H), resizeHandle(this) {
        loadSharedResources();
        const Preset& d = presets()[0];
        for (uint32_t i = 0; i < kParamCount; ++i) { val[i] = d.v[i]; defv[i] = d.v[i]; }
        for (int& m : paramCC) m = -1;
        const double sf = getScaleFactor();
        if (sf != 1.0) setSize((uint)(W * sf), (uint)(H * sf));
        setGeometryConstraints((uint)(W * 0.6 * sf), (uint)(H * 0.6 * sf), true);
        if (isResizable()) resizeHandle.hide();
        userPresets = listUserPresets();
        std::copy(val, val + kParamCount, before);
        fft.init(ANA_N);
        for (int i = 0; i < ANA_PTS; ++i) specIn[i] = specOut[i] = specHold[i] = -120;
        for (int i = 0; i < CURVE_PTS; ++i) total[i] = 0;
    }

protected:
    float uiScale() const { return (float)getWidth() / W; }
    static bool isView(uint32_t p) { return (paramDef(p).hints & kHintNoAuto) != 0; }

    // ---- values -------------------------------------------------------------
    void setValue(uint32_t p, float v) {
        const ParamDef& d = paramDef(p); v = clampf(v, d.min, d.max);
        if (d.hints & kHintInteger) v = std::round(v);
        if (v == val[p]) return;
        val[p] = v; setParameterValue(p, v); repaint();
    }
    void commit(uint32_t p, float v) { editParameter(p, true); setValue(p, v); editParameter(p, false); }
    void parameterChanged(uint32_t i, float v) override {
        if (i >= kParamCount) return;
        if (i == kOutLearn) {
            if (v != lastLearnEcho && v > 0) {
                const int code = (int)v % 1000000 - 1, prm = code / 128, cc = code % 128;
                for (int& m : paramCC) if (m == cc) m = -1;
                if (prm >= 0 && prm < (int)kParamCount) paramCC[prm] = cc;
                learnWaiting = -1;
            }
            lastLearnEcho = v; repaint(); return;
        }
        if (val[i] != v) { val[i] = v; if (!gesture) before[i] = v; repaint(); }
        if (selBand >= 0 && !used(selBand)) selBand = firstUsed();
    }
    void stateChanged(const char* key, const char* value) override {
        if (std::strcmp(key, "midimap") != 0) return;
        for (int& m : paramCC) m = -1;
        const char* q = value;
        while (q && *q) { int cc = -1, prm = -1; if (std::sscanf(q, "%d:%d", &cc, &prm) == 2 && prm >= 0 && prm < (int)kParamCount) paramCC[prm] = cc;
            const char* s = std::strchr(q, ';'); if (!s) break; q = s + 1; }
        repaint();
    }

    // ---- undo / redo / A-B (view settings are not part of a setting) ------------------
    typedef std::vector<float> Snap;
    Snap snapshot() const { Snap s(val, val + kParamCount); s.push_back((float)abSlot); return s; }
    bool differs(const float* a, const float* b) const { for (uint32_t i = 0; i < kOutLearn; ++i) if (!isView(i) && a[i] != b[i]) return true; return false; }
    void beginGesture() { gesture = true; std::copy(val, val + kParamCount, before); }
    void endGesture() {
        gesture = false;
        if (differs(before, val)) { Snap u(before, before + kParamCount); u.push_back((float)abSlot); undoStack.push_back(u); if (undoStack.size() > 200) undoStack.erase(undoStack.begin()); redoStack.clear(); }
        std::copy(val, val + kParamCount, before); repaint();
    }
    void applyAll(const float* s) {
        for (uint32_t i = 0; i < kOutLearn; ++i) if (!isView(i) && val[i] != s[i]) commit(i, s[i]);
        if (selBand < 0 || !used(selBand)) selBand = firstUsed();
    }
    void restoreSnap(const Snap& s) {
        if (s.size() > kParamCount && (int)s[kParamCount] != abSlot) { abStore[abSlot] = snapshot(); abSlot = (int)s[kParamCount]; }
        applyAll(s.data()); std::copy(val, val + kParamCount, before);
    }
    void undo() { if (undoStack.empty()) return; redoStack.push_back(snapshot()); restoreSnap(undoStack.back()); undoStack.pop_back(); flash("Undo"); }
    void redo() { if (redoStack.empty()) return; undoStack.push_back(snapshot()); restoreSnap(redoStack.back()); redoStack.pop_back(); flash("Redo"); }
    void switchAB(int to) {
        if (to == abSlot) return;
        abStore[abSlot] = snapshot(); if (abStore[to].empty()) abStore[to] = snapshot();
        beginGesture(); applyAll(abStore[to].data()); endGesture();
        abSlot = to; flash(to == 0 ? "Setting A" : "Setting B");
    }
    void copyAB() { abStore[1 - abSlot] = snapshot(); flash(abSlot == 0 ? "Copied A to B" : "Copied B to A"); }

    // ---- bands --------------------------------------------------------------------
    bool used(int b) const { return b >= 0 && b < kMaxBands && val[bp(b, kBUsed)] > 0.5f; }
    int firstUsed() const { for (int b = 0; b < kMaxBands; ++b) if (used(b)) return b; return -1; }
    int numUsed() const { int n = 0; for (int b = 0; b < kMaxBands; ++b) n += used(b); return n; }
    float lo(int b) const { return val[bp(b, kBLo)]; }
    float hi(int b) const { return val[bp(b, kBHi)]; }
    float centre(int b) const { return bandCentre(lo(b), hi(b)); }
    int mode() const { return (int)std::round(val[kProcMode]); }
    int order(int b) const { const int T = slopeOrder(val[bp(b, kBSlope)]); return mode() == kMinimumPhase && T > 1 ? 2 * ((T + 1) / 2) : T; }
    float curveDb(int b, float f, float g) const { return mode() == kDynamicPhase ? bandDynDb(f, lo(b), hi(b), order(b), g) : bandSplitDb(f, lo(b), hi(b), order(b), g); }
    bool expert() const { return val[kExpert] > 0.5f; }
    // how far a band can move or grow before it meets its neighbours
    float leftLimit(int b, int skip = -1) const { float l = kFreqMin; const float c = centre(b); for (int k = 0; k < kMaxBands; ++k) if (k != b && k != skip && used(k) && centre(k) < c) l = std::max(l, hi(k)); return l; }
    float rightLimit(int b, int skip = -1) const { float r = kFreqMax; const float c = centre(b); for (int k = 0; k < kMaxBands; ++k) if (k != b && k != skip && used(k) && centre(k) > c) r = std::min(r, lo(k)); return r; }
    int neighbourAt(int b, int side) const {      // a band whose edge touches this band's lo (side 0) or hi (side 1)
        for (int k = 0; k < kMaxBands; ++k) { if (k == b || !used(k)) continue;
            if (side == 0 && !openLo(lo(b)) && std::fabs(std::log(hi(k) / lo(b))) < 0.005f) return k;
            if (side == 1 && !openHi(hi(b)) && std::fabs(std::log(lo(k) / hi(b))) < 0.005f) return k; }
        return -1;
    }

    // ---- parameter of a control --------------------------------------------------
    uint32_t effParam(const Ctl& c) const { return c.group == GGlobal ? (uint32_t)c.field : bp(std::max(0, selBand), c.field); }
    bool visible(const Ctl& c) const {
        if (c.group == GGlobal) return true;
        if (selBand < 0) return false;
        if (c.group == GExpert) return expert();
        return true;
    }
    bool dimmed(const Ctl& c) const {
        if (c.group == GGlobal && c.field == kOversampling) return mode() == kLinearPhase;
        if (c.group == GExpert && (c.field == kBDetLo || c.field == kBDetHi)) return val[bp(selBand, kBDetCustom)] < 0.5f;
        if (c.group == GBand && c.field == kBRatio) return std::fabs(val[bp(selBand, kBRange)]) < 0.05f;
        return false;
    }

    // ---- formatting ---------------------------------------------------------
    static std::string fmtHz(float hz) { char b[24]; if (hz >= 1000) snprintf(b, sizeof b, hz >= 10000 ? "%.1f kHz" : "%.2f kHz", hz / 1000); else snprintf(b, sizeof b, hz < 100 ? "%.1f Hz" : "%.0f Hz", hz); return b; }
    void fmtValue(uint32_t p, float v, char* b, size_t n) const {
        int ne = 0;
        if (const char* const* l = enumLabels(p, ne)) { snprintf(b, n, "%s", l[std::max(0, std::min(ne - 1, (int)std::round(v)))]); return; }
        const ParamDef& d = paramDef(p);
        if (d.hints & kHintBoolean) { snprintf(b, n, "%s", v > 0.5f ? "On" : "Off"); return; }
        const int f = fieldOf(p);
        if (f == kBLo && openLo(v)) { snprintf(b, n, "Shelf"); return; }
        if (f == kBHi && openHi(v)) { snprintf(b, n, "Shelf"); return; }
        if (std::strcmp(d.unit, "Hz") == 0) { snprintf(b, n, "%s", fmtHz(v).c_str()); return; }
        if (f == kBRatio) { if (ratioIsInf(v)) snprintf(b, n, "Inf:1"); else snprintf(b, n, v >= 10 ? "%.0f:1" : "%.1f:1", v); return; }
        if (f == kBThreshold || f == kBKnee) { snprintf(b, n, "%.1f dB", v); return; }
        if (std::strcmp(d.unit, "dB") == 0) { snprintf(b, n, "%+.1f dB", v); return; }
        if (std::strcmp(d.unit, "ms") == 0) { if (v >= 1000) snprintf(b, n, "%.2f s", v / 1000); else snprintf(b, n, v < 1 ? "%.2f ms" : (v < 10 ? "%.1f ms" : "%.0f ms"), v); return; }
        if (p == kOutPan) { if (std::fabs(v) < 0.005f) snprintf(b, n, "C"); else snprintf(b, n, "%s%.0f", v < 0 ? "L" : "R", std::fabs(v) * 100); return; }
        snprintf(b, n, "%.0f%%", v * 100);
    }
    void fmt(uint32_t p, char* b, size_t n) const { fmtValue(p, val[p], b, n); }
    bool parse(uint32_t p, const std::string& in, float& out) const {
        std::string s; for (char ch : in) if (!std::isspace((unsigned char)ch)) s += (char)std::tolower((unsigned char)ch);
        if (s.empty()) return false;
        if (p == kOutPan) { if (s == "c") { out = 0; return true; } if (s[0] == 'l' || s[0] == 'r') { out = (float)(std::atof(s.c_str() + 1) / 100.0 * (s[0] == 'l' ? -1 : 1)); return true; } }
        char* end = nullptr; double x = std::strtod(s.c_str(), &end);
        if (end == s.c_str()) return false;
        const std::string suf(end); const ParamDef& d = paramDef(p);
        if (suf == "k" || suf == "khz") x *= 1000;
        if (std::strcmp(d.unit, "ms") == 0) { out = (float)(suf == "s" ? x * 1000 : x); return true; }
        if (std::strcmp(d.unit, "Hz") == 0 || std::strcmp(d.unit, "dB") == 0 || fieldOf(p) == kBRatio) { out = (float)x; return true; }
        if (p == kOutPan) { out = (float)(x / 100.0); return true; }
        out = (float)(x / 100.0); return true;
    }

    // ---- drawing ------------------------------------------------------------
    void onNanoDisplay() override {
        const float s = uiScale();
        save(); scale(s, s);
        beginPath(); rect(0, 0, W, H); fillColor(C_BG); fill();
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        drawHeader();
        drawDisplay();
        drawMeters();
        drawBandPanel();
        drawGlobalPanel();
        const std::vector<Ctl>& cs = controls();
        for (int i = 0; i < (int)cs.size(); ++i) if (visible(cs[i])) drawCtl(cs[i], i == active || i == hover);
        if (editing >= 0) drawEditBox();
        drawStatus();
        if (menuOpen) drawPresetMenu();
        if (saving) drawSaveBox();
        if (helpOpen) drawHelp();
        else if (tipShown && gTipsOn) drawTooltip();
        restore();
    }
    void panel(float x, float y, float w, float h) { beginPath(); roundedRect(x, y, w, h, 6); fillColor(C_PANEL); fill(); strokeColor(C_LINE); strokeWidth(1); stroke(); }
    void caption(float x, float y, const char* t, int align = ALIGN_CENTER) { fontSize(7.5f); textLetterSpacing(1.8f); textAlign(align | ALIGN_MIDDLE); fillColor(C_FAINT); text(x, y, t, nullptr); }
    void drawHeaderButton(float x, float w, const char* label, bool on, bool hot, bool disabled) {
        beginPath(); roundedRect(x, PS_Y, w, PS_H, 5); fillColor(on ? withAlpha(C_ACC, 0.2f) : C_PANEL); fill();
        strokeColor(on ? C_ACC : (hot && !disabled ? C_DIM : C_LINE)); strokeWidth(1); stroke();
        fontSize(9); textLetterSpacing(1.6f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(disabled ? C_FAINT : (on ? C_TEXT : C_DIM));
        text(x + w / 2, PS_Y + PS_H / 2 + 0.5f, label, nullptr);
    }
    void drawHeader() {
        // logo: foliated layers of schist, with a glint of mica
        beginPath(); roundedRect(16, 17, 30, 30, 4); fillColor(Color(0x22, 0x28, 0x26)); fill();
        for (int k = 0; k < 5; ++k) {
            const float y = 22.5f + k * 5.f;
            beginPath(); moveTo(19, y + 1.5f); bezierTo(26, y - 2.f, 33, y + 3.f, 43, y - 0.5f);
            strokeColor(k % 2 ? withAlpha(C_ACC, 0.55f) : bandColor(k)); strokeWidth(k % 2 ? 1.4f : 2.f); lineCap(ROUND); stroke();
        }
        beginPath(); circle(37, 26, 1.4f); fillColor(Color(0xf4, 0xf0, 0xe2)); fill();
        fontSize(22); textLetterSpacing(9); fillColor(C_TEXT); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); text(56, 33, "SCHIST", nullptr);
        fontSize(9); textLetterSpacing(2.2f); fillColor(C_DIM); text(204, 28, "MULTIBAND DYNAMICS", nullptr);
        fontSize(7); textLetterSpacing(1.1f); fillColor(C_FAINT); text(204, 40, "EPSILON AUDIO 2.0", nullptr);
        drawPresetBar();
        for (int k = 0; k < 2; ++k) {
            const float x = A_X + k * AB_W; const bool on = abSlot == k, hot = hoverHeader == 6 + k;
            beginPath(); roundedRect(x, PS_Y, AB_W - 2, PS_H, 5); fillColor(on ? withAlpha(C_ACC, 0.25f) : C_PANEL); fill();
            strokeColor(on ? C_ACC : (hot ? C_DIM : C_LINE)); strokeWidth(1); stroke();
            fontSize(10); textLetterSpacing(0); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(on ? C_TEXT : C_DIM); text(x + (AB_W - 2) / 2, PS_Y + PS_H / 2 + 0.5f, k ? "B" : "A", nullptr);
        }
        drawHeaderButton(COPY_X, COPY_W, "COPY", false, hoverHeader == 8, false);
        drawHeaderButton(UNDO_X, UR_W, "UNDO", false, hoverHeader == 3, undoStack.empty());
        drawHeaderButton(REDO_X, UR_W, "REDO", false, hoverHeader == 4, redoStack.empty());
        drawHeaderButton(LEARN_X, LEARN_W, learnMode ? "LEARNING" : "LEARN", learnMode, hoverHeader == 1, false);
        drawHeaderButton(MIDI_X, MIDI_W, "MIDI", val[kMidiOn] > 0.5f, hoverHeader == 2, false);
        drawHeaderButton(HELP_X, HELP_W, "HELP", helpOpen, hoverHeader == 5, false);
        char b[24]; snprintf(b, sizeof b, "%.1f ms", latencyMs);
        fontSize(7); textLetterSpacing(1.4f); textAlign(ALIGN_RIGHT | ALIGN_MIDDLE); fillColor(C_FAINT); text(W - 16, 27, "LATENCY", nullptr);
        fontSize(9.5f); textLetterSpacing(0.4f); fillColor(C_DIM); text(W - 16, 40, b, nullptr);
    }

    // ---- display ------------------------------------------------------------------
    static float xOfHz(float hz) { return DX + 6 + (DW - 12) * std::log(clampf(hz, kFreqMin, kFreqMax) / kFreqMin) / std::log(kFreqMax / kFreqMin); }
    static float hzOfX(float x) { return kFreqMin * std::pow(kFreqMax / kFreqMin, clampf((x - DX - 6) / (DW - 12), 0.f, 1.f)); }
    float rangeDb() const { return displayRangeDb(val[kDisplayRange]); }
    float yOfDb(float db) const { const float mid = 0.5f * (GAIN_TOP + GAIN_BOT), half = 0.5f * (GAIN_BOT - GAIN_TOP); return mid - clampf(db / rangeDb(), -1.08f, 1.08f) * half; }
    float dbPerPx() const { return rangeDb() / (0.5f * (GAIN_BOT - GAIN_TOP)); }
    static float yOfSpec(float db) { return DY + 6 + (DH - 26) * clampf((6.f - db) / 96.f, 0.f, 1.f); }
    float bandX0(int b) const { return openLo(lo(b)) ? DX + 1 : xOfHz(lo(b)); }
    float bandX1(int b) const { return openHi(hi(b)) ? DX + DW - 1 : xOfHz(hi(b)); }
    float handleDb(int b) const { return val[bp(b, kBRange)] + val[bp(b, kBGain)]; }
    float handleX(int b) const { return 0.5f * (std::max(bandX0(b), DX + 30) + std::min(bandX1(b), DX + DW - 30)); }

    void curvePath(int b, float g, bool closeToZero) {
        const float x0 = std::max(DX + 1, bandX0(b) - 140), x1 = std::min(DX + DW - 1, bandX1(b) + 140);
        const int n = 120; const float y0 = yOfDb(0);
        beginPath(); if (closeToZero) moveTo(x0, y0);
        for (int i = 0; i <= n; ++i) { const float x = x0 + (x1 - x0) * i / n, y = yOfDb(curveDb(b, hzOfX(x), g)); if (!closeToZero && i == 0) moveTo(x, y); else lineTo(x, y); }
        if (closeToZero) { lineTo(x1, y0); closePath(); }
    }
    void drawDisplay() {
        beginPath(); roundedRect(DX, DY, DW, DH, 6); fillColor(C_DISPLAY); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        scissor(DX + 1, DY + 1, DW - 2, DH - 2);
        // grid
        const float grid[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 }; const char* gl[] = { "20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };
        beginPath(); for (float g : grid) { const float x = xOfHz(g); moveTo(x, DY + 6); lineTo(x, DY + DH - 18); }
        const float R = rangeDb(), step = R <= 6 ? 2.f : (R <= 12 ? 3.f : 10.f);
        for (float db = -R; db <= R + 0.01f; db += step) { const float y = yOfDb(db); moveTo(DX + 6, y); lineTo(DX + DW - 6, y); }
        strokeColor(Color(255, 255, 255, 7 / 255.f)); strokeWidth(1); stroke();
        beginPath(); moveTo(DX + 6, yOfDb(0)); lineTo(DX + DW - 6, yOfDb(0)); strokeColor(Color(255, 255, 255, 18 / 255.f)); stroke();
        fontSize(8); textLetterSpacing(1); textAlign(ALIGN_CENTER | ALIGN_BOTTOM); fillColor(C_FAINT);
        for (int i = 0; i < 10; ++i) text(xOfHz(grid[i]), DY + DH - 4, gl[i], nullptr);
        textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
        for (float db = -R; db <= R + 0.01f; db += step * (R <= 12 ? 2 : 1)) { char b[12]; snprintf(b, sizeof b, db == 0 ? "0" : "%+.0f", db); text(DX + DW - 8, yOfDb(db) - 6, b, nullptr); }
        // spectrum
        const int am = (int)std::round(val[kAnaMode]);
        auto specPath = [&](const float* sp) { beginPath(); moveTo(DX + 6, DY + DH);
            for (int i = 0; i < ANA_PTS; ++i) lineTo(DX + 6 + (DW - 12) * i / (ANA_PTS - 1.f), yOfSpec(sp[i]));
            lineTo(DX + DW - 6, DY + DH); closePath(); };
        if (am == 1 || am == 3) { specPath(specIn); fillColor(Color(0x60, 0x6a, 0x66, am == 3 ? 0.22f : 0.32f)); fill(); }
        if (am >= 2) { specPath(specOut); fillPaint(linearGradient(0, DY, 0, DY + DH, withAlpha(C_ACC, 0.22f), withAlpha(C_ACC, 0.03f))); fill(); }
        if (val[kAnaFreeze] > 0.5f && am > 0) {
            beginPath(); for (int i = 0; i < ANA_PTS; ++i) { const float x = DX + 6 + (DW - 12) * i / (ANA_PTS - 1.f), y = yOfSpec(specHold[i]); if (!i) moveTo(x, y); else lineTo(x, y); }
            strokeColor(Color(0xf4, 0xe6, 0xa0, 0.6f)); strokeWidth(1); stroke();
        }
        // bands: full range (faint), live gain change (filled), edges, handles
        for (int b = 0; b < kMaxBands; ++b) {
            if (!used(b)) continue;
            const Color col = bandColor(b); const bool sel = b == selBand, byp = val[bp(b, kBBypass)] > 0.5f;
            const float x0 = bandX0(b), x1 = bandX1(b);
            beginPath(); rect(x0, DY + 1, x1 - x0, DH - 20); fillColor(withAlpha(col, sel ? 0.07f : (b == hoverBand ? 0.05f : 0.025f))); fill();
            curvePath(b, handleDb(b), true); fillColor(withAlpha(col, byp ? 0.03f : 0.09f)); fill();
            curvePath(b, handleDb(b), false); strokeColor(withAlpha(col, byp ? 0.2f : (sel ? 0.55f : 0.35f))); strokeWidth(1); stroke();
            if (!byp) { const float g = liveDb[b]; curvePath(b, g, true); fillColor(withAlpha(col, 0.32f)); fill(); curvePath(b, g, false); strokeColor(col); strokeWidth(sel ? 2.f : 1.5f); stroke(); }
            // edges
            for (int side = 0; side < 2; ++side) {
                if (side == 0 ? openLo(lo(b)) : openHi(hi(b))) continue;
                const float x = side ? x1 : x0; const bool hot = (hoverEdgeBand == b && hoverEdgeSide == side) || (dragKind == DragEdge && dragBand == b && dragSide == side);
                beginPath(); moveTo(x, DY + 4); lineTo(x, DY + DH - 20); strokeColor(withAlpha(col, hot ? 0.95f : (sel ? 0.5f : 0.25f))); strokeWidth(hot ? 2.f : 1.f); stroke();
                if (sel || hot) { beginPath(); roundedRect(x - 3.5f, GAIN_TOP + (GAIN_BOT - GAIN_TOP) * 0.5f - 14, 7, 28, 3); fillColor(hot ? col : C_PANEL_HI); fill(); strokeColor(col); strokeWidth(1); stroke(); }
            }
            // range handle
            const float hx = handleX(b), hy = yOfDb(handleDb(b)); const bool hh = hoverHandle == b || (dragKind == DragMove && dragBand == b);
            beginPath(); roundedRect(hx - 15, hy - 4, 30, 8, 4); fillColor(hh || sel ? col : withAlpha(col, 0.6f)); fill();
            if (std::fabs(gcLive[b]) > 0.3f && !byp) { beginPath(); circle(hx, hy, 9); strokeColor(withAlpha(gcLive[b] < 0 ? C_CUT : C_BOOST, 0.7f)); strokeWidth(1.5f); stroke(); }
            // label
            const float lw = x1 - x0;
            if (lw > 24) {
                char t[48]; snprintf(t, sizeof t, "%d", b + 1);
                fontSize(10); textLetterSpacing(0.5f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillColor(byp ? C_FAINT : col); text(x0 + 6, DY + 8, t, nullptr);
                std::string tag; if (val[bp(b, kBMode)] > 0.5f) tag = "EXP"; if (val[bp(b, kBStereo)] > 0.5f) tag += std::string(tag.empty() ? "" : " ") + (val[bp(b, kBStereo)] > 1.5f ? "S" : "M");
                if (val[bp(b, kBSidechain)] > 0.5f) tag += tag.empty() ? "SC" : " SC";
                if (byp) tag = "BYPASS"; else if (val[bp(b, kBSolo)] > 0.5f) tag += tag.empty() ? "SOLO" : " SOLO";
                if (!tag.empty() && lw > 60) { fontSize(7.5f); textLetterSpacing(1.2f); fillColor(withAlpha(col, 0.8f)); text(x0 + 18, DY + 9.5f, tag.c_str(), nullptr); }
            }
            if (hh) { char t[48]; snprintf(t, sizeof t, "RANGE %+.1f dB", val[bp(b, kBRange)]); fontSize(9); textLetterSpacing(0.6f); textAlign(ALIGN_CENTER | ALIGN_BOTTOM); fillColor(C_TEXT); text(hx, hy - (handleDb(b) >= 0 ? 8 : -20), t, nullptr); }
        }
        // total response
        if (numUsed() > 0) {
            beginPath(); for (int i = 0; i < CURVE_PTS; ++i) { const float x = DX + 6 + (DW - 12) * i / (CURVE_PTS - 1.f), y = yOfDb(total[i]); if (!i) moveTo(x, y); else lineTo(x, y); }
            strokeColor(withAlpha(C_TEXT, 0.85f)); strokeWidth(1.5f); stroke();
        }
        // custom detection range of the selected band
        if (selBand >= 0 && expert() && val[bp(selBand, kBDetCustom)] > 0.5f) {
            const int b = selBand; const Color col = bandColor(b);
            const float dl = val[bp(b, kBDetLo)], dh = val[bp(b, kBDetHi)];
            const float x0 = openLo(dl) ? DX + 4 : xOfHz(dl), x1 = openHi(dh) ? DX + DW - 4 : xOfHz(dh);
            beginPath(); roundedRect(x0, DET_Y - 3, x1 - x0, 6, 3); fillColor(withAlpha(col, hoverDet >= 0 || dragKind == DragDet ? 0.75f : 0.45f)); fill();
            for (int k = 0; k < 2; ++k) { beginPath(); roundedRect((k ? x1 : x0) - 3, DET_Y - 8, 6, 16, 2); fillColor(hoverDet == k ? C_TEXT : col); fill(); }
            fontSize(7.5f); textLetterSpacing(1.2f); textAlign(ALIGN_LEFT | ALIGN_BOTTOM); fillColor(col); text(x0 + 2, DET_Y - 6, "DETECTION", nullptr);
        }
        if (numUsed() == 0) { fontSize(12); textLetterSpacing(2.f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_DIM); text(DX + DW / 2, DY + DH / 2 - 30, "DOUBLE-CLICK TO CREATE A BAND", nullptr); }
        else if (numUsed() < kMaxBands && hoverBand < 0 && hoverEdgeBand < 0 && dragKind == DragNone && inDisplay(tipX, tipY)) {
            fontSize(8); textLetterSpacing(1); textAlign(ALIGN_LEFT | ALIGN_BOTTOM); fillColor(C_FAINT); text(DX + 10, DY + DH - 20, "DOUBLE-CLICK TO CREATE A BAND", nullptr); }
        resetScissor();
    }

    // ---- meters --------------------------------------------------------------------
    void drawMeters() {
        beginPath(); roundedRect(MX, MY, MW, MH, 6); fillColor(C_DISPLAY); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        const float top = MY + 40, bot = MY + MH - 22, h = bot - top;
        auto y = [&](float db) { return top + h * clampf((6.f - db) / 66.f, 0.f, 1.f); };
        auto bar = [&](float x, float lin, const Color& c) {
            beginPath(); rect(x, top, 8, h); fillColor(C_PANEL_HI); fill();
            const float db = gainToDb(lin); if (db > -60) { const float yy = y(db); beginPath(); rect(x, yy, 8, bot - yy); fillColor(c); fill(); }
            if (db > 0.f) { beginPath(); rect(x, top, 8, 3); fillColor(C_CUT); fill(); }
        };
        bar(MX + 8, mIn[0], Color(0x7a, 0x82, 0x7e)); bar(MX + 18, mIn[1], Color(0x7a, 0x82, 0x7e));
        bar(MX + 46, mOut[0], C_ACC); bar(MX + 56, mOut[1], C_ACC);
        fontSize(6.5f); textLetterSpacing(0.3f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_FAINT);
        for (float db = 0; db >= -54; db -= 6) { char b[8]; snprintf(b, sizeof b, "%.0f", db); text(MX + 36, y(db), b, nullptr); }
        fontSize(7.5f); textLetterSpacing(1.2f); text(MX + 17, MY + 12, "IN", nullptr); text(MX + 56, MY + 12, "OUT", nullptr);
        char b[16]; fontSize(8.5f); textLetterSpacing(0.2f);
        snprintf(b, sizeof b, "%.1f", pkIn); fillColor(pkIn > 0 ? C_CUT : C_DIM); text(MX + 17, MY + 27, pkIn > -100 ? b : "-inf", nullptr);
        snprintf(b, sizeof b, "%.1f", pkOut); fillColor(pkOut > 0 ? C_CUT : C_ACC); text(MX + 56, MY + 27, pkOut > -100 ? b : "-inf", nullptr);
        fontSize(7); fillColor(C_FAINT); text(MX + MW / 2, MY + MH - 10, "dBFS", nullptr);
    }

    // ---- band panel ------------------------------------------------------------
    void drawBandPanel() {
        panel(BX, BY, BW, BH);
        const int b = selBand;
        if (b < 0) {
            fontSize(11); textLetterSpacing(2.f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_DIM);
            text(BX + BW / 2, BY + BH / 2 - 10, numUsed() ? "CLICK A BAND TO EDIT IT" : "DOUBLE-CLICK THE DISPLAY TO CREATE A BAND", nullptr);
            fontSize(9); textLetterSpacing(0.6f); fillColor(C_FAINT); text(BX + BW / 2, BY + BH / 2 + 12, "Or load a preset from the menu at the top.", nullptr);
            return;
        }
        const Color col = bandColor(b);
        beginPath(); circle(BX + 22, BY + 24, 6); fillColor(col); fill();
        char t[64]; snprintf(t, sizeof t, "BAND %d", b + 1);
        fontSize(11); textLetterSpacing(2.2f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(C_TEXT); text(BX + 36, BY + 21, t, nullptr);
        const std::string r = (openLo(lo(b)) ? std::string("SHELF") : fmtHz(lo(b))) + " - " + (openHi(hi(b)) ? std::string("SHELF") : fmtHz(hi(b)));
        fontSize(7.5f); textLetterSpacing(1.f); fillColor(C_FAINT); text(BX + 36, BY + 34, r.c_str(), nullptr);
        // remove
        { const bool h = hoverRemove; beginPath(); roundedRect(REM_X, REM_Y, REM_W, REM_H, 5); fillColor(h ? C_PANEL_HI : C_PANEL); fill(); strokeColor(h ? C_DIM : C_LINE); strokeWidth(1); stroke();
          fontSize(8.5f); textLetterSpacing(1.4f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_DIM); text(REM_X + REM_W / 2, REM_Y + REM_H / 2 + 0.5f, "REMOVE", nullptr); }
        caption(BX + 516, BY + 190, "SLOPE");
        drawBandMeter(b);
        drawTransfer(b);
        beginPath(); moveTo(BX + 14, BY + 268); lineTo(BX + BW - 14, BY + 268); strokeColor(C_LINE); strokeWidth(1); stroke();
        if (expert()) {
            caption(BX + 168, BY + 287, "CHANNELS"); caption(BX + 352, BY + 287, "SIDECHAIN"); caption(BX + 504, BY + 287, "DETECTION");
            fontSize(7.5f); textLetterSpacing(1.6f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(C_FAINT); text(BX + 20, BY + 312, "EXPERT", nullptr);
        } else {
            fontSize(8.5f); textLetterSpacing(1.2f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_FAINT);
            text(BX + BW / 2, BY + 310, "EXPERT MODE (GLOBAL PANEL): MID/SIDE BANDS, EXTERNAL SIDECHAIN AND CUSTOM DETECTION RANGES", nullptr);
        }
    }
    // level the band hears (with the threshold) and the gain change it applies
    float bmY(float db) const { return BMY + BMH * clampf(-db / 60.f, 0.f, 1.f); }
    void drawBandMeter(int b) {
        const float x = BMX;
        beginPath(); rect(x, BMY, 12, BMH); fillColor(C_PANEL_HI); fill();
        const float lv = levelLive[b];
        if (lv > -60) { const float yy = bmY(lv); beginPath(); rect(x, yy, 12, BMY + BMH - yy); fillColor(withAlpha(bandColor(b), 0.75f)); fill(); }
        const float ty = bmY(val[bp(b, kBThreshold)]); const bool hot = hoverThr || dragKind == DragThr;
        beginPath(); moveTo(x - 4, ty); lineTo(x + 16, ty); strokeColor(hot ? C_TEXT : C_DIM); strokeWidth(hot ? 2.f : 1.5f); stroke();
        beginPath(); moveTo(x - 6, ty - 4); lineTo(x - 1, ty); lineTo(x - 6, ty + 4); closePath(); fillColor(hot ? C_TEXT : C_DIM); fill();
        // gain change, centred
        const float gx = x + 20, mid = BMY + BMH / 2, R = std::max(6.f, std::fabs(val[bp(b, kBRange)]));
        beginPath(); rect(gx, BMY, 12, BMH); fillColor(C_PANEL_HI); fill();
        const float g = val[bp(b, kBBypass)] > 0.5f ? 0.f : gcLive[b], gh = clampf(g / R, -1.f, 1.f) * BMH / 2;
        if (std::fabs(gh) > 0.5f) { beginPath(); rect(gx, gh > 0 ? mid - gh : mid, 12, std::fabs(gh)); fillColor(g < 0 ? C_CUT : C_BOOST); fill(); }
        beginPath(); moveTo(gx - 1, mid); lineTo(gx + 13, mid); strokeColor(C_FAINT); strokeWidth(1); stroke();
        fontSize(7); textLetterSpacing(1.f); textAlign(ALIGN_CENTER | ALIGN_TOP); fillColor(C_FAINT);
        text(x + 6, BMY + BMH + 5, "LVL", nullptr); text(gx + 6, BMY + BMH + 5, "GAIN", nullptr);
        char t[16]; snprintf(t, sizeof t, "%+.1f", g); fontSize(8); fillColor(std::fabs(g) > 0.05f ? (g < 0 ? C_CUT : C_BOOST) : C_DIM); textAlign(ALIGN_CENTER | ALIGN_BOTTOM); text(gx + 6, BMY - 4, t, nullptr);
    }
    float tfX(float db) const { return TX + TS * clampf((db + 60.f) / 60.f, 0.f, 1.f); }
    float tfY(float db) const { return TY + TS * (1.f - clampf((db + 60.f) / 60.f, 0.f, 1.f)); }
    void drawTransfer(int b) {
        beginPath(); roundedRect(TX, TY, TS, TS, 4); fillColor(C_DISPLAY); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        beginPath(); for (int k = 1; k < 6; ++k) { const float d = -60.f + k * 10.f; moveTo(tfX(d), TY + 2); lineTo(tfX(d), TY + TS - 2); moveTo(TX + 2, tfY(d)); lineTo(TX + TS - 2, tfY(d)); }
        strokeColor(Color(255, 255, 255, 7 / 255.f)); strokeWidth(1); stroke();
        beginPath(); moveTo(tfX(-60), tfY(-60)); lineTo(tfX(0), tfY(0)); strokeColor(C_FAINT); stroke();
        const float thr = val[bp(b, kBThreshold)], ratio = val[bp(b, kBRatio)], knee = val[bp(b, kBKnee)], range = val[bp(b, kBRange)];
        const int md = val[bp(b, kBMode)] > 0.5f ? kExpand : kCompress;
        scissor(TX + 1, TY + 1, TS - 2, TS - 2);
        const float tx = tfX(thr); const bool hot = hoverTf || dragKind == DragTf;
        for (float y = TY + 3; y < TY + TS; y += 8) { beginPath(); moveTo(tx, y); lineTo(tx, y + 4); strokeColor(withAlpha(C_TEXT, hot ? 0.8f : 0.3f)); strokeWidth(1); stroke(); }
        beginPath(); for (int i = 0; i <= 90; ++i) { const float in = -60.f + 60.f * i / 90.f, out = staticOut(in, thr, ratio, knee, md, range); if (!i) moveTo(tfX(in), tfY(out)); else lineTo(tfX(in), tfY(out)); }
        strokeColor(bandColor(b)); strokeWidth(2.f); stroke();
        const float lv = levelLive[b];
        if (lv > -60) { const float out = lv + (val[bp(b, kBBypass)] > 0.5f ? 0.f : gcLive[b]); beginPath(); circle(tfX(lv), tfY(out), 3.5f); fillColor(C_TEXT); fill(); }
        resetScissor();
        fontSize(7); textLetterSpacing(1.2f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillColor(C_FAINT); text(TX + 5, TY + 5, "OUT", nullptr);
        textAlign(ALIGN_RIGHT | ALIGN_BOTTOM); text(TX + TS - 5, TY + TS - 4, "IN", nullptr);
        static const char* const kind[4] = { "DOWNWARD COMPRESSION", "UPWARD COMPRESSION", "DOWNWARD EXPANSION", "UPWARD EXPANSION" };
        fontSize(7.5f); textLetterSpacing(1.2f); textAlign(ALIGN_CENTER | ALIGN_TOP); fillColor(std::fabs(range) < 0.05f ? C_FAINT : C_DIM);
        const int ki = (md == kExpand ? 2 : 0) + (range > 0 ? 1 : 0);
        const char* label = kind[ki];
        if (ratioIsInf(ratio) && ki == 0) label = "LIMITING (INF:1)";        // the two classic extremes get their usual names
        if (ratioIsInf(ratio) && ki == 2) label = "GATING (INF:1)";
        text(TX + TS / 2, TY + TS + 6, std::fabs(range) < 0.05f ? "RANGE 0: NO GAIN CHANGE" : label, nullptr);
    }
    void drawGlobalPanel() {
        panel(GX, GY, GW, GH);
        caption(GX + 170, GY + 22, "PROCESSING MODE"); caption(GX + 96, GY + 84, "OVERSAMPLING"); caption(GX + 120, GY + 260, "ANALYZER");
        fontSize(7.5f); textLetterSpacing(1.8f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(C_FAINT); text(GX + 20, GY + 326, "DISPLAY", nullptr);
        if (mode() == kLinearPhase) { fontSize(7); textLetterSpacing(1.f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_FAINT); text(GX + 96, GY + 126, "N/A IN LINEAR PHASE", nullptr); }
        beginPath(); moveTo(GX + 14, GY + 140); lineTo(GX + GW - 14, GY + 140); moveTo(GX + 14, GY + 244); lineTo(GX + GW - 14, GY + 244); strokeColor(C_LINE); strokeWidth(1); stroke();
    }

    // ---- controls ------------------------------------------------------------------
    void drawKnobBody(float x, float y, float r, float n, bool bipolar, bool hot, bool dim, const Color& col) {
        const float a0 = 0.75f * 3.14159265f, a1 = 2.25f * 3.14159265f, ang = a0 + (a1 - a0) * n;
        const float tw = r > 26 ? 5.f : (r > 18 ? 3.5f : 2.5f);
        beginPath(); arc(x, y, r, a0, a1, NanoVG::CW); strokeColor(C_LINE); strokeWidth(tw); lineCap(ROUND); stroke();
        const float from = bipolar ? (a0 + a1) / 2 : a0;
        if (std::fabs(ang - from) > 0.001f) { beginPath(); arc(x, y, r, std::min(from, ang), std::max(from, ang), NanoVG::CW); strokeColor(dim ? C_FAINT : col); stroke(); }
        const float cr = r - (r > 26 ? 9 : (r > 18 ? 7 : 5));
        beginPath(); circle(x, y, cr);
        fillPaint(radialGradient(x - cr * 0.3f, y - cr * 0.4f, 0, cr * 1.4f, hot ? Color(0x38, 0x40, 0x3d) : C_PANEL_HI, Color(0x12, 0x14, 0x13)));
        fill(); strokeColor(Color(255, 255, 255, 18 / 255.f)); strokeWidth(1); stroke();
        beginPath(); moveTo(x + std::cos(ang) * cr * 0.3f, y + std::sin(ang) * cr * 0.3f); lineTo(x + std::cos(ang) * cr * 0.9f, y + std::sin(ang) * cr * 0.9f);
        strokeColor(dim ? C_DIM : C_TEXT); strokeWidth(r > 12 ? 2.f : 1.5f); lineCap(ROUND); stroke();
    }
    void drawCtl(const Ctl& c, bool hot) {
        const uint32_t p = effParam(c); const bool dim = dimmed(c); char buf[48];
        const Color col = c.group == GGlobal ? C_ACC : bandColor(selBand);
        switch (c.type) {
        case Knob: {
            const bool bip = (paramDef(p).hints & kHintBipolar) != 0;
            drawKnobBody(c.x, c.y, c.r, toNorm(p, val[p]), bip, hot, dim, (p == bp(selBand, kBRange) && val[p] > 0) ? C_BOOST : ((p == bp(selBand, kBRange) && val[p] < 0) ? C_CUT : col));
            fontSize(c.r > 26 ? 9.f : 8.f); textLetterSpacing(c.r > 26 ? 1.8f : 1.4f); textAlign(ALIGN_CENTER | ALIGN_TOP); fillColor(dim ? C_FAINT : C_DIM);
            text(c.x, c.y + c.r + 6, c.label, nullptr);
            fmt(p, buf, sizeof buf); fontSize(c.r > 26 ? 11.5f : 10.f); textLetterSpacing(0.4f); fillColor(dim ? C_FAINT : (hot ? col : Color(0xd2, 0xd8, 0xd4)));
            text(c.x, c.y + c.r + (c.r > 26 ? 19 : 17), buf, nullptr);
            break; }
        case Toggle: {
            const bool on = val[p] > 0.5f;
            const float w = 30, h = 16, x = c.x - w / 2, y = c.y - h / 2;
            beginPath(); roundedRect(x, y, w, h, h / 2); fillColor(on ? withAlpha(col, 0.25f) : C_PANEL_HI); fill(); strokeColor(on ? col : C_LINE); strokeWidth(1); stroke();
            beginPath(); circle(on ? x + w - h / 2 : x + h / 2, c.y, 5); fillColor(on ? col : C_DIM); fill();
            fontSize(8.5f); textLetterSpacing(1.5f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(on ? C_TEXT : C_DIM); text(c.x + w / 2 + 7, c.y + 0.5f, c.label, nullptr);
            break; }
        case Segment: {
            int n = 0; const char* const* names = segLabels(c.field, c.group == GGlobal, n);
            const int cur = (int)std::round(val[p]); const float sw = c.r / n, x0 = c.x - c.r / 2;
            for (int i = 0; i < n; ++i) {
                const float x = x0 + sw * i;
                beginPath(); roundedRect(x + 1.5f, c.y - SEG_H / 2, sw - 3, SEG_H, 4); fillColor(i == cur ? (dim ? C_FAINT : col) : C_PANEL_HI); fill();
                fontSize(8.5f); textLetterSpacing(1.2f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(i == cur ? C_BG : (dim ? C_FAINT : C_DIM)); text(x + sw / 2, c.y + 0.5f, names[i], nullptr);
            }
            break; }
        case Stepper: {
            const float x = c.x - c.r / 2, y = c.y - 11, w = c.r, h = 22;
            beginPath(); roundedRect(x, y, w, h, 4); fillColor(C_PANEL_HI); fill(); strokeColor(hot ? C_DIM : C_LINE); strokeWidth(1); stroke();
            for (int side = 0; side < 2; ++side) { const float ax = side == 0 ? x + 9 : x + w - 9, d = side == 0 ? -1.f : 1.f;
                beginPath(); moveTo(ax - 2.5f * d, c.y - 4); lineTo(ax + 2.5f * d, c.y); lineTo(ax - 2.5f * d, c.y + 4); closePath(); fillColor(C_DIM); fill(); }
            fmt(p, buf, sizeof buf); std::string up(buf); for (char& ch : up) ch = (char)std::toupper((unsigned char)ch);
            fontSize(8.5f); textLetterSpacing(0.8f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(col); text(c.x, c.y + 0.5f, up.c_str(), nullptr);
            if (mode() == kMinimumPhase && order(selBand) != slopeOrder(val[p])) { char b2[40]; snprintf(b2, sizeof b2, "CROSSOVER %d DB/OCT", order(selBand) * 6); fontSize(7); textLetterSpacing(1); fillColor(C_FAINT); text(c.x, c.y + 20, b2, nullptr); }
            break; }
        }
        if (learnMode && c.type != Stepper) {
            const int cc = paramCC[p]; const bool waiting = learnWaiting == (int)p;
            if (cc >= 0 || waiting) {
                const float by = c.type == Knob ? c.y - c.r - 12 : c.y - SEG_H / 2 - 8;
                snprintf(buf, sizeof buf, waiting ? "?" : "CC%d", cc);
                fontSize(8); textLetterSpacing(0.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                const float bw = waiting ? 14 : 32;
                beginPath(); roundedRect(c.x - bw / 2, by - 6, bw, 12, 3); fillColor(waiting ? C_ACC : C_PANEL_HI); fill(); strokeColor(C_ACC); strokeWidth(1); stroke();
                fillColor(waiting ? C_BG : C_ACC); text(c.x, by + 0.5f, buf, nullptr);
            }
        }
    }
    void drawStatus() {
        std::string msg;
        if (statusFlash > 0) msg = statusText;
        else if (learnMode) msg = learnWaiting >= 0 ? std::string("Move a MIDI controller to assign: ") + paramDef(learnWaiting).name : "MIDI Learn: click a control, then move a controller. Right-click unassigns.";
        else if (dragKind == DragMove && dragBand >= 0) { char b[96]; snprintf(b, sizeof b, "Band %d: %s - %s, range %+.1f dB", dragBand + 1, openLo(lo(dragBand)) ? "shelf" : fmtHz(lo(dragBand)).c_str(), openHi(hi(dragBand)) ? "shelf" : fmtHz(hi(dragBand)).c_str(), val[bp(dragBand, kBRange)]); msg = b; }
        else if (dragKind == DragEdge && dragBand >= 0) { const float f = dragSide ? hi(dragBand) : lo(dragBand); msg = std::string("Band ") + std::to_string(dragBand + 1) + (dragSide ? " top: " : " bottom: ") + ((dragSide ? openHi(f) : openLo(f)) ? std::string("shelf") : fmtHz(f)); }
        if (!msg.empty()) { fontSize(10); textLetterSpacing(0.6f); textAlign(ALIGN_RIGHT | ALIGN_MIDDLE); fillColor(C_ACC); text(DX + DW - 40, DY + 16, msg.c_str(), nullptr); }
        if (learnMode && learnWaiting < 0) { fontSize(9); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(hoverClear ? C_TEXT : C_DIM); text(DX + 14, DY + DH - 40, "CLEAR ALL ASSIGNMENTS", nullptr); }
        if (statusFlash > 0) { --statusFlash; repaint(); }
    }
    void flash(const std::string& s) { statusText = s; statusFlash = 90; repaint(); }
    void drawEditBox() {
        const Ctl& c = controls()[editing];
        const float w = 90, h = 22, x = c.x - w / 2, y = c.y + c.r + 4;
        beginPath(); roundedRect(x, y, w, h, 4); fillColor(C_DISPLAY); fill(); strokeColor(C_ACC); strokeWidth(1); stroke();
        fontSize(12); textLetterSpacing(0.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_TEXT); text(c.x, y + h / 2 + 0.5f, (editBuf + "|").c_str(), nullptr);
    }

    // ---- presets ------------------------------------------------------------------
    bool matchesValues(const float* v) const { for (uint32_t i = 0; i < kOutLearn; ++i) if (!isView(i) && std::fabs(val[i] - v[i]) > 1e-4f * std::max(1.f, std::fabs(v[i]))) return false; return true; }
    void applyValues(const float* v) { beginGesture(); applyAll(v); endGesture(); }
    void applyFactory(int idx) { idx = (idx % (int)kNumPresets + (int)kNumPresets) % (int)kNumPresets; applyValues(presets()[idx].v); curFactory = idx; curUser.clear(); selBand = firstUsed(); repaint(); }
    void applyUser(const std::string& name) { float v[kParamCount]; std::copy(defv, defv + kParamCount, v); if (!loadUserPreset(name, v)) return; applyValues(v); std::copy(v, v + kParamCount, userVals); curUser = name; curFactory = -1; selBand = firstUsed(); repaint(); }
    void programLoaded(uint32_t index) override { curFactory = (int)index; curUser.clear(); selBand = firstUsed(); repaint(); }
    void step(int d) {
        const int nf = (int)kNumPresets, nu = (int)userPresets.size(), total_ = nf + nu;
        int cur = curFactory; if (!curUser.empty()) for (int i = 0; i < nu; ++i) if (userPresets[i] == curUser) cur = nf + i;
        if (cur < 0) cur = 0;
        const int next = ((cur + d) % total_ + total_) % total_;
        if (next < nf) applyFactory(next); else applyUser(userPresets[next - nf]);
    }
    void drawPresetBar() {
        const float x = PS_X, y = PS_Y, w = PS_W, h = PS_H, a = PS_ARROW;
        beginPath(); roundedRect(x, y, w, h, 5); fillColor(C_PANEL); fill(); strokeColor(menuOpen ? C_ACC : C_LINE); strokeWidth(1); stroke();
        beginPath(); moveTo(x + a, y + 5); lineTo(x + a, y + h - 5); moveTo(x + w - a, y + 5); lineTo(x + w - a, y + h - 5); strokeColor(C_LINE); stroke();
        for (int side = 0; side < 2; ++side) { const bool hot = presetHover == (side == 0 ? 0 : 2);
            const float cx = side == 0 ? x + a / 2 : x + w - a / 2, cy = y + h / 2, d = side == 0 ? -1.f : 1.f;
            beginPath(); moveTo(cx - 3 * d, cy - 5); lineTo(cx + 3 * d, cy); lineTo(cx - 3 * d, cy + 5); closePath(); fillColor(hot ? C_ACC : C_DIM); fill(); }
        std::string name; bool modified = false;
        if (!curUser.empty()) { name = curUser; modified = !matchesValues(userVals); }
        else if (curFactory >= 0) { name = presets()[curFactory].name; modified = !matchesValues(presets()[curFactory].v); }
        else name = "Custom";
        if (modified) name += " *";
        fontSize(11.5f); textLetterSpacing(0.6f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(presetHover == 1 || menuOpen ? C_TEXT : Color(0xd0, 0xd8, 0xd3)); text(x + w / 2, y + h / 2 + 0.5f, name.c_str(), nullptr);
    }
    // preset browser: categories, user presets and favourites in columns; typing searches names and categories.
    // kind: 0 factory, 1 user, 2 save button, 3 search field, 4 favourite (idx into favs)
    struct MRect { float x, y, w, h; int kind; int idx; };
    static std::string lower(const std::string& s) { std::string r(s); for (char& c : r) c = (char)std::tolower((unsigned char)c); return r; }
    std::string favKey(int kind, int idx) const { return kind == 0 ? std::string("f:") + presets()[idx].name : (kind == 1 ? "u:" + userPresets[idx] : (idx < (int)favs.size() ? favs[idx] : std::string())); }
    bool isFav(const std::string& k) const { return std::find(favs.begin(), favs.end(), k) != favs.end(); }
    void toggleFav(const std::string& k) {
        if (k.empty()) return;
        auto it = std::find(favs.begin(), favs.end(), k); if (it != favs.end()) favs.erase(it); else favs.push_back(k);
        saveFavourites(favs); repaint();
    }
    static constexpr int kMenuCols = kNumCategories + 2, kMenuRows = 16;
    void menuLayout(float& mx, float& my, float& mw, float& mh, std::vector<MRect>& items) const {
        mw = PM_COLW * kMenuCols + 2 * PM_PAD; mx = std::min(W - mw - 8, std::max(8.f, PS_X + PS_W / 2 - mw / 2)); my = PS_Y + PS_H + 4;
        items.clear();
        const float top = my + PM_PAD + PM_ROW + 6 + PM_HEAD;
        int maxRows = 1;
        items.push_back({ mx + PM_PAD + PM_COLW, my + PM_PAD, 300, PM_ROW, 3, 0 });
        items.push_back({ mx + mw - PM_PAD - 150, my + PM_PAD, 150, PM_ROW, 2, 0 });
        if (!query.empty()) {
            const std::string q = lower(query); int k = 0;
            auto place = [&](int kind, int idx) { const int col = k / kMenuRows, row = k % kMenuRows; if (col >= kMenuCols) return; items.push_back({ mx + PM_PAD + col * PM_COLW, top + row * PM_ROW, PM_COLW - 4, PM_ROW, kind, idx }); maxRows = std::max(maxRows, row + 1); ++k; };
            for (uint32_t i = 0; i < kNumPresets; ++i) if (lower(presets()[i].name).find(q) != std::string::npos || lower(presets()[i].category).find(q) != std::string::npos) place(0, (int)i);
            for (int u = 0; u < (int)userPresets.size(); ++u) if (lower(userPresets[u]).find(q) != std::string::npos || std::string("user").find(q) != std::string::npos) place(1, u);
        } else {
            int rows[kNumCategories] = {};
            for (uint32_t i = 0; i < kNumPresets; ++i) {
                const char* cat = presets()[i].category;
                if (std::strcmp(cat, "Init") == 0) { items.push_back({ mx + PM_PAD, my + PM_PAD, PM_COLW - 4, PM_ROW, 0, (int)i }); continue; }
                for (int c = 0; c < kNumCategories; ++c)
                    if (std::strcmp(cat, kCategories[c]) == 0) { items.push_back({ mx + PM_PAD + c * PM_COLW, top + rows[c] * PM_ROW, PM_COLW - 4, PM_ROW, 0, (int)i }); maxRows = std::max(maxRows, ++rows[c]); }
            }
            for (int u = 0; u < (int)userPresets.size() && u < kMenuRows; ++u) items.push_back({ mx + PM_PAD + kNumCategories * PM_COLW, top + u * PM_ROW, PM_COLW - 4, PM_ROW, 1, u });
            maxRows = std::max(maxRows, std::min(kMenuRows, (int)userPresets.size()));
            for (int f = 0; f < (int)favs.size() && f < kMenuRows; ++f) items.push_back({ mx + PM_PAD + (kNumCategories + 1) * PM_COLW, top + f * PM_ROW, PM_COLW - 4, PM_ROW, 4, f });
            maxRows = std::max(maxRows, std::min(kMenuRows, (int)favs.size()));
        }
        mh = PM_PAD + PM_ROW + 6 + PM_HEAD + maxRows * PM_ROW + PM_PAD;
    }
    std::string itemLabel(const MRect& r) const {
        if (r.kind == 0) return presets()[r.idx].name;
        if (r.kind == 1) return userPresets[r.idx];
        if (r.kind == 4 && r.idx < (int)favs.size()) return favs[r.idx].substr(2);
        return "";
    }
    void drawPresetMenu() {
        float mx, my, mw, mh; std::vector<MRect> items; menuLayout(mx, my, mw, mh, items);
        beginPath(); rect(mx - 12, my - 4, mw + 24, mh + 20); fillPaint(boxGradient(mx, my + 2, mw, mh, 6, 14, Color(0, 0, 0, 0.6f), Color(0, 0, 0, 0.f))); fill();
        beginPath(); roundedRect(mx, my, mw, mh, 6); fillColor(C_PANEL_HI); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        beginPath(); moveTo(mx + PM_PAD, my + PM_PAD + PM_ROW + 3); lineTo(mx + mw - PM_PAD, my + PM_PAD + PM_ROW + 3); strokeColor(C_LINE); stroke();
        fontSize(8.5f); textLetterSpacing(1.8f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        const float hy = my + PM_PAD + PM_ROW + 6 + PM_HEAD / 2;
        if (query.empty()) {
            for (int c = 0; c < kMenuCols; ++c) {
                std::string up = c < kNumCategories ? kCategories[c] : (c == kNumCategories ? "User" : "Favourites"); for (char& ch : up) ch = (char)std::toupper((unsigned char)ch);
                fillColor(c < kNumCategories ? C_ACC : C_FAV); text(mx + PM_PAD + c * PM_COLW + 12, hy, up.c_str(), nullptr);
            }
            fontSize(10); textLetterSpacing(0.4f); fillColor(C_FAINT);
            if (userPresets.empty()) text(mx + PM_PAD + kNumCategories * PM_COLW + 12, hy + PM_HEAD / 2 + PM_ROW / 2, "(none saved yet)", nullptr);
            if (favs.empty()) text(mx + PM_PAD + (kNumCategories + 1) * PM_COLW + 12, hy + PM_HEAD / 2 + PM_ROW / 2, "(click a star)", nullptr);
        } else {
            int found = 0; for (const MRect& r : items) if (r.kind == 0 || r.kind == 1) ++found;
            char b[64]; snprintf(b, sizeof b, found ? "%d MATCHING PRESET%s" : "NO PRESET MATCHES", found, found == 1 ? "" : "S"); fillColor(C_ACC); text(mx + PM_PAD + 12, hy, b, nullptr);
        }
        for (int i = 0; i < (int)items.size(); ++i) {
            const MRect& r = items[i]; const bool hot = i == menuHover;
            if (r.kind == 2) {
                beginPath(); roundedRect(r.x, r.y + 1, r.w, r.h - 2, 4); fillColor(hot ? withAlpha(C_ACC, 0.25f) : C_PANEL); fill(); strokeColor(C_ACC); strokeWidth(1); stroke();
                fontSize(9.5f); textLetterSpacing(1.6f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_TEXT); text(r.x + r.w / 2, r.y + r.h / 2 + 0.5f, "SAVE CURRENT AS...", nullptr);
                continue;
            }
            if (r.kind == 3) {             // search field
                beginPath(); roundedRect(r.x, r.y + 1, r.w, r.h - 2, 4); fillColor(C_DISPLAY); fill(); strokeColor(query.empty() ? C_LINE : C_ACC); strokeWidth(1); stroke();
                beginPath(); circle(r.x + 12, r.y + r.h / 2, 4); moveTo(r.x + 15, r.y + r.h / 2 + 3); lineTo(r.x + 18, r.y + r.h / 2 + 6); strokeColor(C_DIM); strokeWidth(1.3f); stroke();
                fontSize(11); textLetterSpacing(0.3f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(query.empty() ? C_FAINT : C_TEXT);
                text(r.x + 26, r.y + r.h / 2 + 0.5f, query.empty() ? "Type to search names and categories" : (query + "|").c_str(), nullptr);
                continue;
            }
            const std::string key = favKey(r.kind, r.idx);
            const bool isCur = r.kind == 0 ? (curUser.empty() && r.idx == curFactory) : (r.kind == 1 ? curUser == userPresets[r.idx] : false);
            const bool del = r.kind == 1 && pendingDelete == r.idx;
            if (hot || del) { beginPath(); roundedRect(r.x, r.y, r.w, r.h, 4); fillColor(del ? Color(0xd0, 0x50, 0x40, 0.35f) : withAlpha(C_ACC, 0.18f)); fill(); }
            if (isCur) { beginPath(); circle(r.x + 6, r.y + r.h / 2, 2.5f); fillColor(C_ACC); fill(); }
            fontSize(11); textLetterSpacing(0.3f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(hot || isCur || del ? C_TEXT : C_DIM);
            const std::string label = del ? "Delete? click again" : itemLabel(r);
            text(r.x + 12, r.y + r.h / 2 + 0.5f, label.c_str(), nullptr);
            // the favourite star
            const bool fav = isFav(key);
            if (fav || hot) drawStar(r.x + r.w - 10, r.y + r.h / 2, 5.f, fav);
        }
    }
    void drawStar(float cx, float cy, float rr, bool filled) {
        beginPath();
        for (int k = 0; k < 10; ++k) { const float a = -3.14159265f / 2 + k * 3.14159265f / 5, q = k % 2 ? rr * 0.45f : rr; if (!k) moveTo(cx + q * std::cos(a), cy + q * std::sin(a)); else lineTo(cx + q * std::cos(a), cy + q * std::sin(a)); }
        closePath();
        if (filled) { fillColor(C_FAV); fill(); } else { strokeColor(C_DIM); strokeWidth(1); stroke(); }
    }
    int menuHit(float x, float y) const { float mx, my, mw, mh; std::vector<MRect> it; menuLayout(mx, my, mw, mh, it);
        for (int i = 0; i < (int)it.size(); ++i) if (x >= it[i].x && x < it[i].x + it[i].w && y >= it[i].y && y < it[i].y + it[i].h) return i;
        return -1; }
    MRect menuItem(int i) const { float mx, my, mw, mh; std::vector<MRect> it; menuLayout(mx, my, mw, mh, it); return it[i]; }
    void openMenu() { userPresets = listUserPresets(); favs = loadFavourites(); query.clear(); menuOpen = true; menuHover = -1; pendingDelete = -1; repaint(); }
    void applyFavourite(int idx) {
        if (idx >= (int)favs.size()) return;
        const std::string& k = favs[idx];
        if (k.compare(0, 2, "f:") == 0) { for (uint32_t i = 0; i < kNumPresets; ++i) if (k.substr(2) == presets()[i].name) { applyFactory((int)i); return; } }
        else applyUser(k.substr(2));
    }
    void drawSaveBox() {
        const float w = 360, h = 96, x = W / 2 - w / 2, y = 150;
        beginPath(); rect(0, 0, W, H); fillColor(Color(0, 0, 0, 0.45f)); fill();
        beginPath(); roundedRect(x, y, w, h, 8); fillColor(C_PANEL_HI); fill(); strokeColor(C_ACC); strokeWidth(1); stroke();
        fontSize(9.5f); textLetterSpacing(2.f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillColor(C_DIM); text(x + 16, y + 14, "SAVE PRESET AS", nullptr);
        beginPath(); roundedRect(x + 16, y + 36, w - 32, 26, 4); fillColor(C_DISPLAY); fill(); strokeColor(C_LINE); stroke();
        fontSize(13); textLetterSpacing(0.4f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(C_TEXT); text(x + 26, y + 49.5f, (saveName + "|").c_str(), nullptr);
        fontSize(8.5f); textLetterSpacing(1.2f); fillColor(C_FAINT); textAlign(ALIGN_RIGHT | ALIGN_TOP);
        text(x + w - 16, y + 72, saveError ? "COULD NOT SAVE - CHECK THE NAME" : "ENTER TO SAVE   ·   ESC TO CANCEL", nullptr);
    }

    // ---- band create / remove / move ----------------------------------------------------
    void createBandAt(float hz) {
        int slot = -1; for (int b = 0; b < kMaxBands; ++b) if (!used(b)) { slot = b; break; }
        if (slot < 0) { flash("Six bands is the maximum"); return; }
        float l = kFreqMin, r = kFreqMax;
        for (int k = 0; k < kMaxBands; ++k) { if (!used(k)) continue;
            if (hz >= lo(k) && hz <= hi(k)) return;
            if (hi(k) <= hz) l = std::max(l, hi(k));
            if (lo(k) >= hz) r = std::min(r, lo(k)); }
        float a = std::max(l, hz / 1.4142f), z = std::min(r, hz * 1.4142f);
        if (z / a < 1.4142f) { a = std::max(l, z / 1.4142f); z = std::min(r, a * 1.4142f); }
        if (z / a < 1.25f) { flash("Not enough room for a band here"); return; }
        if (a < kFreqMin * 1.3f) a = kFreqMin;                     // right at the bottom: make it a shelf
        if (z > 24000.f) z = kFreqMax;
        beginGesture();
        for (int f = 0; f < kBandFields; ++f) commit(bp(slot, f), defv[bp(slot, f)]);
        commit(bp(slot, kBLo), a); commit(bp(slot, kBHi), z); commit(bp(slot, kBDetLo), a); commit(bp(slot, kBDetHi), z);
        commit(bp(slot, kBUsed), 1);
        endGesture(); selBand = slot; flash("Band " + std::to_string(slot + 1) + " created");
    }
    void removeBand(int b) {
        if (!used(b)) return;
        beginGesture(); commit(bp(b, kBUsed), 0); for (int f = 0; f < kBandFields; ++f) commit(bp(b, f), defv[bp(b, f)]); endGesture();
        selBand = firstUsed(); flash("Band removed");
    }
    void moveBand(float x, float y, bool fine) {
        const int b = dragBand; const float k = fine ? 0.2f : 1.f;
        const float oct = (x - dragX0) / (DW - 12) * std::log(kFreqMax / kFreqMin) * k;
        const bool ol = openLo(dragLo0), oh = openHi(dragHi0);
        if (!(ol && oh)) {
            float f = std::exp(oct);
            const float lmin = leftLimit(b), rmax = rightLimit(b);
            if (!ol) f = std::max(f, lmin / dragLo0); else f = std::max(f, (dragLo0 * 1.3f) / dragHi0);
            if (!oh) f = std::min(f, rmax / dragHi0); else f = std::min(f, rmax / (dragLo0 * 1.3f));
            if (!ol) setValue(bp(b, kBLo), dragLo0 * f);
            if (!oh) setValue(bp(b, kBHi), dragHi0 * f);
            if (val[bp(b, kBDetCustom)] < 0.5f) { setValue(bp(b, kBDetLo), lo(b)); setValue(bp(b, kBDetHi), hi(b)); }
        }
        const float r = clampf(dragRange0 - (y - dragY0) * dbPerPx() * k, -48.f, 48.f);
        setValue(bp(b, kBRange), std::round(r * 10.f) / 10.f);
    }
    void moveEdge(float x) {
        const int b = dragBand; float f = hzOfX(x);
        if (dragSide == 0) {
            const float lim = dragNb >= 0 ? lo(dragNb) * 1.12f : leftLimit(b, dragNb), top = hi(b) / 1.12f;
            if (dragNb < 0 && std::fabs(x - xOfHz(lim)) < 6 && lim > kFreqMin) f = lim;               // snap to the neighbour
            if (x < DX + 10 && dragNb < 0 && lim <= kFreqMin) f = kFreqMin;                             // all the way down: shelf
            f = clampf(f, std::max(lim, kFreqMin), top);
            setValue(bp(b, kBLo), f); if (dragNb >= 0) setValue(bp(dragNb, kBHi), f);
        } else {
            const float lim = dragNb >= 0 ? hi(dragNb) / 1.12f : rightLimit(b, dragNb), bot = lo(b) * 1.12f;
            if (dragNb < 0 && std::fabs(x - xOfHz(lim)) < 6 && lim < kFreqMax) f = lim;
            if (x > DX + DW - 10 && dragNb < 0 && lim >= kFreqMax) f = kFreqMax;
            f = clampf(f, bot, std::min(lim, kFreqMax));
            setValue(bp(b, kBHi), f); if (dragNb >= 0) setValue(bp(dragNb, kBLo), f);
        }
        for (int k : { b, dragNb }) if (k >= 0 && val[bp(k, kBDetCustom)] < 0.5f) { setValue(bp(k, kBDetLo), lo(k)); setValue(bp(k, kBDetHi), hi(k)); }
    }
    void moveDet(float x) {
        const int b = selBand;
        if (dragSide == 0) setValue(bp(b, kBDetLo), clampf(x < DX + 10 ? kFreqMin : hzOfX(x), kFreqMin, val[bp(b, kBDetHi)] / 1.12f));
        else if (dragSide == 1) setValue(bp(b, kBDetHi), clampf(x > DX + DW - 10 ? kFreqMax : hzOfX(x), val[bp(b, kBDetLo)] * 1.12f, kFreqMax));
        else {
            const float oct = (x - dragX0) / (DW - 12) * std::log(kFreqMax / kFreqMin);
            float f = std::exp(oct); f = clampf(f, kFreqMin / dragLo0, kFreqMax / dragHi0);
            setValue(bp(b, kBDetLo), dragLo0 * f); setValue(bp(b, kBDetHi), dragHi0 * f);
        }
    }

    // ---- hit testing ----------------------------------------------------------------
    static bool inRect(float x, float y, float rx, float ry, float rw, float rh) { return x >= rx && x < rx + rw && y >= ry && y < ry + rh; }
    int hit(float x, float y, int* seg = nullptr) const {
        const std::vector<Ctl>& cs = controls();
        for (int i = 0; i < (int)cs.size(); ++i) {
            const Ctl& c = cs[i]; if (!visible(c)) continue;
            switch (c.type) {
            case Knob: { const float rr = c.r + 6; if ((x - c.x) * (x - c.x) + (y - c.y) * (y - c.y) <= rr * rr) return i; break; }
            case Toggle: if (x >= c.x - 18 && x <= c.x + 70 && std::fabs(y - c.y) <= 11) return i; break;
            case Segment: { int n = 0; segLabels(c.field, c.group == GGlobal, n); const float x0 = c.x - c.r / 2;
                if (x >= x0 && x < x0 + c.r && std::fabs(y - c.y) <= SEG_H / 2 + 1) { if (seg) *seg = (int)((x - x0) / (c.r / n)); return i; } break; }
            case Stepper: if (std::fabs(x - c.x) <= c.r / 2 && std::fabs(y - c.y) <= 11) { if (seg) *seg = x < c.x - c.r / 4 ? -1 : (x > c.x + c.r / 4 ? 1 : 1); return i; } break;
            }
        }
        return -1;
    }
    int headerHit(float x, float y) const {
        if (y < PS_Y || y > PS_Y + PS_H) return 0;
        if (x >= LEARN_X && x <= LEARN_X + LEARN_W) return 1;
        if (x >= MIDI_X && x <= MIDI_X + MIDI_W) return 2;
        if (x >= UNDO_X && x <= UNDO_X + UR_W) return 3;
        if (x >= REDO_X && x <= REDO_X + UR_W) return 4;
        if (x >= HELP_X && x <= HELP_X + HELP_W) return 5;
        if (x >= A_X && x < A_X + AB_W) return 6;
        if (x >= A_X + AB_W && x < A_X + 2 * AB_W) return 7;
        if (x >= COPY_X && x <= COPY_X + COPY_W) return 8;
        return 0;
    }
    int presetBarHit(float x, float y) const { if (y < PS_Y || y > PS_Y + PS_H || x < PS_X || x > PS_X + PS_W) return -1; return x < PS_X + PS_ARROW ? 0 : (x > PS_X + PS_W - PS_ARROW ? 2 : 1); }
    bool inDisplay(float x, float y) const { return inRect(x, y, DX + 2, DY + 2, DW - 4, DH - 4); }
    int bandAt(float x, float y) const {
        if (!inDisplay(x, y) || y > DY + DH - 20) return -1;
        if (used(selBand) && x >= bandX0(selBand) && x <= bandX1(selBand)) return selBand;
        for (int b = 0; b < kMaxBands; ++b) if (used(b) && x >= bandX0(b) && x <= bandX1(b)) return b;
        return -1;
    }
    bool edgeHit(float x, float y, int& band, int& side) const {
        if (!inDisplay(x, y) || y > DY + DH - 20) return false;
        float best = 6; band = -1;
        for (int pass = 0; pass < 2; ++pass) for (int b = 0; b < kMaxBands; ++b) {
            if (!used(b) || (pass == 0) != (b == selBand)) continue;
            if (!openLo(lo(b)) && std::fabs(x - bandX0(b)) < best) { best = std::fabs(x - bandX0(b)); band = b; side = 0; }
            if (!openHi(hi(b)) && std::fabs(x - bandX1(b)) < best) { best = std::fabs(x - bandX1(b)); band = b; side = 1; }
            if (pass == 0 && band >= 0) return true;
        }
        return band >= 0;
    }
    int handleHit(float x, float y) const { for (int b = 0; b < kMaxBands; ++b) if (used(b) && std::fabs(x - handleX(b)) <= 17 && std::fabs(y - yOfDb(handleDb(b))) <= 7) return b; return -1; }
    int detHit(float x, float y) const {        // 0 low end, 1 high end, 2 body
        if (selBand < 0 || !expert() || val[bp(selBand, kBDetCustom)] < 0.5f || std::fabs(y - DET_Y) > 9) return -1;
        const float dl = val[bp(selBand, kBDetLo)], dh = val[bp(selBand, kBDetHi)];
        const float x0 = openLo(dl) ? DX + 4 : xOfHz(dl), x1 = openHi(dh) ? DX + DW - 4 : xOfHz(dh);
        if (std::fabs(x - x0) <= 6) return 0;
        if (std::fabs(x - x1) <= 6) return 1;
        if (x > x0 && x < x1) return 2;
        return -1;
    }
    bool thrHit(float x, float y) const { return selBand >= 0 && x >= BMX - 8 && x <= BMX + 16 && std::fabs(y - bmY(val[bp(selBand, kBThreshold)])) <= 6; }
    bool bandMeterHit(float x, float y) const { return selBand >= 0 && inRect(x, y, BMX - 8, BMY - 14, 50, BMH + 30); }
    bool tfHit(float x, float y) const { return selBand >= 0 && inRect(x, y, TX, TY, TS, TS); }
    bool removeHit(float x, float y) const { return selBand >= 0 && inRect(x, y, REM_X, REM_Y, REM_W, REM_H); }
    bool clearHit(float x, float y) const { return learnMode && learnWaiting < 0 && inRect(x, y, DX + 10, DY + DH - 50, 170, 20); }
    bool metersHit(float x, float y) const { return inRect(x, y, MX, MY, MW, MH); }

    void setLearn(bool on) { learnMode = on; learnWaiting = -1; if (!on) setState("cmd", "cancel"); repaint(); }
    void endEdit(bool apply) {
        if (editing < 0) return;
        const uint32_t p = effParam(controls()[editing]); float v;
        if (apply && parse(p, editBuf, v)) { beginGesture(); commit(p, v); endGesture(); }
        editing = -1; editBuf.clear(); repaint();
    }
    static float angleNorm(const Ctl& c, float x, float y) {
        float a = std::atan2(y - c.y, x - c.x); const float a0 = 0.75f * 3.14159265f;
        float rel = a - a0; while (rel < 0) rel += 2 * 3.14159265f;
        if (rel > 1.5f * 3.14159265f) rel = rel > 1.75f * 3.14159265f ? 0.f : 1.5f * 3.14159265f;
        return rel / (1.5f * 3.14159265f);
    }

    bool onKeyboard(const KeyboardEvent& ev) override {
        if (!ev.press) return false;
        std::string* buf = saving ? &saveName : (editing >= 0 ? &editBuf : nullptr);
        if (!buf && menuOpen) {                              // typing in the preset browser searches
            if (ev.key == kKeyEscape) { if (!query.empty()) query.clear(); else menuOpen = false; }
            else if (ev.key == kKeyBackspace || ev.key == kKeyDelete) { if (!query.empty()) query.pop_back(); }
            else if (ev.key >= 32 && ev.key < 127 && query.size() < 32) query += (char)ev.key;
            else return false;
            menuHover = -1; pendingDelete = -1; repaint(); return true;
        }
        if (!buf) {
            const bool cmd = (ev.mod & (kModifierControl | kModifierSuper)) != 0;
            if (cmd && (ev.key == 'z' || ev.key == 'Z')) { if (ev.mod & kModifierShift) redo(); else undo(); return true; }
            if (cmd && (ev.key == 'y' || ev.key == 'Y')) { redo(); return true; }
            if (ev.key == kKeyEscape) { if (helpOpen) helpOpen = false; else if (menuOpen) menuOpen = false; else if (learnMode) setLearn(false); repaint(); return true; }
            if ((ev.key == kKeyDelete || ev.key == kKeyBackspace) && selBand >= 0 && !helpOpen && !menuOpen) { removeBand(selBand); return true; }
            return false;
        }
        if (ev.key == kKeyEnter) {
            if (saving) { if (saveUserPreset(saveName, val)) { userPresets = listUserPresets(); curUser = sanitizeName(saveName); curFactory = -1; std::copy(val, val + kParamCount, userVals); saving = false; saveError = false; } else saveError = true; }
            else endEdit(true);
            repaint(); return true;
        }
        if (ev.key == kKeyEscape) { if (saving) saving = false; else endEdit(false); repaint(); return true; }
        if (ev.key == kKeyBackspace || ev.key == kKeyDelete) { if (!buf->empty()) buf->pop_back(); repaint(); return true; }
        if (ev.key >= 32 && ev.key < 127 && buf->size() < (saving ? 40u : 12u)) { *buf += (char)ev.key; repaint(); return true; }
        return true;
    }

    void endDrag() {
        if (dragKind == DragNone) return;
        dragKind = DragNone; dragBand = -1; endGesture();
    }
    bool onMouse(const MouseEvent& ev) override {
        const float s = uiScale(), x = ev.pos.getX() / s, y = ev.pos.getY() / s;
        if (saving) return true;
        if (ev.press) { statusFlash = 0; tipShown = false; tipSuppressed = tipKey; }
        if (helpOpen) { if (ev.press && ev.button == 1) helpClick(x, y); return true; }
        if (ev.button == 2 && ev.press) {                  // right-click (DPF numbers buttons 1 left, 2 right, 3 middle)
            if (menuOpen) { const int m = menuHit(x, y); if (m >= 0 && menuItem(m).kind == 1) { pendingDelete = menuItem(m).idx; repaint(); } return true; }
            if (learnMode) { const int i = hit(x, y); if (i >= 0) { const uint32_t p = effParam(controls()[i]); char b[24]; snprintf(b, sizeof b, "unmap:%u", p); setState("cmd", b); paramCC[p] = -1; repaint(); } return true; }
            int eb, es; const int b = edgeHit(x, y, eb, es) ? eb : bandAt(x, y);
            if (b >= 0) { removeBand(b); return true; }
            return false;
        }
        if (ev.button != 1) return false;
        if (!ev.press) {
            if (active >= 0) { editParameter(effParam(controls()[active]), false); active = -1; endGesture(); return true; }
            if (dragKind != DragNone) { endDrag(); return true; }
            return false;
        }
        if (editing >= 0) endEdit(true);
        if (menuOpen) {
            const int m = menuHit(x, y);
            if (m >= 0) { const MRect r = menuItem(m);
                if (r.kind == 3) return true;                       // the search field: just type
                if ((r.kind == 0 || r.kind == 1 || r.kind == 4) && x >= r.x + r.w - 20) { toggleFav(favKey(r.kind, r.idx)); return true; }
                if (r.kind == 2) { menuOpen = false; saving = true; saveError = false; saveName = curUser; }
                else if (r.kind == 1 && pendingDelete == r.idx) { deleteUserPreset(userPresets[r.idx]); if (curUser == userPresets[r.idx]) curUser.clear(); userPresets = listUserPresets(); pendingDelete = -1; repaint(); return true; }
                else { menuOpen = false; if (r.kind == 0) applyFactory(r.idx); else if (r.kind == 1) applyUser(userPresets[r.idx]); else applyFavourite(r.idx); } }
            else menuOpen = false;
            pendingDelete = -1; menuHover = -1; repaint(); return true;
        }
        const int hh = headerHit(x, y);
        if (hh == 1) { setLearn(!learnMode); return true; }
        if (hh == 2) { beginGesture(); commit(kMidiOn, val[kMidiOn] > 0.5f ? 0.f : 1.f); endGesture(); return true; }
        if (hh == 3) { undo(); return true; }
        if (hh == 4) { redo(); return true; }
        if (hh == 5) { helpOpen = true; repaint(); return true; }
        if (hh == 6 || hh == 7) { switchAB(hh - 6); return true; }
        if (hh == 8) { copyAB(); return true; }
        const int pb = presetBarHit(x, y);
        if (pb == 0) { step(-1); return true; }
        if (pb == 2) { step(1); return true; }
        if (pb == 1) { openMenu(); return true; }
        if (clearHit(x, y)) { setState("cmd", "clear"); for (int& m : paramCC) m = -1; repaint(); return true; }
        if (metersHit(x, y)) { pkIn = pkOut = -120; repaint(); return true; }
        if (removeHit(x, y) && !learnMode) { removeBand(selBand); return true; }

        int seg = 0; const int i = hit(x, y, &seg);
        if (i >= 0) {
            const Ctl& c = controls()[i]; const uint32_t p = effParam(c);
            if (learnMode) { learnWaiting = (int)p; char b[24]; snprintf(b, sizeof b, "learn:%u", p); setState("cmd", b); repaint(); return true; }
            if (isView(p)) { if (c.type == Toggle) commit(p, val[p] > 0.5f ? 0.f : 1.f); else if (c.type == Segment) commit(p, (float)seg); if (p == kAnaFreeze) for (float& h : specHold) h = -120; return true; }
            if (c.type == Toggle) { beginGesture(); commit(p, val[p] > 0.5f ? 0.f : 1.f); endGesture(); return true; }
            if (c.type == Segment) { beginGesture(); commit(p, (float)seg); endGesture(); return true; }
            if (c.type == Stepper) { const ParamDef& d = paramDef(p); float v = val[p] + seg; if (v > d.max) v = d.min; if (v < d.min) v = d.max; beginGesture(); commit(p, v); endGesture(); return true; }
            if (ev.mod & (kModifierControl | kModifierSuper)) { beginGesture(); commit(p, defv[p]); endGesture(); return true; }
            if (i == lastClick && ev.time - lastClickTime < 350) {
                lastClick = -1; editing = i; char b[40]; fmt(p, b, sizeof b); editBuf = b;
                const size_t spc = editBuf.find(' '); if (spc != std::string::npos) editBuf.resize(spc);
                if (!editBuf.empty() && editBuf.back() == '%') editBuf.pop_back();
                if (editBuf.size() > 2 && editBuf.compare(editBuf.size() - 2, 2, ":1") == 0) editBuf.resize(editBuf.size() - 2);
                if (editBuf == "Shelf") editBuf = fieldOf(p) == kBLo ? "10" : "30k";
                repaint(); return true;
            }
            lastClick = i; lastClickTime = ev.time;
            beginGesture(); active = i; dragY = y; dragNorm = toNorm(p, val[p]);
            const float dist = std::sqrt((x - c.x) * (x - c.x) + (y - c.y) * (y - c.y));
            circular = dist > c.r * 0.62f && c.r > 15;
            editParameter(p, true);
            if (circular) setValue(p, fromNorm(p, angleNorm(c, x, y)));
            repaint(); return true;
        }
        if (learnMode) return false;
        if (thrHit(x, y) || (bandMeterHit(x, y) && x < BMX + 14)) { beginGesture(); dragKind = DragThr; dragBand = selBand; setValue(bp(selBand, kBThreshold), std::round(-clampf((y - BMY) / BMH, 0.f, 1.f) * 600.f) / 10.f); return true; }
        if (tfHit(x, y)) { beginGesture(); dragKind = DragTf; dragBand = selBand; setValue(bp(selBand, kBThreshold), std::round(clampf((x - TX) / TS * 60.f - 60.f, -80.f, 0.f) * 10.f) / 10.f); return true; }
        const int dh = detHit(x, y);
        if (dh >= 0) { beginGesture(); dragKind = DragDet; dragSide = dh; dragX0 = x; dragLo0 = val[bp(selBand, kBDetLo)]; dragHi0 = val[bp(selBand, kBDetHi)]; return true; }
        if (inDisplay(x, y)) {
            const bool dbl = ev.time - lastDispClick < 350 && std::fabs(x - lastDispX) < 6 && std::fabs(y - lastDispY) < 6;
            lastDispClick = ev.time; lastDispX = x; lastDispY = y;
            int eb = -1, es = 0;
            const int hb = handleHit(x, y);
            if (hb < 0 && edgeHit(x, y, eb, es)) {
                selBand = eb; beginGesture(); dragKind = DragEdge; dragBand = eb; dragSide = es; dragNb = neighbourAt(eb, es); return true;
            }
            const int b = hb >= 0 ? hb : bandAt(x, y);
            if (b >= 0) {
                if (dbl) { lastDispClick = 0; beginGesture(); commit(bp(b, kBBypass), val[bp(b, kBBypass)] > 0.5f ? 0.f : 1.f); endGesture(); flash(val[bp(b, kBBypass)] > 0.5f ? "Band bypassed" : "Band active"); return true; }
                selBand = b; beginGesture(); dragKind = DragMove; dragBand = b; dragX0 = x; dragY0 = y; dragLo0 = lo(b); dragHi0 = hi(b); dragRange0 = val[bp(b, kBRange)]; repaint(); return true;
            }
            if (dbl) { lastDispClick = 0; createBandAt(hzOfX(x)); return true; }
            return true;
        }
        return false;
    }

    bool onMotion(const MotionEvent& ev) override {
        const float s = uiScale(), x = ev.pos.getX() / s, y = ev.pos.getY() / s;
        if (saving) return true;
        if (helpOpen) { const int h = helpHit(x, y); if (h != helpHover) { helpHover = h; repaint(); } return true; }
        const bool fine = (ev.mod & kModifierShift) != 0;
        if (active >= 0) {
            const Ctl& c = controls()[active]; const uint32_t p = effParam(c);
            if (circular) { setValue(p, fromNorm(p, angleNorm(c, x, y))); return true; }
            dragNorm = clampf(dragNorm + (dragY - y) / 240.f * (fine ? 0.2f : 1.f), 0.f, 1.f); dragY = y; setValue(p, fromNorm(p, dragNorm)); return true;
        }
        switch (dragKind) {
        case DragMove: moveBand(x, y, fine); return true;
        case DragEdge: moveEdge(x); return true;
        case DragDet: moveDet(x); return true;
        case DragThr: setValue(bp(dragBand, kBThreshold), std::round(-clampf((y - BMY) / BMH, 0.f, 1.f) * 600.f) / 10.f); return true;
        case DragTf: setValue(bp(dragBand, kBThreshold), std::round(clampf((x - TX) / TS * 60.f - 60.f, -80.f, 0.f) * 10.f) / 10.f); return true;
        default: break;
        }
        if (menuOpen) { const int m = menuHit(x, y); if (m != menuHover) { menuHover = m; repaint(); } return true; }
        tipX = x; tipY = y;
        const int hh = headerHit(x, y); if (hh != hoverHeader) { hoverHeader = hh; repaint(); }
        const int pb = presetBarHit(x, y); if (pb != presetHover) { presetHover = pb; repaint(); }
        const bool hc = clearHit(x, y); if (hc != hoverClear) { hoverClear = hc; repaint(); }
        const int h = hit(x, y); if (h != hover) { hover = h; repaint(); }
        const int hd = detHit(x, y); if (hd != hoverDet) { hoverDet = hd; repaint(); }
        const int hb = hd < 0 ? handleHit(x, y) : -1; if (hb != hoverHandle) { hoverHandle = hb; repaint(); }
        int eb = -1, es = 0; const bool he = hd < 0 && hb < 0 && edgeHit(x, y, eb, es);
        if ((he ? eb : -1) != hoverEdgeBand || es != hoverEdgeSide) { hoverEdgeBand = he ? eb : -1; hoverEdgeSide = es; repaint(); }
        const int ba = (hd < 0 && !he) ? (hb >= 0 ? hb : bandAt(x, y)) : -1; if (ba != hoverBand) { hoverBand = ba; repaint(); }
        const bool hr = removeHit(x, y); if (hr != hoverRemove) { hoverRemove = hr; repaint(); }
        const bool ht = thrHit(x, y); if (ht != hoverThr) { hoverThr = ht; repaint(); }
        const bool htf = tfHit(x, y); if (htf != hoverTf) { hoverTf = htf; repaint(); }
        if (inDisplay(x, y)) repaint();
        // tips
        int key = -1;
        if (h >= 0) key = h; else if (hh > 0) key = 1000 + hh; else if (pb >= 0) key = 2000 + pb;
        else if (hd >= 0) key = 3000 + kTipDetect; else if (hb >= 0) key = 3000 + kTipRangeHandle; else if (he) key = 3000 + kTipEdge; else if (ba >= 0) key = 3000 + kTipBand;
        else if (hr) key = 3000 + kTipRemove; else if (bandMeterHit(x, y)) key = 3000 + kTipBandMeter; else if (htf) key = 3000 + kTipTransfer;
        else if (metersHit(x, y)) key = 3000 + kTipMeters; else if (inDisplay(x, y)) key = 3000 + kTipDisplay;
        if (key != tipKey) { const bool was = tipShown; tipKey = key; tipSince = nowSec(); tipShown = false; if (key != tipSuppressed) tipSuppressed = -2; if (was) repaint(); }
        return false;
    }

    bool onScroll(const ScrollEvent& ev) override {
        const float s = uiScale(), sx = ev.pos.getX() / s, sy = ev.pos.getY() / s;
        if (menuOpen || saving || helpOpen) return true;
        if (presetBarHit(sx, sy) >= 0) { step(ev.delta.getY() > 0 ? -1 : 1); return true; }
        const bool fine = (ev.mod & kModifierShift) != 0; const float st = fine ? 0.004f : 0.02f, d = ev.delta.getY() > 0 ? st : -st;
        const int i = hit(sx, sy);
        if (i >= 0 && controls()[i].type == Knob) { const uint32_t p = effParam(controls()[i]); beginGesture(); commit(p, fromNorm(p, toNorm(p, val[p]) + d)); endGesture(); return true; }
        int eb, es; const int b = edgeHit(sx, sy, eb, es) ? eb : bandAt(sx, sy);
        if (b >= 0) { const uint32_t p = bp(b, kBThreshold); beginGesture(); commit(p, clampf(std::round((val[p] + (ev.delta.getY() > 0 ? 1.f : -1.f) * (fine ? 0.2f : 1.f)) * 10.f) / 10.f, -80.f, 0.f)); endGesture();
            char t[48]; snprintf(t, sizeof t, "Band %d threshold %.1f dB", b + 1, val[p]); flash(t); return true; }
        return false;
    }

    void uiIdle() override {
        const double t = nowSec();
        if (t - lastPoll > 0.033) {
            lastPoll = t;
            if (SchistPlugin* const dsp = (SchistPlugin*)getPluginInstancePointer()) {
                const Engine& e = dsp->engine;
                for (int b = 0; b < kMaxBands; ++b) { gcLive[b] = e.gcNow[b]; levelLive[b] = e.levelNow[b]; liveDb[b] = e.bandDb(b); }
                for (int i = 0; i < CURVE_PTS; ++i) total[i] = e.responseDb(hzOfX(DX + 6 + (DW - 12) * i / (CURVE_PTS - 1.f)));
                for (int c = 0; c < 2; ++c) { mIn[c] = e.meterIn[c]; mOut[c] = e.meterOut[c]; }
                pkIn = std::max(pkIn, gainToDb(std::max(mIn[0], mIn[1]))); pkOut = std::max(pkOut, gainToDb(std::max(mOut[0], mOut[1])));
                latencyMs = e.latency() * 1000.f / std::max(1.f, e.sampleRate());
                const uint32_t w = e.anaWrite.load();
                if (w != lastAna && val[kAnaMode] > 0.5f) { lastAna = w; analyse(e, w); }
            }
            repaint();
        }
        if (gTipsOn && !tipShown && tipKey >= 0 && tipKey != tipSuppressed && active < 0 && dragKind == DragNone && !menuOpen && !saving && !helpOpen && editing < 0
            && t - tipSince > 0.6) { tipShown = true; repaint(); }
    }
    void analyse(const Engine& e, uint32_t w) {
        const float sr = e.sampleRate(); const int am = (int)std::round(val[kAnaMode]);
        for (int which = 0; which < 2; ++which) {
            if ((which == 0 && !(am == 1 || am == 3)) || (which == 1 && am < 2)) continue;
            const float* src = which ? e.anaOut.data() : e.anaIn.data();
            for (int i = 0; i < ANA_N; ++i) { const float hann = 0.5f - 0.5f * std::cos(2 * kPi * i / (ANA_N - 1)); buf[i] = cpx(src[(w - ANA_N + i + Engine::kAna) % Engine::kAna] * hann, 0); }
            fft.run(buf, false);
            float* sp_ = which ? specOut : specIn;
            for (int k = 0; k < ANA_PTS; ++k) {
                const float f0 = kFreqMin * std::pow(kFreqMax / kFreqMin, k / (float)ANA_PTS), f1 = kFreqMin * std::pow(kFreqMax / kFreqMin, (k + 1) / (float)ANA_PTS);
                int b0 = (int)(f0 / sr * ANA_N), b1 = std::max(b0 + 1, (int)(f1 / sr * ANA_N)); b0 = std::min(b0, ANA_N / 2 - 1); b1 = std::min(b1, ANA_N / 2);
                float m = 0; for (int b = b0; b < b1; ++b) m = std::max(m, std::norm(buf[b]));
                const float tilt = 4.5f * std::log2(std::max(f0, 20.f) / 1000.f);         // ~4.5 dB/oct tilt so a balanced mix looks flat
                const float db = (f0 > sr * 0.5f) ? -120.f : 10.f * std::log10(m + 1e-20f) - 20.f * std::log10(ANA_N / 4.f) + tilt;
                sp_[k] = db > sp_[k] ? db : sp_[k] + (db - sp_[k]) * 0.25f;
            }
        }
        if (val[kAnaFreeze] > 0.5f) { const float* sp_ = am >= 2 ? specOut : specIn; for (int k = 0; k < ANA_PTS; ++k) specHold[k] = std::max(specHold[k], sp_[k]); }
    }

    // ---- tips --------------------------------------------------------------------
    bool tipContent(std::string& title, std::string& value, std::string& body) const {
        if (tipKey < 0) return false;
        char b[64];
        if (tipKey < 1000) { const uint32_t p = effParam(controls()[tipKey]); fmt(p, b, sizeof b); title = paramDef(p).name; value = b; body = paramTip(p);
            if (controls()[tipKey].type == Knob) body += "\n\nDrag up/down or grab the rim. Double-click to type, Cmd/Ctrl-click to reset, Shift for fine steps.";
            if (paramCC[p] >= 0) { char cc[24]; snprintf(cc, sizeof cc, "   ·   CC%d", paramCC[p]); value += cc; }
            return true; }
        int t = -1;
        if (tipKey >= 3000) t = tipKey - 3000;
        else if (tipKey >= 2000) t = kTipPrev + (tipKey - 2000);
        else { const int hh = tipKey - 1000; t = hh == 1 ? kTipLearn : hh == 2 ? kTipMidi : hh == 3 ? kTipUndo : hh == 4 ? kTipRedo : hh == 5 ? kTipHelp : hh == 6 ? kTipA : hh == 7 ? kTipB : kTipCopy; }
        title = targetTitle(t); value.clear(); body = targetTip(t);
        if (t == kTipUndo || t == kTipRedo) { const size_t n = t == kTipUndo ? undoStack.size() : redoStack.size(); snprintf(b, sizeof b, "%d step%s", (int)n, n == 1 ? "" : "s"); value = b; }
        if (t == kTipBand && hoverBand >= 0) { title = "Band " + std::to_string(hoverBand + 1); snprintf(b, sizeof b, "%s, %s", fmtHz(centre(hoverBand)).c_str(), val[bp(hoverBand, kBMode)] > 0.5f ? "expand" : "compress"); value = b; }
        if (t == kTipRangeHandle && hoverHandle >= 0) { fmt(bp(hoverHandle, kBRange), b, sizeof b); value = b; }
        if (t == kTipDisplay) { const float hz = hzOfX(tipX); value = fmtHz(hz); }
        return true;
    }
    void drawTooltip() {
        std::string title, value, body; if (!tipContent(title, value, body)) return;
        const float w = 320, pad = 12;
        fontSize(11); textLetterSpacing(0.2f); textLineHeight(1.3f);
        float bb[4]; textBoxBounds(0, 0, w - 2 * pad, body.c_str(), nullptr, bb);
        const float h = pad + 16 + 8 + (bb[3] - bb[1]) + pad;
        float x = tipX + 16, y = tipY + 20;
        if (x + w > W - 8) x = tipX - w - 12;
        if (y + h > H - 8) y = tipY - h - 12;
        x = clampf(x, 8, W - w - 8); y = clampf(y, 8, H - h - 8);
        beginPath(); rect(x - 10, y - 4, w + 20, h + 18); fillPaint(boxGradient(x, y + 3, w, h, 6, 14, Color(0, 0, 0, 0.55f), Color(0, 0, 0, 0.f))); fill();
        beginPath(); roundedRect(x, y, w, h, 6); fillColor(Color(0x1e, 0x22, 0x21)); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        fontSize(10); textLetterSpacing(1.6f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillColor(C_TEXT);
        std::string up = title; for (char& ch : up) ch = (char)std::toupper((unsigned char)ch); text(x + pad, y + pad + 1, up.c_str(), nullptr);
        if (!value.empty()) { fontSize(10.5f); textLetterSpacing(0.4f); textAlign(ALIGN_RIGHT | ALIGN_TOP); fillColor(C_ACC); text(x + w - pad, y + pad + 1, value.c_str(), nullptr); }
        fontSize(11); textLetterSpacing(0.2f); textLineHeight(1.3f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillColor(C_DIM); textBox(x + pad, y + pad + 24, w - 2 * pad, body.c_str(), nullptr);
        textLineHeight(1.f);
    }

    // ---- help window ---------------------------------------------------------------
    static constexpr float HX = 210, HY = 110, HW = 760, HH = 560, HTAB_W = 170, HTAB_H = 34;
    int helpHit(float x, float y) const {
        if (x < HX || x > HX + HW || y < HY || y > HY + HH) return -2;
        for (int i = 0; i < kNumHelpPages; ++i) if (x >= HX + 12 && x < HX + 12 + HTAB_W && y >= HY + 56 + i * HTAB_H && y < HY + 56 + (i + 1) * HTAB_H) return i;
        const float by = HY + HH - 44;
        if (y >= by && y < by + 28) { if (x >= HX + 12 && x < HX + 12 + HTAB_W) return 100; if (x >= HX + HW - 330 && x < HX + HW - 140) return 101; if (x >= HX + HW - 124 && x < HX + HW - 16) return 102; }
        if (x >= HX + HW - 40 && y < HY + 40) return 102;
        return -1;
    }
    void helpClick(float x, float y) {
        const int h = helpHit(x, y);
        if (h >= 0 && h < kNumHelpPages) helpPage = h; else if (h == 100) gTipsOn = !gTipsOn; else if (h == 101) openURL(kManualURL); else if (h == 102 || h == -2) helpOpen = false;
        repaint();
    }
    void drawHelp() {
        beginPath(); rect(0, 0, W, H); fillColor(Color(0, 0, 0, 0.55f)); fill();
        beginPath(); roundedRect(HX, HY, HW, HH, 10); fillColor(C_PANEL); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        fontSize(13); textLetterSpacing(4.f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(C_TEXT); text(HX + 22, HY + 28, "SCHIST HELP", nullptr);
        const float cx = HX + HW - 24, cy = HY + 26;
        beginPath(); moveTo(cx - 6, cy - 6); lineTo(cx + 6, cy + 6); moveTo(cx + 6, cy - 6); lineTo(cx - 6, cy + 6); strokeColor(helpHover == 102 ? C_TEXT : C_DIM); strokeWidth(1.6f); stroke();
        for (int i = 0; i < kNumHelpPages; ++i) {
            const float ty = HY + 56 + i * HTAB_H;
            if (i == helpPage || i == helpHover) { beginPath(); roundedRect(HX + 12, ty + 2, HTAB_W, HTAB_H - 4, 5); fillColor(i == helpPage ? withAlpha(C_ACC, 0.18f) : C_PANEL_HI); fill(); }
            if (i == helpPage) { beginPath(); roundedRect(HX + 12, ty + 8, 3, HTAB_H - 16, 1.5f); fillColor(C_ACC); fill(); }
            fontSize(11.5f); textLetterSpacing(0.6f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(i == helpPage ? C_TEXT : C_DIM); text(HX + 28, ty + HTAB_H / 2 + 0.5f, kHelpPages[i].title, nullptr);
        }
        beginPath(); moveTo(HX + HTAB_W + 26, HY + 56); lineTo(HX + HTAB_W + 26, HY + HH - 56); strokeColor(C_LINE); strokeWidth(1); stroke();
        const float bx = HX + HTAB_W + 46, bw = HW - HTAB_W - 70;
        fontSize(15); textLetterSpacing(0.8f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillColor(C_ACC); text(bx, HY + 60, kHelpPages[helpPage].title, nullptr);
        fontSize(12); textLetterSpacing(0.2f); textLineHeight(1.38f); fillColor(Color(0xc4, 0xcc, 0xc8)); textBox(bx, HY + 92, bw, kHelpPages[helpPage].body, nullptr); textLineHeight(1.f);
        const float by = HY + HH - 44;
        beginPath(); moveTo(HX + 12, by - 10); lineTo(HX + HW - 12, by - 10); strokeColor(C_LINE); stroke();
        const float tw = 34, th = 18, tx = HX + 16, tyy = by + 14 - th / 2;
        beginPath(); roundedRect(tx, tyy, tw, th, th / 2); fillColor(gTipsOn ? withAlpha(C_ACC, 0.25f) : C_PANEL_HI); fill(); strokeColor(gTipsOn ? C_ACC : C_LINE); stroke();
        beginPath(); circle(gTipsOn ? tx + tw - th / 2 : tx + th / 2, by + 14, 6); fillColor(gTipsOn ? C_ACC : C_DIM); fill();
        fontSize(10); textLetterSpacing(1.6f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(helpHover == 100 ? C_TEXT : C_DIM); text(tx + tw + 10, by + 14.5f, "HOVER TIPS", nullptr);
        auto btn = [&](float x0, float w0, const char* label, bool hot, bool primary) {
            beginPath(); roundedRect(x0, by, w0, 28, 5); fillColor(primary ? (hot ? withAlpha(C_ACC, 0.32f) : withAlpha(C_ACC, 0.16f)) : (hot ? C_PANEL_HI : C_PANEL)); fill();
            strokeColor(primary ? C_ACC : C_LINE); strokeWidth(1); stroke();
            fontSize(9.5f); textLetterSpacing(1.8f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_TEXT); text(x0 + w0 / 2, by + 14.5f, label, nullptr); };
        btn(HX + HW - 330, 190, "FULL MANUAL (WEB)", helpHover == 101, false);
        btn(HX + HW - 124, 108, "CLOSE", helpHover == 102, true);
    }

private:
    enum DragKind { DragNone, DragMove, DragEdge, DragDet, DragThr, DragTf };
    float val[kParamCount], defv[kParamCount], userVals[kParamCount] = {}, before[kParamCount];
    int paramCC[kParamCount];
    int active = -1, hover = -1, lastClick = -1, editing = -1;
    uint lastClickTime = 0, lastDispClick = 0; float lastDispX = 0, lastDispY = 0;
    float dragY = 0, dragNorm = 0; bool circular = false, gesture = false;
    int selBand = -1, hoverBand = -1, hoverHandle = -1, hoverEdgeBand = -1, hoverEdgeSide = 0, hoverDet = -1;
    int dragKind = DragNone, dragBand = -1, dragSide = 0, dragNb = -1; float dragX0 = 0, dragY0 = 0, dragLo0 = 0, dragHi0 = 0, dragRange0 = 0;
    bool hoverRemove = false, hoverThr = false, hoverTf = false;
    std::vector<Snap> undoStack, redoStack; Snap abStore[2]; int abSlot = 0;
    std::string editBuf, statusText; int statusFlash = 0;
    int curFactory = 0, presetHover = -1, menuHover = -1, pendingDelete = -1, hoverHeader = 0;
    std::string curUser, query; std::vector<std::string> userPresets, favs;
    bool menuOpen = false, saving = false, saveError = false; std::string saveName;
    bool learnMode = false, hoverClear = false; int learnWaiting = -1; float lastLearnEcho = 0;
    int tipKey = -1, tipSuppressed = -2; double tipSince = 0; bool tipShown = false; float tipX = 0, tipY = 0;
    bool helpOpen = false; int helpPage = 0, helpHover = -1;
    float gcLive[kMaxBands] = {}, levelLive[kMaxBands] = {}, liveDb[kMaxBands] = {}, total[CURVE_PTS];
    float mIn[2] = {}, mOut[2] = {}, pkIn = -120, pkOut = -120;
    float latencyMs = 0; double lastPoll = 0; uint32_t lastAna = 0;
    Fft fft; cpx buf[ANA_N]; float specIn[ANA_PTS], specOut[ANA_PTS], specHold[ANA_PTS];
    ResizeHandle resizeHandle;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SchistUI)
};

UI* createUI() { return new SchistUI(); }

END_NAMESPACE_DISTRHO
