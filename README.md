# ChromaCosm (VST3 & Standalone Audio Plugin)

Dual studio multi-effects processor combining the warm, vintage instability of the **Hologram Electronics Chroma Console** with the granular synthesis, micro-looping, and glitch multi-delays of the **Hologram Electronics Microcosm**.

Built in modern C++ with JUCE. Features an authentic hardware-inspired interface with interactive controls, real-time waveform & grain cloud visualizer, built-in guide, hover descriptions, and safe, anti-blowup DSP design.

---

## 🎨 Visual Interface & Features

- **Chroma Console (Left Pedal)**:
  - Freely re-orderable 4-module signal chain (`CHAR`, `MOVE`, `DIFF`, `TEXT`).
  - **4 Colored Modules (20 Algorithms)**:
    - 🔴 **CHARACTER**: Drive, Sweeten, Fuzz, Howl, Swell.
    - 🟡 **MOVEMENT**: Doubler, Vibrato, Phaser, Tremolo, Pitch (-12..+12 st).
    - 🟢 **DIFFUSION**: Cascade, Reels, Space, Collage, Reverse.
    - 🔵 **TEXTURE**: Filter (Morphing LPF/HPF), Squash, Cassette, Broken, Interference.
  - **Calibration**: Low, Med, High, Synth headroom matching.
  - **Capture**: Ephemeral loop sustainer.
  - **Secondary Drift & Effect Vol controls** per module.

- **Microcosm (Right Pedal)**:
  - **11 Effects across 4 Categories + 4 Preset Variations (A, B, C, D = 44 Algorithms)**:
    - 🌊 **Micro Loop**: Mosaic, Seq, Glide.
    - ☁️ **Granules**: Haze, Tunnel, Strum.
    - ⚡ **Glitch**: Blocks, Interrupt, Arp.
    - 🌌 **Multi-Delay**: Pattern, Warp.
  - **Secondary Parameters**: Pitch Modulation (Rate & Depth), 4 Space Reverb Modes (`Room`, `Hall`, `Cathedral`, `Shimmer Abyss`), Filter Resonance.
  - **Hold Sampler**: Infinite ambient freeze footswitch.
  - **40-Second Stereo Phrase Looper**: Rec/Play/Overdub, Stop, Clear, Speed (0.5x, 1x, 2x), Reverse, and Pre/Post FX routing.

- **Global Controls**:
  - **Combo Routing**:
    - `CHROMA -> MICROCOSM` (Console drive/tape into granular clouds)
    - `MICROCOSM -> CHROMA` (Granular clouds crushed & filtered by tape/fuzz)
    - `PARALLEL BLEND` (Both pedals processing in parallel)
  - **Interactive Guide (? GUIDA)**: 4-tab integrated manual inside the plugin.
  - **Hover Descriptions (INFO MOUSE)**: Toggleable real-time tooltip banner explaining every single knob, button, algorithm, and variation on hover.
  - **12 Factory Presets**: Curated ambient, shoegaze, lo-fi, and glitch combinations.
  - **Tap Tempo**: Manual tap override + automatic host DAW BPM tempo sync.
  - **Real-Time Visualizer**: Audio output oscilloscope + live granular clouds (spatial pan, amplitude, and pitch tinting).

---

## 🛡️ DSP Stability & Safety
- Built using **TPT (Topology-Preserving Transform) State-Variable Filters** that remain unconditionally stable across extreme audio-rate modulation.
- Internal saturation and `sanitizeAudio` guards on all feedback paths (`Repeats`, `Cascade`, `Reels`, `Space Reverb`, `Looper`). Pushing knobs to 100% will never produce `NaN`/`Inf` or silent freeze glitches.
- Soft-knee output ceiling protects your DAW master bus.

---

## 🚀 Building from Source

### Prerequisites
- **CMake** 3.22 or newer
- **Visual Studio 2022** (Windows) or **Xcode / Clang** (macOS)
- **JUCE** framework

### Build Instructions
```bash
git clone https://github.com/DaMa-sys/ChromaCosm.git
cd ChromaCosm
cmake -B build -S .
cmake --build build --config Release
```

The compiled artefacts will be located in:
- `build/ChromaCosm_artefacts/Release/VST3/ChromaCosm.vst3`
- `build/ChromaCosm_artefacts/Release/Standalone/ChromaCosm.exe`

---

## 📄 Legal Disclaimer & Trademarks

**DaMA ChromaCosm** is an independent, non-commercial open-source audio research project developed by **DaMA Audio**.

- All product names, logos, and brands mentioned (including *Hologram Electronics*, *Chroma Console*, and *Microcosm*) are the registered trademarks and intellectual property of their respective owners.
- Their mention here is purely descriptive and for comparative/educational reference regarding audio topology and workflow inspiration.
- This software is NOT endorsed, sponsored, affiliated with, or produced by Hologram Electronics.
- All algorithms are original C++/JUCE DSP implementations.

Licensed under the MIT License.
