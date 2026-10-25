# Cyber Acoustic Preamp v1.5.0
### Ultimate Fluid Space Acoustic Guitar Pickup Preamp & Cavity Physics
**Free & Open Source Standalone App, VST3, AU, and LV2 Plugin for Windows, macOS, and Linux.**

---

## What's New in v1.5.0
- **Expanded Fluid Reverb Engine**:
  - **Studio Room (Type 0)**: Natural, warm wooden room ambiance with intimate early reflections.
  - **Concert Hall (Type 1)**: Expansive, lush acoustic space ($5.10\times$ delay scaling, slower high-frequency absorption, increased pre-diffusion) for grand auditorium and cathedral acoustics.
  - **Vintage Plate (Type 2)**: Dense, high-frequency plate dispersion ($1.42\times$ time scale, $160\text{ Hz}$ high-pass conditioning, shimmering $14\text{ kHz}$ decay) for 1970s acoustic studio tracks.
- **F-Style Mandolin (F-5) Cavity Model**:
  - Mathematically derived for carved spruce top, flamed maple back/sides, twin f-holes, and floating ebony bridge.
  - Coupled Christensen parameters: Helmholtz air resonance $A_0 = 285\text{ Hz}$, soundboard mode $T_1 = 350\text{ Hz}$, shape factor $\sigma = 0.15$.
  - Includes new factory preset **Preset 10: Bluegrass F-5 Mandolin Chop**.
- **Level-Matched Master Bypass ("A/B MATCHED BYPASS")**:
  - True loudness-compensated comparison between processed acoustic tone and raw pickup signal. Continuously tracks dry and wet short-term RMS, eliminating loudness bias so you hear the pure tone improvement.
- **Input Gain Staging & RMS Sweet-Spot Meter**:
  - Smooth input gain knob ($-24\text{ dB} \dots +24\text{ dB}$).
  - Live dual-ballistic meter showing RMS bar and Peak needle with clear green target zone (`-24 dBFS` to `-12 dBFS`) indicating optimal converter headroom and fluid non-linearity drive.

---

## Overview
Acoustic guitar pickups (especially under-saddle piezo transducers) suffer from three notorious physical flaws:
1. **"Piezo Quack"**: Razor-sharp compressive force transients caused by the lack of physical bridge/soundboard mass inertia.
2. **"Transducer Fizz"**: Harsh, undamped mechanical contact vibrations between $4.5\text{ kHz}$ and $14\text{ kHz}$ that bypass the natural internal damping of spruce and cedar wood fibers.
3. **"Cardboard Boxiness"**: Trapped soundboard resonance from listening directly at the bridge saddle without exterior air propagation, phase-inverted soundhole cancellation, or boundary acoustic diffraction.

**Cyber Acoustic Preamp v1.5.0** resolves these mechanical discrepancies using physical fluid dynamics and exterior acoustic air radiation physics:
- **Fluid Air & Close-Mic Radiation Engine (`FluidAirMicDSP`)**:
  - **Tri-Point Wavefront Radiation Interference Matrix**: Models the acoustic interference between the Direct String Wave ($t_0$), Top-Plate Wood Wave ($\Delta t_1 \approx 0.28 - 0.63\text{ ms}$), and Soundhole Helmholtz Piston Wave ($\Delta t_2 \approx 0.85 - 1.60\text{ ms}$). Subtracts a portion of the delayed soundhole wave from the top plate wave ($-\gamma_{\text{hole}}$), physically cancelling the boxy $200 - 400\text{ Hz}$ "cardboard box" frequency region that plagues under-saddle pickups.
  - **Free-Air Particle Acceleration Sheen (`micAirSheen`)**: Computes the discrete second derivative of surface velocity ($A_{\text{air}}[n] = v[n] - v[n-1]$) with air viscosity low-pass damping, creating silky, uncompressed $10\text{ kHz}+$ condenser microphone breath that completely bypasses heavy soundboard wood damping.
  - **Close-Mic Pressure-Gradient Proximity Effect (`closeMicProximity`)**: Models the natural, chesty $90 - 130\text{ Hz}$ low-end warmth boost that occurs when a directional capsule is placed 3 to 6 inches from the 12th fret.
  - **Dual-Stage All-Pass Boundary Acoustic Diffraction**: Simulates the acoustic wavefront wrapping around the curved bout of the guitar body in 3D air space ($g_1 = 0.38 \cdot \text{openness}, g_2 = -0.24 \cdot \text{openness}$) with soft capsule diaphragm compliance.
  - **Constant-Time Padé Approximant (`FluidMath::fast_tanh`)**: Replaces standard `std::tanh` calls with a 3-cycle branchless rational approximant ($x(27 + x^2)/(27 + 9x^2)$), guaranteeing zero CPU spikes and zero latency degradation under any signal level.
