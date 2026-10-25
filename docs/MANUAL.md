# Schist 2.0 — User Manual

Schist is a multiband dynamics processor. You place up to six bands anywhere in the spectrum, with gaps between them or snapped together like a classic crossover. Each band is its own compressor or expander and acts only on its own frequencies:

- **downward compression:** turns loud parts down
- **upward compression:** brings quiet parts up
- **downward expansion (gating):** pushes quiet parts further down
- **upward expansion:** makes loud parts louder

It has three processing modes (dynamic phase, linear phase and minimum phase), look‑ahead, mid/side bands, an external sidechain, custom detection ranges, up to 4× oversampling and a spectrum analyzer.

Schist runs as VST3, AU, CLAP and VST2 on macOS, Windows and Linux.

**Contents**

1. [Installing](#1-installing)
2. [A quick tour](#2-a-quick-tour)
3. [Working with bands](#3-working-with-bands)
4. [Compress or expand, down or up](#4-compress-or-expand-down-or-up)
5. [Band controls](#5-band-controls)
6. [Processing modes](#6-processing-modes)
7. [Expert mode](#7-expert-mode)
8. [Global controls](#8-global-controls)
9. [The display and meters](#9-the-display-and-meters)
10. [Knobs](#10-knobs)
11. [A/B, undo and redo](#11-ab-undo-and-redo)
12. [MIDI](#12-midi)
13. [Presets](#13-presets)
14. [Help and hover tips](#14-help-and-hover-tips)
15. [Recipes](#15-recipes)
16. [Keyboard and mouse reference](#16-keyboard-and-mouse-reference)
17. [Parameter reference](#17-parameter-reference)
18. [Troubleshooting](#18-troubleshooting)

---

## 1. Installing

Download the zip for your system from the project's **Releases** page and copy the plugin files:

| System | Copy | To |
|---|---|---|
| macOS | `Schist.vst3`, `Schist.component` (AU), `Schist.clap` | `~/Library/Audio/Plug-Ins/VST3`, `.../Components`, `.../CLAP` |
| Windows | the `Schist.vst3` folder, `Schist.clap` | `C:\Program Files\Common Files\VST3\`, `...\CLAP\` |
| Linux | `Schist.vst3`, `Schist.clap` | `~/.vst3/`, `~/.clap/` |

**macOS:** the builds are signed ad hoc but not notarized. If macOS blocks them, open Terminal and run:

```
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/*/Schist*
```

Rescan plugins in your DAW. Schist appears under **Epsilon Audio** as a dynamics effect. It has a stereo input, a stereo output and a stereo **sidechain** input.

---

## 2. A quick tour

![Schist](../screenshot.png)

- **Header:**
  - preset browser
  - A/B and COPY
  - UNDO/REDO
  - LEARN (MIDI Learn) and MIDI on/off
  - HELP
  - the current latency, at the far right
- **Display:**
  - the spectrum: input in grey, output in green
  - each band as a coloured region:
    - the faint outline is its full range (the most it may change the gain)
    - the filled shape is the gain change it applies right now
    - the bar is its range handle
  - the white line: the total gain the plugin applies at each frequency, right now
- **Meters** (right): input and output peak levels.
- **Band panel** (bottom left): the selected band's controls, a level/gain meter and its transfer curve.
- **Global panel** (bottom right): processing mode, oversampling, expert mode, mix, output level and pan, the analyzer and the display range.

A new instance has no bands and passes audio through untouched. Double‑click the display to create the first band.

---

## 3. Working with bands

- **Create:** double‑click an empty spot in the display. The new band is one octave wide, centred where you clicked, and fits into the gap if neighbouring bands are close. Up to six bands.
- **Select:** click a band. Its controls appear in the band panel.
- **Move:** drag a band sideways.
- **Range:** drag a band up or down. Up makes the range positive (the band boosts); down makes it negative (the band cuts).
- **Resize:** drag a band's left or right edge.
  - An edge snaps to a neighbouring band's edge when you get close. From then on the two bands share that edge, and dragging it moves both, just like a crossover.
  - Drag an edge to the very end of the display and the band becomes a **shelf**: a low band that reaches all the way down, or a high band that reaches all the way up.
- **Threshold:** the mouse wheel over a band (Shift for fine steps).
- **Bypass:** double‑click a band.
- **Remove:** right‑click a band, press Delete with a band selected, or click **REMOVE**. Undo brings it back.

Bands never overlap. Where there's no band, the audio passes through untouched.

---

## 4. Compress or expand, down or up

Two controls decide what a band does: **MODE** (Compress or Expand) and the sign of **RANGE**.

| Mode | Range | What it does | Acts when the level is |
|---|---|---|---|
| Compress | negative | **Downward compression:** turns loud parts down | above the threshold |
| Compress | positive | **Upward compression:** brings quiet parts up, towards the threshold | below the threshold |
| Expand | negative | **Downward expansion (gating):** turns quiet parts further down | below the threshold |
| Expand | positive | **Upward expansion:** makes loud parts louder | above the threshold |

- **Ratio** sets how strongly the band acts:
  - compressing at 4:1, a level 20 dB past the threshold changes by 15 dB
  - expanding at 4:1, a level 10 dB past the threshold changes by 30 dB
- **Range** always caps the gain change. Range 0 switches the band's dynamics off; only its GAIN remains.

**Limiting and gating.** Turn RATIO all the way up to **Inf:1**:

![A band at Inf:1: limiting](../screenshot-limiter.png)

- **Limiter:** COMPRESS with a negative range. Nothing in the band passes the threshold. Use the fastest attack and 2–5 ms of LOOKAHEAD so the band is already down when a peak arrives. Within its band it holds the level to within about 1 dB of the threshold (dynamic phase: exactly). Energy outside the band, such as the click of a hard onset, passes untouched, as with any multiband processor.
- **Gate:** EXPAND with a negative range. The full range applies the moment the level drops below the threshold, and the band passes untouched above it. A fast attack opens it instantly.
- **How deep a band can cut** in LINEAR PHASE and MINIMUM PHASE depends on its SLOPE, because neighbouring frequencies leak through the band edges: about 32 dB at 24 dB/oct, the full 48 dB at 48 dB/oct. DYNAMIC PHASE reaches the full range at any slope.

See the *Three-Band Limiter* and *Multiband Drum Gate* presets.

The transfer curve in the band panel draws the band's input/output curve and names its behaviour underneath.

---

## 5. Band controls

| Control | Range | What it does |
|---|---|---|
| **THRESHOLD** | −80 – 0 dB | Where the band starts to act. |
| **RANGE** | −48 – +48 dB | The most gain change the band may apply. Negative cuts, positive boosts. |
| **RATIO** | 1:1 – Inf:1 | How strongly the band acts past the threshold. The top position is infinite: a limiter when compressing, a gate when expanding. |
| **KNEE** | 0 – 30 dB | How gradually the band eases in around the threshold. |
| **LOW / HIGH** | 10 Hz – 30 kHz | The band's edges. At the ends of the range the band becomes a shelf. |
| **ATTACK** | 0.01 – 250 ms | How fast the gain change moves away from 0 dB. |
| **RELEASE** | 5 ms – 5 s | How fast the gain change returns. |
| **LOOKAHEAD** | 0 – 20 ms | Lets the band react before a peak arrives. |
| **GAIN** | ±30 dB | A fixed boost or cut, on top of the dynamics. |
| **LINK** | 0 – 100% | How much left and right share the gain change. |
| **SLOPE** | 6 – 48 dB/oct | How steeply the band's edges fall away. |
| **COMPRESS / EXPAND** | | See section 4. |
| **SOLO** | | Listen to just this band's part of the spectrum. Solo several bands to hear them together. |
| **BYPASS** | | Switch the band off without removing it. It glides smoothly back to 0 dB. |

**How a band measures level.** Each band listens to its own frequencies (or a custom range, see Expert mode). Level is measured as a short‑term RMS, scaled so a steady sine reads its peak level. The measuring window adapts to the band: longer for low bands, shorter for high ones.

**Smart timing.**
- *Frequency‑dependent:* low bands are kept a little slower, and never faster than a quarter of a cycle, so fast settings don't distort the bass.
- *Program‑dependent release:* a short overshoot recovers quickly; sustained gain change releases at the set time. Fast transients don't pump, and long phrases stay smooth.

**Lookahead.** As soon as any band uses lookahead, the plugin adds 20 ms of latency, the maximum, and reports it to your host. You can then change any band's lookahead without the latency changing.

**The band meter** in the band panel:
- *LVL:* the level the band hears, with the threshold marker. Drag the marker to set the threshold.
- *GAIN:* the gain change right now. Red cuts, green boosts.

**The transfer curve** shows input level across and output level up. The dot shows where the band is right now. Drag sideways in it to move the threshold.

---

## 6. Processing modes

| Mode | Latency | How it works |
|---|---|---|
| **DYNAMIC PHASE** (default) | none | Each band is a shelving filter pair whose gain follows the compressor. A band changes the sound, including its phase, only while it applies gain. A band doing nothing is an exact wire. |
| **LINEAR PHASE** | 4096 samples (about 90 ms at 44.1/48 kHz) | Each band is a linear‑phase FIR filter. The processed bands are added back to the time‑aligned input, so nothing is phase‑shifted, however hard the bands work. |
| **MINIMUM PHASE** | none | Linkwitz‑Riley crossovers split the audio at every band edge, and each piece gets its band's gain. Tight band separation and zero latency. The crossovers shift the phase slightly all the time, like any classic multiband compressor. |

Notes:
- In minimum phase, slopes are rounded up to the crossover slopes the mode supports: 6 dB/oct (a first‑order split that adds no phase shift), 12, 24, 36 or 48 dB/oct. The SLOPE display shows the crossover actually in use.
- Changing mode, oversampling or the band layout fades the output out and back in over a few milliseconds to avoid clicks.
- At the same settings, linear and minimum phase reach slightly less gain change than dynamic phase in a narrow band. Their band shapes fall away more gently.

**Oversampling** (OFF, 2X, 4X) runs dynamic and minimum phase at a higher rate, with linear‑phase halfband filters. It helps:
- very fast gain changes, which create less aliasing
- bands near the top of the spectrum, whose filters keep their shape closer to Nyquist

It adds 32 (2×) or 48 (4×) samples of latency. Linear phase always runs at the base rate.

---

## 7. Expert mode

Click **EXPERT** in the global panel to show these band controls:

- **STEREO / MID / SIDE:** process both channels, only the centre of the stereo image (mid), or only its width (side). A mid or side band shows **M** or **S** in the display.
- **SIDECHAIN: INTERNAL / EXTERNAL.**
  - INTERNAL: the band listens to the plugin's input.
  - EXTERNAL: the band listens to the plugin's sidechain input. Route another track to it in your DAW. The band still listens only to its own frequencies (or its custom range) of the sidechain signal.
- **CUSTOM DETECTION** and **DET LOW / DET HIGH:** the band listens to a different frequency range from the one it processes. The range appears as a bar under the display; drag its ends, or drag the middle to move it.
  - Example: a 6–16 kHz band that listens to 40–120 Hz ducks the hi‑hats whenever the kick hits (the *Hats From Kick* preset).

---

## 8. Global controls

| Control | What it does |
|---|---|
| **PROCESSING MODE** | Dynamic, linear or minimum phase (section 6). |
| **OVERSAMPLING** | Off, 2× or 4× (section 6). |
| **EXPERT** | Shows the expert band controls (section 7). |
| **MIX** | 0–200%. The dry signal is time‑aligned. Below 100% blends in the dry signal (parallel processing); above 100% exaggerates the gain changes. |
| **OUTPUT** | Output level, ±36 dB. |
| **PAN** | Output balance. |
| **ANALYZER** | OFF, PRE (input), POST (output) or BOTH. |
| **FREEZE** | Holds the peak of the spectrum while audio plays, so you can see where the energy builds up. Switch it off to clear. |
| **DISPLAY** | Vertical range of the display: ±6, ±12 or ±30 dB. |

Expert, the analyzer settings and the display range are view settings: they are saved with your project, but presets and A/B don't change them.

---

## 9. The display and meters

- The frequency axis runs from 10 Hz to 30 kHz. Above half your sample rate, a band is treated as reaching to the top.
- The analyzer has a gentle 4.5 dB/oct tilt, so a balanced mix looks roughly flat.
- A ring around a band's range handle means the band is working right now: red for cutting, green for boosting.
- Click the level meters to reset the peak readouts.

---

## 10. Knobs

- **Drag** up or down; **Shift** for fine adjustment. **Grab the rim** of a knob to turn it in a circle.
- **Mouse wheel** turns a knob.
- **Double‑click** to type a value, for example `1.5k`, `-24`, `4` (for 4:1) or `250ms`.
- **Cmd/Ctrl‑click** resets to the default.

---

## 11. A/B, undo and redo

- **A / B** hold two complete settings. Click the other letter to switch; the first time, B starts as a copy of A.
- **COPY** copies the current setting into the other slot.
- Every edit is an undo step, including creating, moving and removing bands, preset loads and A/B switches.
  - Undo: Cmd/Ctrl‑Z
  - Redo: Cmd/Ctrl‑Shift‑Z or Cmd/Ctrl‑Y

---

## 12. MIDI

- **MIDI Learn:**
  1. Click **LEARN**.
  2. Click a control. Band controls are learned for the selected band.
  3. Move a knob or fader on your controller.
  4. Click **LEARN** again (or press Esc) to finish.

  While learning, right‑click a control to unassign it. Assignments are saved with your project.
- The **MIDI** button switches MIDI handling off, for hosts that send MIDI to every effect.

---

## 13. Presets

41 factory presets in seven groups, plus your own:

| Group | Presets |
|---|---|
| Mastering | Four Band Master, Gentle Glue, Tame Harshness, Low End Control, Air Lift, Mid/Side Master, Three-Band Limiter |
| Mix Bus | Bus Punch, De‑Mud, Upward Detail, Side Cleanup, Full Four‑Way |
| Drums | Kick Tight, Snare Snap, Room Tamer, Overheads Smooth, Drum Bus Weight, Kick Ducks Bass (SC), Multiband Drum Gate |
| Vocals | De‑Esser, Vocal Body, Breath Control, Presence Up, Plosive Guard |
| Bass & Keys | Bass Even, Sub Focus, Piano Tamer, Pad Widener, Guitar Fizz |
| Repair | Hum Gate, Hiss Gate, Resonance Snuff, Rumble Control, Sibilance (Side) |
| Creative | Pumping Lows, Transient Shaper, Reverse Dynamics, Spectral Gate, Side Bloom, Hats From Kick |

Presets are starting points. Thresholds depend on how loud your material is, so adjust THRESHOLD first. *Kick Ducks Bass (SC)* expects the kick on the sidechain input.

- **SAVE CURRENT AS…** stores your own presets, in one of these folders:
  - macOS: `~/Library/Audio/Presets/Epsilon Audio/Schist`
  - Windows: `%APPDATA%\Epsilon Audio\Schist\Presets`
  - Linux: `~/.config/epsilon-audio/schist/presets`
- To delete one of your presets, right‑click it in the browser and click again to confirm.

---

## 14. Help and hover tips

- Rest the pointer on any control, band, edge, handle or meter to see what it does and its current value.
- **HELP** opens the in‑plugin pages. The switch at the bottom of the help window turns tips on or off.
- **FULL MANUAL (WEB)** opens this manual.

---

## 15. Recipes

- **Master bus control:**
  1. Four bands snapped together, shelves at both ends.
  2. Gentle ratios (1.5–2.5:1), ranges of 3–6 dB, linear phase.
  3. Set each threshold so its band works only on the loudest moments (see *Four Band Master*).
- **De‑essing:** one band at 5.5–10 kHz, 24 dB/oct, fast attack, ratio 6:1 or more, range −10 dB, a little lookahead. For a whole mix, make it a side band to leave a centred vocal untouched.
- **Fix a boomy bass note:** a narrow band (48 dB/oct) around the note, ratio 6:1, range −12 dB. Only that note gets pulled down, only when it rings.
- **Bring out detail:** upward compression (Compress, positive range) at 1.5–12 kHz, threshold well below the material, ratio 2:1, range +4 to +6 dB.
- **Tighter drums:** downward expansion (Expand, negative range) on the low mids with a fast attack shortens the ring of a kick or toms.
- **Duck the bass with the kick:** a low band with SIDECHAIN EXTERNAL and the kick on the sidechain input (see *Kick Ducks Bass*).
- **Parallel multiband:** heavy settings, global MIX at 30–50%.
- **Multiband limiting:** three bands snapped together, COMPRESS at Inf:1, range −24 dB, fastest attack, 5 ms lookahead, thresholds just under the loudest peaks of each band (see *Three-Band Limiter*). Follow it with a true-peak limiter for the final ceiling.
- **Multiband gate:** EXPAND at Inf:1, range −30 to −40 dB, 48 dB/oct slopes, very fast attack, release 50–100 ms; set each band's threshold between its hits and its bleed (see *Multiband Drum Gate*).

---

## 16. Keyboard and mouse reference

| Action | How |
|---|---|
| Create a band | Double‑click an empty spot in the display |
| Select a band | Click it |
| Move a band | Drag it sideways |
| Band range | Drag the band up/down |
| Resize a band | Drag its edge (snaps to neighbours; to the end of the display for a shelf) |
| Threshold | Mouse wheel over the band; drag the meter marker; drag in the transfer curve |
| Bypass a band | Double‑click it |
| Remove a band | Right‑click it, Delete, or REMOVE |
| Move the detection range | Drag its ends or middle (expert mode, custom detection on) |
| Fine adjustment | Shift‑drag, Shift‑wheel |
| Type a value | Double‑click a knob |
| Reset | Cmd/Ctrl‑click a knob |
| Undo / redo | Cmd/Ctrl‑Z / Cmd/Ctrl‑Shift‑Z or Cmd/Ctrl‑Y |
| Close help or the preset menu, leave Learn, cancel typing | Esc |

---

## 17. Parameter reference

There are 136 parameters: 132 automatable controls and 4 view settings. New parameters are only ever added at the end, so sessions and automation keep working across versions.

- **Global:**
  - Processing Mode
  - Oversampling
  - Mix (0–200%)
  - Output Level (±36 dB)
  - Output Pan
  - MIDI
  - View settings (not automatable): Expert Mode, Analyzer, Analyzer Freeze, Display Range
- **Band 1–6** (21 each):
  - Enabled, Bypass, Solo
  - Low Frequency, High Frequency (10 Hz – 30 kHz), Slope (6–48 dB/oct)
  - Mode (Compress/Expand)
  - Threshold (−80 – 0 dB), Range (±48 dB), Ratio (1–100; 100 is Inf:1), Knee (0–30 dB)
  - Attack (0.01–250 ms), Release (5 ms – 5 s), Lookahead (0–20 ms)
  - Gain (±30 dB), Stereo Link (0–100%)
  - Stereo Mode (Stereo/Mid/Side), Sidechain (Internal/External)
  - Custom Detection, Detection Low, Detection High

---

## 18. Troubleshooting

- **Schist doesn't appear in my DAW.** Check the install folders above and rescan plugins. On macOS, run the `xattr` command from section 1.
- **A band does nothing.**
  - Check that RANGE isn't 0.
  - Check the threshold against the band's level meter. A downward compressor needs the level above the threshold; an upward compressor needs it below.
- **The external sidechain doesn't react.** Route a track to Schist's sidechain input in your DAW, and set the band's SIDECHAIN to EXTERNAL (expert mode).
- **The latency changed.** Linear phase adds 4096 samples; any lookahead adds 20 ms; oversampling adds 32–48 samples. Your host compensates automatically.
- **Upward compression lifts the noise.** That's what it does to anything below the threshold. Use a smaller positive range or a lower threshold.
- **CPU is high.** Six bands in minimum phase at 4× oversampling is the most expensive setting. Dynamic phase without oversampling is the lightest.
