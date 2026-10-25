# Cyber Acoustic Preamp
### Ultimate Fluid Space Acoustic Guitar Pickup Preamp & Cavity Physics
**Free & Open Source Standalone App, VST3, AU, and LV2 Plugin for Windows, macOS, and Linux.**

---

## Overview
Acoustic guitar pickups (especially under-saddle piezo transducers) suffer from two notorious physical flaws:
1. **"Piezo Quack"**: Razor-sharp compressive force transients caused by the lack of physical bridge/soundboard mass inertia.
2. **"Transducer Fizz"**: Harsh, undamped mechanical contact vibrations between $4.5\text{ kHz}$ and $14\text{ kHz}$ that bypass the natural internal damping of spruce and cedar wood fibers.

**Cyber Acoustic Preamp** resolves these mechanical discrepancies using physical fluid dynamics and acoustic cavity physics:
- **Mechanical Soundboard Inertia (`AcousticInertialCell`)**: Recreates the physical mass momentum ($F=ma$) and non-linear grain compliance sag of a vibrating spruce bridge, absorbing the quack into organic woody bloom.
- **2-DOF Coupled Fluid Cavity Engine (`FluidAcousticCavity`)**: Uses coupled Christensen differential equations (air soundhole Helmholtz $A_0$ + soundboard breathing mode $T_1$) with boundary shape factor ($\sigma$) and corner eddy vortices to reconstruct the 3D acoustic bloom of **8 classic guitar shapes** (*Dreadnought, Jumbo, 000/OM, Grand Auditorium, Parlor, Classical Nylon, Acoustic Archtop, and Selmer Maccaferri*).
- **High-Performance Low-Latency ASIO**: Standalone application on Windows includes native Steinberg ASIO driver backend (`JUCE_ASIO=1`) and WASAPI Exclusive support (`JUCE_WASAPI_EXCLUSIVE=1`) for sub-3ms ultra-low round-trip latency.
- **Dedicated Mono Input Routing**: Built-in 1-tap input selector with 5 modes (Mono In 1, Mono In 2, Auto-Detect Active, Stereo Sum, Stereo Discrete), accurately routing single-pickup guitars from any interface channel into the 3D stereo room imager without comb-filtering or volume drops.
- **Smart De-Fizz & Flexural Phase Smear (`SmartAcousticDeFizz`)**: A 4-stage allpass dispersion network emulates transverse wave propagation across wood grain ($c_{\text{flex}} \propto \sqrt{\omega}$), while a 4th-order (24 dB/oct) Linkwitz-Riley crossover dynamically clamps harsh contact fizz during pick strikes while retaining crystalline air shimmer on sustained chords.
- **Auto-Tracking Stage Feedback Killer (`AcousticFeedbackKiller`)**: Dual surgical $-\infty\text{ dB}$ SVF notch filters ($Q = 40 \dots 100$) lock onto drifting low-end stage resonance without thinning guitar tone.
- **Stereo Acoustic Imager**: Virtual studio microphone pair (12th-fret neck position vs. bridge soundhole position) with inter-aural time difference (ITD) and acoustic body shadowing.

---

## Supported Formats & Platforms

| Platform | Formats | Architectures |
| :--- | :--- | :--- |
| **Windows** | Standalone (.exe), VST3, LV2 | x64 |
| **macOS** | Standalone (.app), AU (.component), VST3, LV2 | Universal Binary (Apple Silicon arm64 + Intel x86_64) |
| **Linux** | Standalone, VST3, LV2 | x86_64 |

---

## 8 Built-in Acoustic Body Presets
1. **Dreadnought Studio Flatpicker (D-28)**: Deep booming bass, scooped low-mids, immense flatpicking projection.
2. **Modern Fingerstyle (000 / OM)**: Laser-focused mid-range clarity and fast transient articulation.
3. **Cavernous Strumming (J-200 Jumbo)**: Huge sub-harmonic punch ($88\text{ Hz}$), cavernous headroom.
4. **Delta Blues Boxy Parlor (Single-0)**: Mid-forward boxy bark, intimate percussive decay.
5. **Concert Classical Warm Nylon**: Spanish fan-bracing compliance and rich second-harmonic warmth.
6. **Acoustic Jazz Archtop**: High air velocity through carved f-holes, fast acoustic comping "chop".
7. **Gypsy Django Selmer**: Piercing oval-soundhole bite and stinging lead acoustic cut.
8. **Custom Physical Morphing**: Continuous Body Size ($0.5\times \dots 2.0\times$) and Shape ($\sigma$).

---

## Building from Source

### Prerequisites
- **CMake** 3.22 or higher
- C++20 compatible compiler (MSVC 2022 on Windows, Xcode/Clang on macOS, GCC 11+ on Linux)
- Git

### Build Instructions
```bash
git clone https://github.com/danny1marshall1587-maker/cyber-acoustic-preamp-vst.git
cd cyber-acoustic-preamp-vst
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j 4
```

The compiled Standalone application, VST3, LV2, and AU binaries will be generated inside `build/CyberAcousticPreamp_artefacts/Release/`.

---

## License
MIT License. Free for personal and commercial musical production.
Developed by Danny Marshall.