- **10-Band Cyber-Denoiser PRO (`AcousticCyberDenoiser`)**: Zero-latency 10-band complementary filter bank ($60\text{ Hz}, 150\text{ Hz}, 400\text{ Hz}, 800\text{ Hz}, 1.5\text{ kHz}, 3\text{ kHz}, 5\text{ kHz}, 8\text{ kHz}, 12\text{ kHz}$) with perfect reconstruction. Continuously subtracts preamp hiss, cable buzz, and electromagnetic interference in parallel with a non-zero floor ($0.08$) that allows delicate acoustic note decays to sustain naturally into $-\infty\text{ dBFS}$ with zero gating, stuttering, or squelch. Features an intelligent Harmonic Shield protecting acoustic guitar fundamentals ($150 - 800\text{ Hz}$) and Listen Noise audition mode.
- **Mechanical Soundboard Inertia (`AcousticInertialCell`)**: Recreates the physical mass momentum ($F=ma$) and non-linear grain compliance sag of a vibrating spruce bridge, absorbing the quack into organic woody bloom.
- **2-DOF Coupled Fluid Cavity Engine (`FluidAcousticCavity`)**: Uses coupled Christensen differential equations (air soundhole Helmholtz $A_0$ + soundboard breathing mode $T_1$) with boundary shape factor ($\sigma$) and corner eddy vortices to reconstruct the 3D acoustic bloom of **9 classic instruments** (*Dreadnought, Jumbo, 000/OM, Grand Auditorium, Parlor, Classical Nylon, Acoustic Archtop, Selmer Maccaferri, and F-Style Mandolin*).
- **Studio 4-Band Parametric EQ (Active 3D Tone Matrix)**:
  - **Low Warmth Shelf**: $110\text{ Hz}, \pm 12\text{ dB}$, dialing in deep low-end body resonance or controlling stage boom.
  - **Low-Mid Boxiness Cut**: $200 - 1000\text{ Hz}, \pm 12\text{ dB}$, surgically scooping out the hollow "cardboard" honk typical of acoustic guitar pickups.
  - **High-Mid String Articulation**: $1.5 - 6.5\text{ kHz}, \pm 12\text{ dB}$, bringing out fingerpicking separation, pick definition, and acoustic chime.
  - **Air Glass Shelf**: $9.5\text{ kHz}, \pm 12\text{ dB}$, adding silky acoustic gloss and crystalline high-end sheen without harsh piezo brittle sizzle.
- **Pure Fluid Dynamics Reverb Space (`FluidReverbSpace`)**: Integrated from the Euphoria Amp engine with Abbey Road low/high-cut send filters, 12AT7 tube driver, dual allpass diffusers, 4x4 Hadamard feedback delay network (scaled along the 432 Hz natural period ladder), switchable Studio Room, Concert Hall, and Vintage Plate algorithms, and corner eddy boundary turbulence.
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
| **Linux** | Standalone, VST3, LV2 | x64 |

---

## Built-in Studio Acoustic Presets
1. **Dreadnought Studio Flatpicker (D-28)**: Deep booming bass, scooped low-mids, immense flatpicking projection.
2. **Modern Fingerstyle (000 / OM)**: Laser-focused mid-range clarity, tightened proximity, and fast transient articulation.
3. **Cavernous Strumming (J-200 Jumbo)**: Huge sub-harmonic punch ($88\text{ Hz}$), Concert Hall reverberation.
4. **Delta Blues Boxy Parlor (Single-0)**: Mid-forward boxy bark, close-mic percussive thumb snap.
5. **Concert Classical Warm Nylon**: Spanish fan-bracing compliance, gentle sheen, Concert Hall bloom.
6. **Acoustic Jazz Archtop**: High air velocity through carved f-holes, Vintage Plate shimmer.
7. **Gypsy Django Selmer**: Piercing oval-soundhole bite, Vintage Plate lead cut.
8. **Takamine Piezo Rescue**: Maximum soundhole/top-plate phase relief ($90\%$ Openness, $75\%$ Sheen) to completely eradicate plastic $300\text{ Hz}$ pickup boxiness.
9. **Martin D-45 Deep Air**: Full dreadnought chest weight ($80\%$ Proximity) with sparkling condenser breath ($85\%$ Sheen, $88\%$ Openness) in a Concert Hall.
10. **Bluegrass F-5 Mandolin Chop**: Fast percussive woody bark ($A_0 = 285\text{ Hz}$, $T_1 = 350\text{ Hz}$), high proximity, restrained highs to tame double-course pick strike harshness.

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
