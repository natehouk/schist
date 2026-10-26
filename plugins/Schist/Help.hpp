// Schist — hover tips and the in-plugin help pages. License: MIT.
#pragma once
#include "ParamDefs.hpp"

namespace schist {

static const char* const kManualURL = "https://github.com/natehouk/schist/blob/main/docs/MANUAL.md";

inline const char* paramTip(uint32_t p) {
    switch (fieldOf(p)) {
    case kBUsed: return "Whether this band exists. Double-click the display to create a band; right-click a band to remove it.";
    case kBBypass: return "Switches the band off without removing it. The band glides smoothly back to no gain change.";
    case kBSolo: return "Listen to just this band's part of the spectrum (after processing). Solo several bands to hear them together.";
    case kBLo: return "Bottom edge of the band. Drag it all the way down and the band becomes a low shelf that reaches to the bottom of the spectrum.";
    case kBHi: return "Top edge of the band. Drag it all the way up and the band becomes a high shelf that reaches to the top of the spectrum.";
    case kBSlope: return "How steeply the band's edges fall away, from a gentle 6 dB/oct to a sharp 48 dB/oct. Click the arrows or the value to step.";
    case kBMode: return "COMPRESS reduces the distance between loud and quiet. EXPAND increases it. Range decides whether the band works downward (negative) or upward (positive).";
    case kBThreshold: return "The level where the band starts to act. Above it a downward compressor turns the band down; below it an upward compressor turns it up, and a downward expander (gate) turns it down.";
    case kBRange: return "The most gain change the band may apply, and its direction. Negative values cut (downward compression or expansion), positive values boost (upward). Drag the band up or down in the display to change it.";
    case kBRatio: return "How strongly the band reacts past the threshold. 2:1 halves the distance. All the way up is Inf:1: compressing, the band becomes a limiter (nothing passes the threshold); expanding, a gate (the full range applies the moment the level drops below it). Use a fast attack, and for limiting some lookahead.";
    case kBKnee: return "How gradually the band starts to act around the threshold. 0 dB is a hard corner; 30 dB eases in very gently.";
    case kBAttack: return "How fast the gain change moves away from 0 dB once the band starts working. Very low bands are kept a little slower automatically, so they don't distort.";
    case kBRelease: return "How fast the gain change returns. Release adapts to the material: short peaks recover faster than sustained compression.";
    case kBLookahead: return "Lets this band see the audio up to 20 ms early, so it can catch sudden peaks. Any lookahead adds 20 ms of latency to the plugin (reported to your host).";
    case kBGain: return "A fixed boost or cut for the band, on top of the dynamic gain change. Use it as make-up gain, or on its own as a static EQ band.";
    case kBLink: return "How much the left and right channels share the gain change. 100%: both move together, so the stereo image stays put. 0%: each channel is processed on its own.";
    case kBStereo: return "Expert: STEREO processes left and right. MID processes only the centre of the stereo image, SIDE only the stereo width.";
    case kBSidechain: return "Expert: INTERNAL listens to the plugin's input. EXTERNAL listens to the sidechain input instead (route another track to it in your host).";
    case kBDetCustom: return "Expert: when on, the band listens to its own detection range (shown under the display) instead of its own frequencies. Lets, say, the kick drive compression of the hi-hats.";
    case kBDetLo: return "Expert: bottom of the frequency range the band listens to.";
    case kBDetHi: return "Expert: top of the frequency range the band listens to.";
    default: break;
    }
    switch (p) {
    case kProcMode: return "DYNAMIC PHASE: zero latency, and a band changes the sound (and phase) only while it applies gain. LINEAR PHASE: no phase change at all, at the cost of about 90 ms latency (4096 samples). MINIMUM PHASE: classic crossovers that split the signal into bands, zero latency, tight band separation.";
    case kOversampling: return "Runs the processing at 2x or 4x the sample rate (dynamic and minimum phase), with linear-phase filters. Cleaner very fast gain changes and bands near the top of the spectrum; more CPU and a little latency.";
    case kMix: return "Blend of processed and dry signal, time-aligned. Below 100% is parallel processing; above 100% exaggerates the gain changes.";
    case kOutGain: return "Final output level.";
    case kOutPan: return "Output balance between left and right.";
    case kExpert: return "Shows the expert band controls: mid/side processing, external sidechain and custom detection range.";
    case kAnaMode: return "The spectrum analyzer: off, the input (PRE), the output (POST), or both.";
    case kAnaFreeze: return "Holds the peak of the spectrum while it plays, so you can see where the energy builds up. Turn it off to clear.";
    case kDisplayRange: return "Vertical range of the gain curves in the display: +-6, +-12 or +-30 dB.";
    case kMidiOn: return "Turns MIDI control (MIDI Learn) on or off.";
    default: return "";
    }
}

enum TipTarget { kTipUndo, kTipRedo, kTipLearn, kTipMidi, kTipHelp, kTipA, kTipB, kTipCopy, kTipPrev, kTipPresets, kTipNext,
                 kTipDisplay, kTipBand, kTipEdge, kTipRangeHandle, kTipDetect, kTipBandMeter, kTipTransfer, kTipRemove, kTipMeters, kNumTipTargets };
inline const char* targetTitle(int t) {
    static const char* const n[kNumTipTargets] = { "Undo", "Redo", "MIDI Learn", "MIDI", "Help", "Setting A", "Setting B", "Copy", "Previous preset", "Presets", "Next preset",
        "Display", "Band", "Band edge", "Range", "Detection range", "Band level", "Transfer curve", "Remove band", "Meters" };
    return n[t];
}
inline const char* targetTip(int t) {
    switch (t) {
    case kTipUndo: return "Steps back through your edits, including band changes, preset loads and A/B switches. Cmd/Ctrl-Z.";
    case kTipRedo: return "Re-applies an edit you undid. Cmd/Ctrl-Shift-Z or Cmd/Ctrl-Y.";
    case kTipLearn: return "Click, then click any control and move a knob or fader on your MIDI controller to assign it. Right-click a control to unassign.";
    case kTipMidi: return paramTip(kMidiOn);
    case kTipHelp: return "Opens the help pages: bands, the controls, processing modes, expert mode, MIDI and shortcuts.";
    case kTipA: case kTipB: return "Schist keeps two complete settings, A and B. Click the other letter to switch and compare.";
    case kTipCopy: return "Copies the current setting to the other slot.";
    case kTipPrev: return "Load the previous preset. The mouse wheel over the preset name also steps through presets.";
    case kTipPresets: return "Open the preset browser. A * after the name means you have changed it. In the browser, type to search names and categories, and click a star to keep a preset in Favourites.";
    case kTipNext: return "Load the next preset.";
    case kTipDisplay: return "Double-click to create a band here (up to six). The white line is the gain the whole plugin applies right now; behind it is the spectrum.";
    case kTipBand: return "Drag sideways to move the band, up or down to change its range. Mouse wheel: threshold. Click to select it, right-click to remove it. The coloured fill shows the gain change right now; the faint outline shows the full range.";
    case kTipEdge: return "Drag to change where the band starts or ends. Bands snap together when their edges meet, and move together afterwards. Drag to the very end of the display to make the band a shelf.";
    case kTipRangeHandle: return "Drag up or down to set the band's range: how far, and in which direction, it may change the gain.";
    case kTipDetect: return "The frequencies this band listens to (custom detection is on). Drag the ends, or the middle to move it.";
    case kTipBandMeter: return "Left: the level the band hears, with the threshold (drag it). Right: the gain change it applies now: red cuts, green boosts.";
    case kTipTransfer: return "Input level (across) against output level (up) for this band. The dot is where the band is now. Drag sideways to move the threshold.";
    case kTipRemove: return "Removes this band. Right-clicking a band in the display does the same; Undo brings it back.";
    case kTipMeters: return "Input and output peak levels. Click to reset the peak readouts.";
    default: return "";
    }
}

struct HelpPage { const char* title; const char* body; };
static const HelpPage kHelpPages[] = {
    { "Overview",
      "Schist is a multiband dynamics processor: up to six bands, each one a compressor or an expander that works only on its own part of the spectrum. "
      "Bands can sit anywhere, with gaps between them, or snap together like a classic crossover.\n\n"
      "Each band can turn loud parts down (downward compression), bring quiet parts up (upward compression), push quiet parts further down (gating) "
      "or make loud parts louder (upward expansion).\n\n"
      "Double-click the display to create a band, then shape it with the controls below. Hover over anything for a moment to see what it does." },
    { "Bands",
      "Double-click the display to create a band (up to six). Drag a band sideways to move it and up or down to set its range. "
      "Drag its edges to resize it; edges that meet snap together and then move together. Drag an edge to the end of the display and the band becomes a shelf.\n\n"
      "The mouse wheel over a band changes its threshold. Right-click a band (or click REMOVE) to delete it.\n\n"
      "The coloured fill is the gain change a band applies right now; the faint outline is its full range; the white line is the total." },
    { "Band controls",
      "THRESHOLD: where the band starts to act.\nRANGE: the most it may change the gain. Negative cuts (downward), positive boosts (upward).\n"
      "RATIO and KNEE: how strongly and how gradually it acts.\nATTACK and RELEASE: how fast it moves. Release adapts to the material, and very low bands are kept a little slower.\n"
      "LOOKAHEAD: lets the band react before a peak arrives (adds 20 ms latency).\nGAIN: a fixed boost or cut. LINK: how much left and right share the gain change.\n"
      "SLOPE: how steep the band's edges are, 6 to 48 dB/oct.\n\n"
      "The meter shows the band's level against the threshold and its gain change; the graph shows the band's input/output curve." },
    { "Processing modes",
      "DYNAMIC PHASE (default): zero latency. A band acts like an EQ band whose gain follows the compressor, so it touches the sound, including its phase, only while it changes the gain. "
      "A band doing nothing is a perfect wire.\n\n"
      "LINEAR PHASE: the bands are linear-phase filters, so nothing is phase-shifted, even with big gain changes. Costs 4096 samples of latency (about 90 ms).\n\n"
      "MINIMUM PHASE: classic Linkwitz-Riley crossovers split the audio at every band edge, with zero latency and tight band separation; the crossovers shift the phase a little all the time.\n\n"
      "OVERSAMPLING 2x or 4x runs dynamic and minimum phase at a higher rate for very fast gain changes and bands near the top of the spectrum." },
    { "Expert mode",
      "Click EXPERT to show more band controls.\n\n"
      "STEREO / MID / SIDE: process both channels, only the centre, or only the width.\n"
      "SIDECHAIN: INTERNAL listens to the input; EXTERNAL listens to the plugin's sidechain input (route a track to it in your host).\n"
      "CUSTOM DETECTION: the band listens to a different frequency range, shown as a bar under the display. "
      "Drag the bar's ends to set it. Example: duck the hi-hats whenever the kick hits." },
    { "A/B, undo & MIDI",
      "A/B holds two complete settings; COPY copies the current one to the other slot. Every edit is an undo step (Cmd/Ctrl-Z, Cmd/Ctrl-Shift-Z).\n\n"
      "MIDI LEARN: click LEARN, click a control, move a controller. Right-click unassigns.\n\n"
      "Double-click a knob to type a value; Cmd/Ctrl-click resets it. Shift-drag is fine adjustment. Delete removes the selected band. Esc closes this window.\n\n"
      "Global MIX goes from 0 to 200%: below 100% blends in the dry signal, above 100% exaggerates the processing." },
};
static constexpr int kNumHelpPages = sizeof(kHelpPages) / sizeof(kHelpPages[0]);

} // namespace schist
