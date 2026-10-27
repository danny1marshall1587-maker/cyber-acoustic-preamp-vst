#pragma once

#include <cmath>
#include <algorithm>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace FluidMath {
    // Constant-Time Rational Padé Approximant (~3 cycles, branchless, zero CPU spikes)
    inline float fast_tanh(float x) noexcept {
        x = std::clamp(x, -3.0f, 3.0f);
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    inline float fast_sin(float x) noexcept {
        return std::sin(x);
    }
}

namespace CyberAcoustic {

inline float flush_denormal(float v) {
    if (std::abs(v) < 1.0e-15f) return 0.0f;
    return v;
}

inline float bounded_tanh(float x) {
    return FluidMath::fast_tanh(x);
}

// ============================================================================
// Stage 0: 10-Band Cyber-Denoiser PRO (Spectral Phase Subtraction)
// ============================================================================
class AcousticCyberDenoiser {
public:
    static constexpr int NUM_BANDS = 10;

private:
    float sample_rate = 48000.0f;
    float mAttCoeff = 0.0f;
    float mLevelAtt = 0.0f;
    float mLevelRel = 0.0f;
    float mHpF = 0.0f;
    float mLpF_cut = 0.0f;
    float mBCoeffs[9] = {};

    float lpStates[9] = {};
    float levelStates[NUM_BANDS] = {};
    float currentGains[NUM_BANDS] = {};
    float hpCutStat = 0.0f;
    float lpCutStat = 0.0f;

    float lastThreshDb = -999.0f;
    float cachedLinThresh[NUM_BANDS] = {};
    float floorVal = 0.08f; // Non-zero floor prevents gating on acoustic decays

public:
    AcousticCyberDenoiser() {
        for (int i = 0; i < NUM_BANDS; ++i) {
            currentGains[i] = 1.0f;
        }
    }

    void init(float sr) {
        sample_rate = sr > 0.0f ? sr : 48000.0f;
        mAttCoeff = 1.0f - std::exp(-1.0f / (0.1f * sample_rate / 1000.0f));
        mLevelAtt = 1.0f - std::exp(-1.0f / (2.0f * sample_rate / 1000.0f));
        mLevelRel = 1.0f - std::exp(-1.0f / (60.0f * sample_rate / 1000.0f));

        auto getCoeff = [this](float freq) {
            return 1.0f - std::exp(-2.0f * (float)M_PI * freq / std::max(1.0f, sample_rate));
        };

        mHpF = getCoeff(40.0f);
        mLpF_cut = getCoeff(18000.0f);

        const float multi[9] = { 60.0f, 150.0f, 400.0f, 800.0f, 1500.0f, 3000.0f, 5000.0f, 8000.0f, 12000.0f };
        for (int i = 0; i < 9; ++i) {
            mBCoeffs[i] = std::min(0.99f, getCoeff(multi[i]));
        }

        reset();
    }

    void reset() {
        for (int i = 0; i < 9; ++i) lpStates[i] = 0.0f;
        for (int i = 0; i < NUM_BANDS; ++i) {
            levelStates[i] = 0.0f;
            currentGains[i] = 1.0f;
        }
        hpCutStat = 0.0f;
        lpCutStat = 0.0f;
        lastThreshDb = -999.0f;
    }

    inline float process(float inSample, float reductionNorm, float thresholdDb,
                         bool harmonicShield, bool listenNoise) {
        if (reductionNorm <= 0.001f && !listenNoise) {
            return inSample;
        }

        // 1. High-Pass (40 Hz) and Low-Pass (18 kHz) DC/Sub & Ultrasonic conditioning
        hpCutStat += (inSample - hpCutStat) * mHpF;
        float s0 = inSample - hpCutStat;
        lpCutStat += (s0 - lpCutStat) * mLpF_cut;
        s0 = lpCutStat;

        // 2. 10-Band Perfect Reconstruction Filter Bank
        float b[NUM_BANDS];
        float currentIn = s0;

        lpStates[0] += (currentIn - lpStates[0]) * mBCoeffs[0];
        b[0] = lpStates[0];

        for (int i = 1; i < 9; ++i) {
            lpStates[i] += (currentIn - lpStates[i]) * mBCoeffs[i];
            b[i] = lpStates[i] - lpStates[i - 1];
        }
        b[9] = currentIn - lpStates[8];

        // 3. Update Cached Linear Thresholds (Only when parameter changes)
        if (std::abs(thresholdDb - lastThreshDb) > 0.05f) {
            lastThreshDb = thresholdDb;
            // Shaped threshold profile matching guitar pickup noise spectrum
            static const float bandOffsets[NUM_BANDS] = {
                -2.0f, 0.0f, +1.0f, +2.0f, +2.5f, +3.0f, +3.5f, +4.0f, +5.0f, +4.0f
            };
            for (int i = 0; i < NUM_BANDS; ++i) {
                float db = std::clamp(thresholdDb + bandOffsets[i], -100.0f, -20.0f);
                cachedLinThresh[i] = std::pow(10.0f, db / 20.0f);
            }
        }

        // 4. Harmonic Shield for Acoustic Guitar Fundamentals
        float fundamentalActivity = 0.0f;
        if (harmonicShield) {
            for (int fBand = 1; fBand <= 3; ++fBand) {
                float rf = levelStates[fBand];
                float rawG = 1.0f - (cachedLinThresh[fBand] / (rf + 1e-9f));
                fundamentalActivity = std::max(fundamentalActivity, std::clamp(rawG, 0.0f, 1.0f));
            }
        }

        // 5. Per-Band Spectral Subtraction
        float outSum = 0.0f;
        for (int i = 0; i < NUM_BANDS; ++i) {
            float absVal = std::abs(b[i]);
            float coeff = (absVal > levelStates[i]) ? mLevelAtt : mLevelRel;
            levelStates[i] += (absVal - levelStates[i]) * coeff;
            float r = levelStates[i];

            float effectiveThresh = cachedLinThresh[i];
            if (harmonicShield && i >= 4) {
                effectiveThresh *= (1.0f - 0.60f * fundamentalActivity);
            }

            // Continuous subtractive gain formula
            float targetG = 1.0f - (effectiveThresh / (r + 1e-9f));
            targetG = std::max(floorVal, std::min(1.0f, targetG));

            float scaledTargetG = 1.0f - reductionNorm * (1.0f - targetG);

            float currentCoeff;
            if (scaledTargetG > currentGains[i]) {
                currentCoeff = mAttCoeff;
            } else {
                float tailDist = (currentGains[i] - floorVal) / (1.01f - floorVal);
                float tailStab = 1.0f + (1.0f - std::min(1.0f, std::max(0.0f, tailDist))) * 8.0f;
                currentCoeff = mLevelRel / tailStab;
            }
            currentGains[i] += (scaledTargetG - currentGains[i]) * currentCoeff;

            outSum += b[i] * currentGains[i];
        }

        if (listenNoise) {
            return s0 - outSum;
        }

        return flush_denormal(outSum);
    }
};

// ============================================================================
// Stage 1: Mechanical Soundboard Inertia & Anti-Quack Cell
// ============================================================================
class AcousticInertialCell {
private:
    float x_prev;
    float momentum;
    float sag_envelope;
    float sample_rate;

public:
    AcousticInertialCell() : x_prev(0.0f), momentum(0.0f), 
                             sag_envelope(0.0f), sample_rate(48000.0f) {}

    void init(float sr) {
        sample_rate = sr;
        reset();
    }

    void reset() {
        x_prev = 0.0f;
        momentum = 0.0f;
        sag_envelope = 0.0f;
    }

    inline float process(float in_sample, float quack_reduction, float wood_density) {
        if (quack_reduction <= 0.001f) return in_sample;

        // Step 1: Bounded input to prevent non-linear overflow
        float x = bounded_tanh(in_sample);

        // Step 2: Instantaneous acoustic velocity
        float v_str = x - x_prev;
        x_prev = x;

        // Step 3: Wood mass viscosity adaptation
        float eta_top = std::clamp(0.78f + 0.14f * wood_density, 0.65f, 0.96f);
        momentum = momentum * eta_top + v_str * (1.0f - eta_top);
        momentum = std::clamp(momentum, -1.0f, 1.0f);

        // Step 4: Saddle compressive compliance (sag)
        float pluck_energy = std::abs(x) * (1.0f + 3.0f * quack_reduction);
        float c_wood = (pluck_energy > sag_envelope) ? 0.0035f : 0.00032f;
        sag_envelope += c_wood * (pluck_energy - sag_envelope);
        float comp_factor = 1.0f - std::clamp(sag_envelope * 0.75f * quack_reduction, 0.0f, 0.32f);

        // Step 5: Asymmetric wood grain transfer
        float u = bounded_tanh((x - 0.40f * v_str * (1.0f - eta_top)) * comp_factor);
        float y_wood = 0.0f;
        float bloom = 0.16f * wood_density;

        if (u >= 0.0f) {
            y_wood = u - bloom * u * u * (1.0f - u);
        } else {
            y_wood = u + bloom * std::pow(std::abs(u), 1.85f) * (1.0f - std::abs(u));
        }

        // Re-inject mechanical inertia
        float wet = bounded_tanh(y_wood + bounded_tanh(momentum) * 0.35f * quack_reduction);
        float out = in_sample * (1.0f - quack_reduction) + wet * quack_reduction;
        return flush_denormal(out);
    }
};

// ============================================================================
// Stage 2: 2-DOF Coupled Fluid Cavity Engine (Soundboard T1 + Helmholtz A0)
// ============================================================================
class FluidAcousticCavity {
private:
    float sample_rate;
    // 2-DOF state variables: Soundboard top (t) and soundhole air (a)
    float s1_t, s2_t;
    float s1_a, s2_a;
    float eddy_state;

    // Frequencies & parameters
    float f_A0, f_T1;
    float shape_sigma;

public:
    FluidAcousticCavity() : sample_rate(48000.0f), s1_t(0), s2_t(0), 
                            s1_a(0), s2_a(0), eddy_state(0),
                            f_A0(100.0f), f_T1(205.0f), shape_sigma(0.5f) {}

    void init(float sr) {
        sample_rate = sr;
        reset();
    }

    void reset() {
        s1_t = s2_t = s1_a = s2_a = 0.0f;
        eddy_state = 0.0f;
    }

    void setBodyGeometry(int body_type_preset, float custom_size, float custom_shape) {
        // Presets: 0=Dreadnought, 1=Jumbo, 2=OM/000, 3=Grand Auditorium,
        //          4=Parlor, 5=Classical, 6=Archtop, 7=Selmer
        float base_A0 = 100.0f;
        float base_T1 = 205.0f;
        float base_sigma = 0.5f;

        switch (body_type_preset) {
            case 0: base_A0 = 98.0f;  base_T1 = 195.0f; base_sigma = 0.82f; break; // Dreadnought
            case 1: base_A0 = 88.0f;  base_T1 = 182.0f; base_sigma = 0.65f; break; // Jumbo
            case 2: base_A0 = 108.0f; base_T1 = 210.0f; base_sigma = 0.42f; break; // OM / 000
            case 3: base_A0 = 102.0f; base_T1 = 202.0f; base_sigma = 0.55f; break; // Grand Auditorium
            case 4: base_A0 = 132.0f; base_T1 = 238.0f; base_sigma = 0.25f; break; // Parlor
            case 5: base_A0 = 102.0f; base_T1 = 218.0f; base_sigma = 0.35f; break; // Classical
            case 6: base_A0 = 138.0f; base_T1 = 225.0f; base_sigma = 0.18f; break; // Archtop
            case 7: base_A0 = 118.0f; base_T1 = 215.0f; base_sigma = 0.12f; break; // Selmer
            case 8: base_A0 = 285.0f; base_T1 = 350.0f; base_sigma = 0.15f; break; // F-Style Mandolin (F-5)
            default: break;
        }

        // Apply custom size scaling
        float size_multiplier = std::clamp(custom_size, 0.5f, 2.0f);
        f_A0 = base_A0 / std::sqrt(size_multiplier);
        f_T1 = base_T1 / std::sqrt(size_multiplier * 0.85f);
        shape_sigma = std::clamp(base_sigma * custom_shape, 0.0f, 1.0f);
    }

    inline float process(float in_sample, float body_depth_blend) {
        if (body_depth_blend <= 0.001f) return in_sample;

        // Cytomic SVF parameters for Helmholtz Mode (A0)
        float w_a = (float)(M_PI * f_A0 / sample_rate);
        float g_a = std::tan(w_a);
        float k_a = 1.0f / 12.0f; // Q ~= 12.0 for resonant air cavity

        // Cytomic SVF parameters for Soundboard Mode (T1)
        float w_t = (float)(M_PI * f_T1 / sample_rate);
        float g_t = std::tan(w_t);
        float k_t = 1.0f / 8.0f;  // Q ~= 8.0 for wood plate

        // Corner eddy turbulence from fluid cavity boundaries
        float tau_vortex = (in_sample * in_sample * in_sample - 0.35f * in_sample) * shape_sigma;
        eddy_state = 0.80f * eddy_state + 0.20f * tau_vortex;

        // Coupled Christensen excitation
        float input_with_eddies = in_sample + 0.25f * eddy_state;

        // Solve T1 soundboard mode
        float hp_t = (input_with_eddies - (k_t + g_t) * s1_t - s2_t) / (1.0f + g_t * (k_t + g_t));
        float bp_t = g_t * hp_t + s1_t;
        s1_t = g_t * hp_t + bp_t;
        float lp_t = g_t * bp_t + s2_t;
        s2_t = g_t * bp_t + lp_t;

        // Cross-couple soundboard displacement into soundhole air cavity
        float air_excitation = bp_t * 0.65f;
        float hp_a = (air_excitation - (k_a + g_a) * s1_a - s2_a) / (1.0f + g_a * (k_a + g_a));
        float bp_a = g_a * hp_a + s1_a;
        s1_a = g_a * hp_a + bp_a;
        float lp_a = g_a * bp_a + s2_a;
        s2_a = g_a * bp_a + lp_a;

        // Acoustically coupled output (Air + Soundboard - Dip anti-resonance)
        float coupled_cavity = bp_t * 0.70f + bp_a * 0.85f - (lp_t - lp_a) * 0.30f;
        float out = in_sample * (1.0f - body_depth_blend) + coupled_cavity * body_depth_blend * 1.35f;

        return flush_denormal(out);
    }
};

// ============================================================================
// Stage 2B: Fluid Air & Close-Mic Radiation Engine (Simulates Close Mic vs Pickup)
// ============================================================================
class FluidAirMicDSP {
private:
    static constexpr int kMaxAirDelay = 1024; // Power-of-two buffer; supports up to 192kHz path diff
    float bodyWaveDelayLine[kMaxAirDelay] = {0.0f};
    float holeWaveDelayLine[kMaxAirDelay] = {0.0f};
    int writeIdx = 0;

    // Exterior Fluid State Variables
    float prevInputPressure = 0.0f;
    float prevSurfaceVelocity = 0.0f;
    float airSheenState = 0.0f;
    float helmholtzLowpass = 0.0f;
    float proximitySubState = 0.0f;

    // All-Pass Acoustic Diffraction States (Simulates wave wrapping around guitar bout)
    float diffX1 = 0.0f, diffY1 = 0.0f;
    float diffX2 = 0.0f, diffY2 = 0.0f;

    float sampleRate = 48000.0f;

public:
    FluidAirMicDSP() {
        reset();
    }

    void init(float sr) noexcept {
        sampleRate = sr > 0.0f ? sr : 48000.0f;
        reset();
    }

    void reset() noexcept {
        std::fill(std::begin(bodyWaveDelayLine), std::end(bodyWaveDelayLine), 0.0f);
        std::fill(std::begin(holeWaveDelayLine), std::end(holeWaveDelayLine), 0.0f);
        writeIdx = 0;
        prevInputPressure = 0.0f;
        prevSurfaceVelocity = 0.0f;
        airSheenState = 0.0f;
        helmholtzLowpass = 0.0f;
        proximitySubState = 0.0f;
        diffX1 = diffY1 = diffX2 = diffY2 = 0.0f;
    }

    // Call right after Acoustic Body Model!
    // rawPiezoOrMic:  The pre-body signal (captures fast string transients before body damping)
    // bodyResonance:  The output of Fluid Body Model
    // openness:       0.0f (Inside the box/pickup) -> 1.0f (Wide open 3D mic in front of guitar)
    // micAirSheen:    0.0f (Dark/Dynamic Mic)      -> 1.0f (Crisp Condenser Mic top-end air)
    // closeProximity: 0.0f (16" Ambient Mic)       -> 1.0f (3" Close-Mic at 12th Fret)
    inline float processSample(float rawPiezoOrMic, float bodyResonance,
                               float openness = 0.80f,
                               float micAirSheen = 0.65f,
                               float closeProximity = 0.70f) noexcept
    {
        if (openness <= 0.001f && micAirSheen <= 0.001f && closeProximity <= 0.001f) {
            return bodyResonance;
        }

        // 1. EXTRACT SOUNDHOLE HELMHOLTZ PISTON (Low-frequency air pump < 220Hz)
        helmholtzLowpass += 0.028f * (bodyResonance - helmholtzLowpass);

        // Write Top-Plate and Soundhole waves into exterior air propagation buffer
        bodyWaveDelayLine[writeIdx] = bodyResonance;
        holeWaveDelayLine[writeIdx] = helmholtzLowpass;

        // 2. TRI-POINT WAVEFRONT GEOMETRY (Neck/String vs. Top Plate vs. Soundhole)
        // As closeProximity increases (closer mic), the angle between the 12th fret,
        // the bridge plate, and the soundhole widens, creating rich acoustic comb-relief
        // that carves out the "boxy" 250Hz-400Hz piezo honk naturally!
        const float distanceScale = 1.0f - (closeProximity * 0.55f);
        const int plateDelaySamples = std::clamp(static_cast<int>((0.00028f + openness * 0.00035f) * distanceScale * sampleRate), 1, kMaxAirDelay - 1);
        const int holeDelaySamples  = std::clamp(static_cast<int>((0.00085f + openness * 0.00075f) * distanceScale * sampleRate), 2, kMaxAirDelay - 1);

        const int plateReadIdx = (writeIdx - plateDelaySamples + kMaxAirDelay) & (kMaxAirDelay - 1);
        const int holeReadIdx  = (writeIdx - holeDelaySamples  + kMaxAirDelay) & (kMaxAirDelay - 1);

        const float delayedPlateWave = bodyWaveDelayLine[plateReadIdx];
        const float delayedHoleWave  = holeWaveDelayLine[holeReadIdx];

        writeIdx = (writeIdx + 1) & (kMaxAirDelay - 1);

        // 3. FREE-AIR PARTICLE ACCELERATION (The "Un-Boxed" Condenser Air Sheen)
        // Piezo quack lives in the midrange (800Hz-2.5kHz). True mic "air" lives in 
        // the 2nd derivative (acceleration) of the acoustic boundary above 7kHz, 
        // smoothed slightly by air viscosity so it shimmers instead of clicking.
        const float surfaceVelocity = rawPiezoOrMic - prevInputPressure;
        prevInputPressure = rawPiezoOrMic;

        const float airAcceleration = surfaceVelocity - prevSurfaceVelocity;
        prevSurfaceVelocity = surfaceVelocity;

        // Air viscosity low-pass on the acceleration creates velvety 10kHz+ condenser sheen
        airSheenState = (airSheenState * 0.45f) + (airAcceleration * 0.55f);
        const float condenserAirWave = FluidMath::fast_tanh(airSheenState * 2.2f) * micAirSheen * 0.55f;

        // 4. CLOSE-MIC CAPSULE PROXIMITY EFFECT (Pressure-Gradient Low-End Weight)
        // A directional mic placed 3-6 inches from the guitar gets a natural, chesty 
        // 90Hz-130Hz warmth boost that moves with air velocity.
        proximitySubState += 0.012f * (delayedPlateWave - proximitySubState);
        const float proximityWarmth = proximitySubState * closeProximity * 0.65f;

        // 5. EXTERIOR ACOUSTIC INTERFERENCE (Combining the 3 Wavefronts in Air)
        // Notice we SUBTRACT a portion of the delayed soundhole wave from the body wave.
        // In real life, this phase cancellation between the soundhole and top plate is 
        // what makes an acoustic guitar sound "open and woody" instead of "trapped in a box"!
        const float openAirInterference = (delayedPlateWave * 0.85f) 
                                        - (delayedHoleWave * 0.50f * openness) 
                                        + proximityWarmth;

        // Blend between raw boxed body (openness = 0) and 3D exterior wave (openness = 1)
        float exteriorField = (bodyResonance * (1.0f - openness * 0.65f)) 
                            + (openAirInterference * openness) 
                            + condenserAirWave;

        // 6. Dual-Stage All-Pass Boundary Diffraction (Gives 3D "Space Around the Mic")
        const float g1 = 0.38f * openness;
        const float g2 = -0.24f * openness;

        const float ap1 = -g1 * exteriorField + diffX1 + g1 * diffY1;
        diffX1 = exteriorField;
        diffY1 = ap1;

        const float ap2 = -g2 * ap1 + diffX2 + g2 * diffY2;
        diffX2 = ap1;
        diffY2 = ap2;

        // Final gentle capsule diaphragm compliance (prevents harsh transient spikes)
        return FluidMath::fast_tanh(ap2 * 1.15f);
    }
};

// ============================================================================
// Stage 3: Zero-Fizz Subtractive High-Frequency Engine
// ============================================================================
class SmartAcousticDeFizz {
private:
    float sample_rate;
    // 4-stage allpass phase smear states
    float ap_x1[4], ap_x2[4], ap_y1[4], ap_y2[4];
    float ap_a1[4], ap_a2[4];

    // 4th-order Linkwitz-Riley split states (3.8 kHz)
    float lp1_x1, lp1_x2, lp1_y1, lp1_y2;
    float lp2_x1, lp2_x2, lp2_y1, lp2_y2;
    float lr_b0, lr_b1, lr_b2, lr_a1, lr_a2;

    // Transient envelope followers
    float env_high, env_body;

public:
    SmartAcousticDeFizz() : sample_rate(48000.0f), env_high(0), env_body(0) {
        reset();
    }

    void init(float sr) {
        sample_rate = sr;
        reset();
        setupFilters();
    }

    void reset() {
        std::memset(ap_x1, 0, sizeof(ap_x1));
        std::memset(ap_x2, 0, sizeof(ap_x2));
        std::memset(ap_y1, 0, sizeof(ap_y1));
        std::memset(ap_y2, 0, sizeof(ap_y2));
        lp1_x1 = lp1_x2 = lp1_y1 = lp1_y2 = 0.0f;
        lp2_x1 = lp2_x2 = lp2_y1 = lp2_y2 = 0.0f;
        env_high = env_body = 0.0f;
    }

    void setupFilters() {
        // 4-stage allpass phase smear (simulates transverse dispersion in spruce)
        float ap_freqs[4] = { 3400.0f, 5200.0f, 7800.0f, 11200.0f };
        float ap_q[4]     = { 1.2f,    1.4f,    1.6f,    1.8f };

        for (int i = 0; i < 4; ++i) {
            float w0 = (float)(2.0 * M_PI * ap_freqs[i] / sample_rate);
            float alpha = std::sin(w0) / (2.0f * ap_q[i]);
            float a0 = 1.0f + alpha;
            ap_a1[i] = (-2.0f * std::cos(w0)) / a0;
            ap_a2[i] = (1.0f - alpha) / a0;
        }

        // 4th-order Linkwitz-Riley crossover coefficients (fc = 3800 Hz)
        float fc = 3800.0f;
        float w0 = (float)(2.0 * M_PI * fc / sample_rate);
        float cosw = std::cos(w0);
        float sinw = std::sin(w0);
        float alpha = sinw / 1.41421356f; // Q = 0.7071 (Butterworth)
        float a0 = 1.0f + alpha;

        lr_b0 = ((1.0f - cosw) * 0.5f) / a0;
        lr_b1 = (1.0f - cosw) / a0;
        lr_b2 = ((1.0f - cosw) * 0.5f) / a0;
        lr_a1 = (-2.0f * cosw) / a0;
        lr_a2 = (1.0f - alpha) / a0;
    }

    inline float process(float in_sample, float defizz_intensity, float air_shimmer) {
        if (defizz_intensity <= 0.001f && air_shimmer <= 0.001f) return in_sample;

        // Step 1: Allpass Phase Smear (softens crystalline piezo spike)
        float smeared = in_sample;
        for (int i = 0; i < 4; ++i) {
            float y = ap_a2[i] * smeared + ap_a1[i] * ap_x1[i] + ap_x2[i]
                      - ap_a1[i] * ap_y1[i] - ap_a2[i] * ap_y2[i];
            ap_x2[i] = ap_x1[i]; ap_x1[i] = smeared;
            ap_y2[i] = ap_y1[i]; ap_y1[i] = y;
            smeared = y;
        }

        // Step 2: Cascaded Linkwitz-Riley low-pass and high-pass
        float lp1 = lr_b0 * smeared + lr_b1 * lp1_x1 + lr_b2 * lp1_x2 - lr_a1 * lp1_y1 - lr_a2 * lp1_y2;
        lp1_x2 = lp1_x1; lp1_x1 = smeared; lp1_y2 = lp1_y1; lp1_y1 = lp1;

        float x_low = lr_b0 * lp1 + lr_b1 * lp2_x1 + lr_b2 * lp2_x2 - lr_a1 * lp2_y1 - lr_a2 * lp2_y2;
        lp2_x2 = lp2_x1; lp2_x1 = lp1; lp2_y2 = lp2_y1; lp2_y1 = x_low;

        // HP Stage via exact subtraction (perfect reconstruction)
        float x_high = smeared - x_low;

        // Step 3: Transient Fizz Detection
        float high_mag = std::abs(x_high);
        float low_mag  = std::abs(x_low);
        env_high += ((high_mag > env_high) ? 0.05f : 0.001f) * (high_mag - env_high);
        env_body += ((low_mag  > env_body) ? 0.02f : 0.0005f) * (low_mag  - env_body);

        float fizz_ratio = env_high / (env_body + 1.0e-5f);
        float fizz_clamp = 1.0f;

        if (fizz_ratio > 0.35f && defizz_intensity > 0.01f) {
            float excess = (fizz_ratio - 0.35f) / fizz_ratio;
            fizz_clamp = 1.0f - std::clamp(excess * defizz_intensity * 1.5f, 0.0f, 0.70f);
        }

        // Apply dynamic defizz + acoustic air shimmer
        float conditioned_high = x_high * fizz_clamp * (1.0f + air_shimmer * 0.45f);

        // Reconstruct full audio spectrum
        float out = x_low + conditioned_high;
        return flush_denormal(out);
    }
};

// ============================================================================
// Stage 4: Auto-Tracking Surgical Feedback Killer (AFD Pro Acoustic)
// ============================================================================
class AcousticFeedbackKiller {
private:
    float sample_rate;
    float s1_n1, s2_n1;
    float s1_n2, s2_n2;
    float freq_n1, freq_n2;
    float depth_n1, depth_n2; // 0.0 (bypass) to 1.0 (-inf dB complete null)
    bool enabled_n1, enabled_n2;

public:
    AcousticFeedbackKiller() : sample_rate(48000.0f),
                               s1_n1(0), s2_n1(0), s1_n2(0), s2_n2(0),
                               freq_n1(98.0f), freq_n2(195.0f),
                               depth_n1(0.75f), depth_n2(0.75f),
                               enabled_n1(false), enabled_n2(false) {}

    void init(float sr) {
        sample_rate = sr;
        reset();
    }

    void reset() {
        s1_n1 = s2_n1 = s1_n2 = s2_n2 = 0.0f;
    }

    void setNotch(int band, bool enable, float freq, float depth_norm) {
        if (band == 0) {
            enabled_n1 = enable;
            freq_n1 = std::clamp(freq, 70.0f, 160.0f);
            depth_n1 = std::clamp(depth_norm, 0.0f, 1.0f);
        } else {
            enabled_n2 = enable;
            freq_n2 = std::clamp(freq, 160.0f, 320.0f);
            depth_n2 = std::clamp(depth_norm, 0.0f, 1.0f);
        }
    }

    inline float process(float in_sample) {
        float out = in_sample;

        // Band 1: Helmholtz A0 Surgical Null (Q = 60)
        if (enabled_n1 && depth_n1 > 0.001f) {
            float w = (float)(M_PI * freq_n1 / sample_rate);
            float g = std::tan(w);
            float k = 1.0f / 60.0f;
            float hp = (out - (k + g) * s1_n1 - s2_n1) / (1.0f + g * (k + g));
            float bp = g * hp + s1_n1;
            s1_n1 = g * hp + bp;
            float lp = g * bp + s2_n1;
            s2_n1 = g * bp + lp;
            float notch = hp + lp; // Complete -inf dB null at center
            out = out * (1.0f - depth_n1) + notch * depth_n1;
        }

        // Band 2: Soundboard T1 Surgical Null (Q = 45)
        if (enabled_n2 && depth_n2 > 0.001f) {
            float w = (float)(M_PI * freq_n2 / sample_rate);
            float g = std::tan(w);
            float k = 1.0f / 45.0f;
            float hp = (out - (k + g) * s1_n2 - s2_n2) / (1.0f + g * (k + g));
            float bp = g * hp + s1_n2;
            s1_n2 = g * hp + bp;
            float lp = g * bp + s2_n2;
            s2_n2 = g * bp + lp;
            float notch = hp + lp;
            out = out * (1.0f - depth_n2) + notch * depth_n2;
        }

        return flush_denormal(out);
    }
};

// ============================================================================
// Stage 5: Fluid Biquad & Pure Fluid Dynamics Reverb Space (From Euphoria Amp)
// ============================================================================
struct FluidBiquad {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    float s1 = 0.0f, s2 = 0.0f;

    inline float process(float in) {
        float out = b0 * in + s1;
        s1 = b1 * in - a1 * out + s2;
        s2 = b2 * in - a2 * out;
        return out;
    }

    void reset() { s1 = s2 = 0.0f; }

    void setLowPass(float fc, float Q, float sr) {
        fc = std::max(20.0f, std::min(sr * 0.45f, fc));
        float omega = 2.0f * (float)M_PI * fc / sr;
        float sn = std::sin(omega);
        float cs = std::cos(omega);
        float alpha = sn / (2.0f * Q);
        float a0 = 1.0f + alpha;
        b0 = ((1.0f - cs) * 0.5f) / a0;
        b1 = (1.0f - cs) / a0;
        b2 = ((1.0f - cs) * 0.5f) / a0;
        a1 = (-2.0f * cs) / a0;
        a2 = (1.0f - alpha) / a0;
    }

    void setHighPass(float fc, float Q, float sr) {
        fc = std::max(20.0f, std::min(sr * 0.45f, fc));
        float omega = 2.0f * (float)M_PI * fc / sr;
        float sn = std::sin(omega);
        float cs = std::cos(omega);
        float alpha = sn / (2.0f * Q);
        float a0 = 1.0f + alpha;
        b0 = ((1.0f + cs) * 0.5f) / a0;
        b1 = (-(1.0f + cs)) / a0;
        b2 = ((1.0f + cs) * 0.5f) / a0;
        a1 = (-2.0f * cs) / a0;
        a2 = (1.0f - alpha) / a0;
    }

    void setPeaking(float fc, float gainDb, float Q, float sr) {
        fc = std::max(20.0f, std::min(sr * 0.45f, fc));
        float A = std::pow(10.0f, gainDb / 40.0f);
        float omega = 2.0f * (float)M_PI * fc / sr;
        float sn = std::sin(omega);
        float cs = std::cos(omega);
        float alpha = sn / (2.0f * Q);
        float a0 = 1.0f + alpha / A;
        b0 = (1.0f + alpha * A) / a0;
        b1 = (-2.0f * cs) / a0;
        b2 = (1.0f - alpha * A) / a0;
        a1 = (-2.0f * cs) / a0;
        a2 = (1.0f - alpha / A) / a0;
    }

    void setLowShelf(float fc, float gainDb, float Q, float sr) {
        fc = std::max(20.0f, std::min(sr * 0.45f, fc));
        float A = std::pow(10.0f, gainDb / 40.0f);
        float omega = 2.0f * (float)M_PI * fc / sr;
        float sn = std::sin(omega);
        float cs = std::cos(omega);
        float alpha = sn / (2.0f * Q);
        float beta = 2.0f * std::sqrt(A) * alpha;

        float a0 = (A + 1.0f) + (A - 1.0f) * cs + beta;
        b0 = (A * ((A + 1.0f) - (A - 1.0f) * cs + beta)) / a0;
        b1 = (2.0f * A * ((A - 1.0f) - (A + 1.0f) * cs)) / a0;
        b2 = (A * ((A + 1.0f) - (A - 1.0f) * cs - beta)) / a0;
        a1 = (-2.0f * ((A - 1.0f) + (A + 1.0f) * cs)) / a0;
        a2 = ((A + 1.0f) + (A - 1.0f) * cs - beta) / a0;
    }

    void setHighShelf(float fc, float gainDb, float Q, float sr) {
        fc = std::max(20.0f, std::min(sr * 0.45f, fc));
        float A = std::pow(10.0f, gainDb / 40.0f);
        float omega = 2.0f * (float)M_PI * fc / sr;
        float sn = std::sin(omega);
        float cs = std::cos(omega);
        float alpha = sn / (2.0f * Q);
        float beta = 2.0f * std::sqrt(A) * alpha;

        float a0 = (A + 1.0f) - (A - 1.0f) * cs + beta;
        b0 = (A * ((A + 1.0f) + (A - 1.0f) * cs + beta)) / a0;
        b1 = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cs)) / a0;
        b2 = (A * ((A + 1.0f) - (A - 1.0f) * cs - beta)) / a0;
        a1 = (2.0f * ((A - 1.0f) - (A + 1.0f) * cs)) / a0;
        a2 = ((A + 1.0f) - (A - 1.0f) * cs - beta) / a0;
    }
};

// ============================================================================
// Stage 3B: Studio 4-Band Acoustic Parametric EQ (Active 3D Tone Matrix)
// ============================================================================
class Acoustic4BandEQ {
public:
    FluidBiquad band1_lowShelf;  // 110 Hz Warmth Shelf
    FluidBiquad band2_lowMid;    // 200 - 1000 Hz Boxiness Cut
    FluidBiquad band3_highMid;   // 1.5 - 6.5 kHz String Articulation
    FluidBiquad band4_highShelf; // 9.5 kHz Air Glass Shelf

    float sample_rate = 48000.0f;

    float lastLowGain = -999.0f;
    float lastLowMidFreq = -999.0f, lastLowMidGain = -999.0f;
    float lastHighMidFreq = -999.0f, lastHighMidGain = -999.0f;
    float lastHighGain = -999.0f;

    void init(float sr) {
        sample_rate = sr > 0.0f ? sr : 48000.0f;
        reset();
        update(0.0f, 450.0f, -2.0f, 3200.0f, 1.5f, 2.0f);
    }

    void reset() {
        band1_lowShelf.reset();
        band2_lowMid.reset();
        band3_highMid.reset();
        band4_highShelf.reset();
        lastLowGain = lastLowMidFreq = lastLowMidGain = lastHighMidFreq = lastHighMidGain = lastHighGain = -999.0f;
    }

    void update(float lowGainDb, float lowMidFreq, float lowMidGainDb,
                float highMidFreq, float highMidGainDb, float highGainDb) {
        if (std::abs(lowGainDb - lastLowGain) > 0.05f) {
            lastLowGain = lowGainDb;
            band1_lowShelf.setLowShelf(110.0f, lowGainDb, 0.7071f, sample_rate);
        }

        if (std::abs(lowMidFreq - lastLowMidFreq) > 1.0f || std::abs(lowMidGainDb - lastLowMidGain) > 0.05f) {
            lastLowMidFreq = lowMidFreq;
            lastLowMidGain = lowMidGainDb;
            band2_lowMid.setPeaking(std::clamp(lowMidFreq, 150.0f, 1200.0f), lowMidGainDb, 1.25f, sample_rate);
        }

        if (std::abs(highMidFreq - lastHighMidFreq) > 2.0f || std::abs(highMidGainDb - lastHighMidGain) > 0.05f) {
            lastHighMidFreq = highMidFreq;
            lastHighMidGain = highMidGainDb;
            band3_highMid.setPeaking(std::clamp(highMidFreq, 1200.0f, 6500.0f), highMidGainDb, 1.20f, sample_rate);
        }

        if (std::abs(highGainDb - lastHighGain) > 0.05f) {
            lastHighGain = highGainDb;
            band4_highShelf.setHighShelf(9500.0f, highGainDb, 0.7071f, sample_rate);
        }
    }

    inline float process(float in_sample) {
        float s = band1_lowShelf.process(in_sample);
        s = band2_lowMid.process(s);
        s = band3_highMid.process(s);
        s = band4_highShelf.process(s);
        return flush_denormal(s);
    }
};

// ============================================================================
// World-Class Modulated Dual-Tank Acoustic Reverb Space (Dattorro / Lexicon Topologies)
// ============================================================================
struct FluidReverbSpace {
    // 1. Pre-Delay buffer (up to 170 ms at 48kHz = 8192 samples)
    static constexpr int PREDLY_SIZE = 8192;
    float preDelayBuf[PREDLY_SIZE] = {};
    int preDelayW = 0;

    // 2. Four Cascaded Schroeder-Dattorro Input Allpass Diffusers (Transient Shattering)
    static constexpr int IN_AP1_SIZE = 1024;
    static constexpr int IN_AP2_SIZE = 1024;
    static constexpr int IN_AP3_SIZE = 2048;
    static constexpr int IN_AP4_SIZE = 2048;
    float inApBuf1[IN_AP1_SIZE] = {};
    float inApBuf2[IN_AP2_SIZE] = {};
    float inApBuf3[IN_AP3_SIZE] = {};
    float inApBuf4[IN_AP4_SIZE] = {};
    int inApIdx1 = 0, inApIdx2 = 0, inApIdx3 = 0, inApIdx4 = 0;

    // 3. Tank Left Buffers (Figure-of-Eight Branch A)
    static constexpr int TANK_MAP_SIZE = 4096;
    static constexpr int TANK_DEL_SIZE = 16384;
    static constexpr int TANK_AP2_SIZE = 8192;

    float tankL_mapBuf[TANK_MAP_SIZE] = {};
    int tankL_mapW = 0;
    float tankL_del1Buf[TANK_DEL_SIZE] = {};
    int tankL_del1W = 0;
    float tankL_ap2Buf[TANK_AP2_SIZE] = {};
    int tankL_ap2W = 0;
    float tankL_del2Buf[TANK_DEL_SIZE] = {};
    int tankL_del2W = 0;
    float tankL_dampLp = 0.0f;
    float crossFeedbackL = 0.0f;

    // 4. Tank Right Buffers (Figure-of-Eight Branch B)
    float tankR_mapBuf[TANK_MAP_SIZE] = {};
    int tankR_mapW = 0;
    float tankR_del1Buf[TANK_DEL_SIZE] = {};
    int tankR_del1W = 0;
    float tankR_ap2Buf[TANK_AP2_SIZE] = {};
    int tankR_ap2W = 0;
    float tankR_del2Buf[TANK_DEL_SIZE] = {};
    int tankR_del2W = 0;
    float tankR_dampLp = 0.0f;
    float crossFeedbackR = 0.0f;

    // Dual LFO Phase Accumulators for Multi-Speed Chorus Modulation
    float lfoPhaseL = 0.0f;
    float lfoPhaseR = 0.0f;

    // Pre-Send Biquad Conditioning
    FluidBiquad sendHp;
    FluidBiquad sendLp;
    float lastLowCut = -1.0f;
    float lastHighCut = -1.0f;

    void reset() {
        std::fill(preDelayBuf, preDelayBuf + PREDLY_SIZE, 0.0f);
        preDelayW = 0;

        std::fill(inApBuf1, inApBuf1 + IN_AP1_SIZE, 0.0f);
        std::fill(inApBuf2, inApBuf2 + IN_AP2_SIZE, 0.0f);
        std::fill(inApBuf3, inApBuf3 + IN_AP3_SIZE, 0.0f);
        std::fill(inApBuf4, inApBuf4 + IN_AP4_SIZE, 0.0f);
        inApIdx1 = inApIdx2 = inApIdx3 = inApIdx4 = 0;

        std::fill(tankL_mapBuf, tankL_mapBuf + TANK_MAP_SIZE, 0.0f);
        std::fill(tankL_del1Buf, tankL_del1Buf + TANK_DEL_SIZE, 0.0f);
        std::fill(tankL_ap2Buf, tankL_ap2Buf + TANK_AP2_SIZE, 0.0f);
        std::fill(tankL_del2Buf, tankL_del2Buf + TANK_DEL_SIZE, 0.0f);
        tankL_mapW = tankL_del1W = tankL_ap2W = tankL_del2W = 0;
        tankL_dampLp = 0.0f;
        crossFeedbackL = 0.0f;

        std::fill(tankR_mapBuf, tankR_mapBuf + TANK_MAP_SIZE, 0.0f);
        std::fill(tankR_del1Buf, tankR_del1Buf + TANK_DEL_SIZE, 0.0f);
        std::fill(tankR_ap2Buf, tankR_ap2Buf + TANK_AP2_SIZE, 0.0f);
        std::fill(tankR_del2Buf, tankR_del2Buf + TANK_DEL_SIZE, 0.0f);
        tankR_mapW = tankR_del1W = tankR_ap2W = tankR_del2W = 0;
        tankR_dampLp = 0.0f;
        crossFeedbackR = 0.0f;

        lfoPhaseL = 0.0f;
        lfoPhaseR = 0.0f;

        sendHp.reset();
        sendLp.reset();
        lastLowCut = -1.0f;
        lastHighCut = -1.0f;
    }

    // Linear fractional delay tap read helper
    static inline float readDelayTap(const float* buffer, int bufSize, int writePos, float tapDelay) {
        float rPos = (float)writePos - tapDelay;
        while (rPos < 0.0f) rPos += (float)bufSize;
        while (rPos >= (float)bufSize) rPos -= (float)bufSize;
        int i0 = (int)rPos;
        float frac = rPos - (float)i0;
        int i1 = (i0 + 1) % bufSize;
        return buffer[i0] + frac * (buffer[i1] - buffer[i0]);
    }

    // 4-point Hermite cubic fractional read for modulated allpass (glitch-free pitch shifting)
    static inline float readDelayHermite(const float* buffer, int bufSize, int writePos, float delay) {
        float rPos = (float)writePos - delay;
        while (rPos < 0.0f) rPos += (float)bufSize;
        while (rPos >= (float)bufSize) rPos -= (float)bufSize;
        int i1 = (int)rPos;
        float frac = rPos - (float)i1;
        int i0 = (i1 - 1 + bufSize) % bufSize;
        int i2 = (i1 + 1) % bufSize;
        int i3 = (i1 + 2) % bufSize;

        float y0 = buffer[i0], y1 = buffer[i1], y2 = buffer[i2], y3 = buffer[i3];
        float c0 = y1;
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    // Standard Allpass step
    static inline float processAllpass(float in, float* buf, int bufSize, int& idx, int delayLength, float g) {
        int rIdx = idx - delayLength;
        if (rIdx < 0) rIdx += bufSize;
        float bufVal = buf[rIdx];
        float vn = in + g * bufVal;
        buf[idx] = vn;
        float out = bufVal - g * vn;
        if (++idx >= bufSize) idx = 0;
        return out;
    }

    // Modulated Allpass step
    static inline float processModAllpass(float in, float* buf, int bufSize, int& idx, float delayLength, float g) {
        float bufVal = readDelayHermite(buf, bufSize, idx, delayLength);
        float vn = in + g * bufVal;
        buf[idx] = vn;
        float out = bufVal - g * vn;
        if (++idx >= bufSize) idx = 0;
        return out;
    }

    inline void process(
        float inL, float inR,
        float curSize, float curShape,
        float sendLowCutHz, float sendHighCutHz,
        float dwellNorm, float mixNorm,
        int reverbType,
        float sampleRate,
        float& outL, float& outR
    ) {
        if (mixNorm <= 0.001f) {
            outL = inL;
            outR = inR;
            return;
        }

        // 1. Algorithm Configuration (0: Studio Room, 1: Concert Hall, 2: Vintage Plate)
        float fsScale = (sampleRate > 8000.0f) ? (sampleRate / 48000.0f) : 1.0f;
        float preDelayMs = 0.0f;
        float sizeScale = 1.0f;
        float decayFeedback = 0.50f;
        float lfoRateHz = 0.70f;
        float lfoDepthSamp = 8.0f;
        float g_inDiff1 = 0.75f, g_inDiff2 = 0.625f;
        float g_map = 0.70f, g_ap2 = 0.50f;
        float effLowCut = sendLowCutHz;
        float effHighCut = sendHighCutHz;

        if (reverbType == 0) {
            // --- 0: STUDIO ROOM ---
            // Warm, intimate wooden room with dense initial diffusion and natural acoustic absorption
            preDelayMs = 0.0f;
            sizeScale = 0.55f * std::clamp(curSize, 0.5f, 2.0f);
            decayFeedback = 0.28f + 0.54f * dwellNorm; // RT60 ~ 0.4s to 2.4s
            lfoRateHz = 1.15f;
            lfoDepthSamp = 3.5f * fsScale; // gentle micro-turbulence
            g_inDiff1 = 0.78f; g_inDiff2 = 0.68f; // fast early reflection buildup
            g_map = 0.65f; g_ap2 = 0.55f;
            effLowCut = std::max(65.0f, sendLowCutHz);
            effHighCut = std::min(sendHighCutHz, 7200.0f); // warm natural room high-cut
        } else if (reverbType == 1) {
            // --- 1: CONCERT HALL ---
            // Expansive, majestic Lexicon 480L-style hall with lush floating tail and cathedral depth
            preDelayMs = 24.0f; // 24ms pre-delay gives acoustic guitar notes clean transient breathing room
            sizeScale = 1.35f * std::clamp(curSize, 0.7f, 2.5f);
            decayFeedback = 0.45f + 0.52f * dwellNorm; // RT60 ~ 1.8s to 12s
            lfoRateHz = 0.65f;
            lfoDepthSamp = 10.0f * fsScale; // lush chorus modulation breaks all resonances
            g_inDiff1 = 0.70f; g_inDiff2 = 0.60f;
            g_map = 0.70f; g_ap2 = 0.50f;
            effLowCut = std::max(35.0f, sendLowCutHz * 0.6f); // full sub-body retention
            effHighCut = sendHighCutHz;
        } else if (reverbType == 2) {
            // --- 2: VINTAGE PLATE (EMT 140) ---
            // Ultra-dense steel plate with shimmering high-end dispersion, high density, tight bass
            preDelayMs = 4.0f;
            sizeScale = 0.85f * std::clamp(curSize, 0.6f, 2.0f);
            decayFeedback = 0.35f + 0.60f * dwellNorm; // bright, long plate tail
            lfoRateHz = 1.45f;
            lfoDepthSamp = 6.0f * fsScale; // mechanical sheet metal dispersion
            g_inDiff1 = 0.82f; g_inDiff2 = 0.72f; // immediate dense reflection onset
            g_map = 0.75f; g_ap2 = 0.60f;
            effLowCut = std::max(180.0f, sendLowCutHz * 1.8f); // authentic EMT 140 HPF removes low-end mud
            effHighCut = std::max(10000.0f, sendHighCutHz);    // silvery plate shimmer
        }

        // 2. Pre-Tank Send Conditioning Filter
        if (std::fabs(effLowCut - lastLowCut) > 0.25f) {
            lastLowCut = effLowCut;
            sendHp.setHighPass(std::clamp(effLowCut, 20.0f, 1000.0f), 0.7071f, sampleRate);
        }
        if (std::fabs(effHighCut - lastHighCut) > 1.0f) {
            lastHighCut = effHighCut;
            sendLp.setLowPass(std::clamp(effHighCut, 1500.0f, 18000.0f), 0.7071f, sampleRate);
        }

        float inMono = (inL + inR) * 0.5f;
        float conditioned = sendLp.process(sendHp.process(inMono));

        // 3. 12AT7 Tube Pre-Driver Stage (Gentle dynamic warmth)
        float driveGain = 0.35f + dwellNorm * 0.35f;
        float driven = FluidMath::fast_tanh(conditioned * driveGain);

        // 4. Pre-Delay
        int predlySamples = std::clamp((int)(preDelayMs * 0.001f * sampleRate), 0, PREDLY_SIZE - 1);
        preDelayBuf[preDelayW] = driven;
        float pdSignal = readDelayTap(preDelayBuf, PREDLY_SIZE, preDelayW, (float)predlySamples);
        if (++preDelayW >= PREDLY_SIZE) preDelayW = 0;

        // 5. Four Cascaded Schroeder-Dattorro Input Allpass Diffusers
        int d_ap1 = std::max(1, (int)(228.0f * fsScale));
        int d_ap2 = std::max(1, (int)(172.0f * fsScale));
        int d_ap3 = std::max(1, (int)(610.0f * fsScale));
        int d_ap4 = std::max(1, (int)(445.0f * fsScale));

        float ap1 = processAllpass(pdSignal, inApBuf1, IN_AP1_SIZE, inApIdx1, d_ap1, g_inDiff1);
        float ap2 = processAllpass(ap1, inApBuf2, IN_AP2_SIZE, inApIdx2, d_ap2, g_inDiff1);
        float ap3 = processAllpass(ap2, inApBuf3, IN_AP3_SIZE, inApIdx3, d_ap3, g_inDiff2);
        float diffused = processAllpass(ap3, inApBuf4, IN_AP4_SIZE, inApIdx4, d_ap4, g_inDiff2);

        // 6. LFO Multi-Phase Pitch Modulation
        float lfoIncL = (lfoRateHz * 6.2831853f) / sampleRate;
        float lfoIncR = (lfoRateHz * 1.272f * 6.2831853f) / sampleRate; // golden ratio spread
        lfoPhaseL += lfoIncL;
        if (lfoPhaseL >= 6.2831853f) lfoPhaseL -= 6.2831853f;
        lfoPhaseR += lfoIncR;
        if (lfoPhaseR >= 6.2831853f) lfoPhaseR -= 6.2831853f;

        float modL = std::sin(lfoPhaseL) * lfoDepthSamp;
        float modR = std::cos(lfoPhaseR) * lfoDepthSamp;

        // 7. Scaled Tank Delay Parameters
        float scale = fsScale * sizeScale;
        float len_L_map = std::clamp(1080.0f * scale, 32.0f, (float)(TANK_MAP_SIZE - 64));
        float len_L_del1 = std::clamp(7120.0f * scale, 32.0f, (float)(TANK_DEL_SIZE - 64));
        int   len_L_ap2  = std::clamp((int)(2900.0f * scale), 16, TANK_AP2_SIZE - 32);
        float len_L_del2 = std::clamp(5900.0f * scale, 32.0f, (float)(TANK_DEL_SIZE - 64));

        float len_R_map = std::clamp(1450.0f * scale, 32.0f, (float)(TANK_MAP_SIZE - 64));
        float len_R_del1 = std::clamp(7500.0f * scale, 32.0f, (float)(TANK_DEL_SIZE - 64));
        int   len_R_ap2  = std::clamp((int)(3700.0f * scale), 16, TANK_AP2_SIZE - 32);
        float len_R_del2 = std::clamp(5100.0f * scale, 32.0f, (float)(TANK_DEL_SIZE - 64));

        float dampAlpha = 1.0f - std::exp(-6.2831853f * effHighCut / sampleRate);

        // 8. Dual Cross-Coupled Figure-of-Eight Feedback Loop
        // Tank Left Input: Diffused audio + cross-feedback from Tank Right
        float tankInL = diffused * 0.7071f + crossFeedbackR * decayFeedback;
        float nodeL1 = processModAllpass(tankInL, tankL_mapBuf, TANK_MAP_SIZE, tankL_mapW, len_L_map + modL, g_map);

        tankL_del1Buf[tankL_del1W] = nodeL1;
        int wL1 = tankL_del1W;
        if (++tankL_del1W >= TANK_DEL_SIZE) tankL_del1W = 0;
        float d1_outL = readDelayTap(tankL_del1Buf, TANK_DEL_SIZE, wL1, len_L_del1);

        tankL_dampLp += dampAlpha * (d1_outL - tankL_dampLp);
        float dampedL = tankL_dampLp;

        // Corner eddy turbulence non-linearity (Fluid physical acoustics)
        float xNormL = FluidMath::fast_tanh(dampedL);
        float eddyL = curShape * 0.05f * (xNormL * xNormL * (1.0f - xNormL));
        dampedL = FluidMath::fast_tanh(dampedL) - eddyL;

        float nodeL2 = processAllpass(dampedL, tankL_ap2Buf, TANK_AP2_SIZE, tankL_ap2W, len_L_ap2, g_ap2);

        tankL_del2Buf[tankL_del2W] = nodeL2;
        int wL2 = tankL_del2W;
        if (++tankL_del2W >= TANK_DEL_SIZE) tankL_del2W = 0;
        float d2_outL = readDelayTap(tankL_del2Buf, TANK_DEL_SIZE, wL2, len_L_del2);
        crossFeedbackL = d2_outL;

        // Tank Right Input: Diffused audio + cross-feedback from Tank Left
        float tankInR = diffused * 0.7071f + crossFeedbackL * decayFeedback;
        float nodeR1 = processModAllpass(tankInR, tankR_mapBuf, TANK_MAP_SIZE, tankR_mapW, len_R_map + modR, g_map);

        tankR_del1Buf[tankR_del1W] = nodeR1;
        int wR1 = tankR_del1W;
        if (++tankR_del1W >= TANK_DEL_SIZE) tankR_del1W = 0;
        float d1_outR = readDelayTap(tankR_del1Buf, TANK_DEL_SIZE, wR1, len_R_del1);

        tankR_dampLp += dampAlpha * (d1_outR - tankR_dampLp);
        float dampedR = tankR_dampLp;

        float xNormR = FluidMath::fast_tanh(dampedR);
        float eddyR = curShape * 0.05f * (xNormR * xNormR * (1.0f - xNormR));
        dampedR = FluidMath::fast_tanh(dampedR) - eddyR;

        float nodeR2 = processAllpass(dampedR, tankR_ap2Buf, TANK_AP2_SIZE, tankR_ap2W, len_R_ap2, g_ap2);

        tankR_del2Buf[tankR_del2W] = nodeR2;
        int wR2 = tankR_del2W;
        if (++tankR_del2W >= TANK_DEL_SIZE) tankR_del2W = 0;
        float d2_outR = readDelayTap(tankR_del2Buf, TANK_DEL_SIZE, wR2, len_R_del2);
        crossFeedbackR = d2_outR;

        // 9. Multi-Tap Spatial Stereo Output Extraction (Zero correlation between L and R)
        float tap_R1_1 = std::clamp(430.0f * scale, 1.0f, (float)(TANK_DEL_SIZE - 16));
        float tap_R1_2 = std::clamp(3500.0f * scale, 1.0f, (float)(TANK_DEL_SIZE - 16));
        float tap_R2_1 = std::clamp(3100.0f * scale, 1.0f, (float)(TANK_DEL_SIZE - 16));
        float tap_Rap_1 = std::clamp(1400.0f * scale, 1.0f, (float)(TANK_AP2_SIZE - 16));
        float tap_L1_1 = std::clamp(2400.0f * scale, 1.0f, (float)(TANK_DEL_SIZE - 16));
        float tap_L2_1 = std::clamp(1900.0f * scale, 1.0f, (float)(TANK_DEL_SIZE - 16));
        float tap_Lap_1 = std::clamp(2100.0f * scale, 1.0f, (float)(TANK_AP2_SIZE - 16));

        float tap_L1_2 = std::clamp(480.0f * scale, 1.0f, (float)(TANK_DEL_SIZE - 16));
        float tap_L1_3 = std::clamp(3700.0f * scale, 1.0f, (float)(TANK_DEL_SIZE - 16));
        float tap_L2_2 = std::clamp(2800.0f * scale, 1.0f, (float)(TANK_DEL_SIZE - 16));
        float tap_Lap_2 = std::clamp(1200.0f * scale, 1.0f, (float)(TANK_AP2_SIZE - 16));
        float tap_R1_3 = std::clamp(2200.0f * scale, 1.0f, (float)(TANK_DEL_SIZE - 16));
        float tap_R2_2 = std::clamp(2100.0f * scale, 1.0f, (float)(TANK_DEL_SIZE - 16));
        float tap_Rap_2 = std::clamp(1900.0f * scale, 1.0f, (float)(TANK_AP2_SIZE - 16));

        float wetL = 0.50f * readDelayTap(tankR_del1Buf, TANK_DEL_SIZE, wR1, tap_R1_1)
                   + 0.42f * readDelayTap(tankR_del1Buf, TANK_DEL_SIZE, wR1, tap_R1_2)
                   - 0.38f * readDelayTap(tankR_del2Buf, TANK_DEL_SIZE, wR2, tap_R2_1)
                   + 0.32f * readDelayTap(tankR_ap2Buf,  TANK_AP2_SIZE, tankR_ap2W, tap_Rap_1)
                   - 0.40f * readDelayTap(tankL_del1Buf, TANK_DEL_SIZE, wL1, tap_L1_1)
                   - 0.35f * readDelayTap(tankL_del2Buf, TANK_DEL_SIZE, wL2, tap_L2_1)
                   + 0.30f * readDelayTap(tankL_ap2Buf,  TANK_AP2_SIZE, tankL_ap2W, tap_Lap_1);

        float wetR = 0.50f * readDelayTap(tankL_del1Buf, TANK_DEL_SIZE, wL1, tap_L1_2)
                   + 0.42f * readDelayTap(tankL_del1Buf, TANK_DEL_SIZE, wL1, tap_L1_3)
                   - 0.38f * readDelayTap(tankL_del2Buf, TANK_DEL_SIZE, wL2, tap_L2_2)
                   + 0.32f * readDelayTap(tankL_ap2Buf,  TANK_AP2_SIZE, tankL_ap2W, tap_Lap_2)
                   - 0.40f * readDelayTap(tankR_del1Buf, TANK_DEL_SIZE, wR1, tap_R1_3)
                   - 0.35f * readDelayTap(tankR_del2Buf, TANK_DEL_SIZE, wR2, tap_R2_2)
                   + 0.30f * readDelayTap(tankR_ap2Buf,  TANK_AP2_SIZE, tankR_ap2W, tap_Rap_2);

        // 10. Equal-Power Dry/Wet Blend
        float mixAngle = mixNorm * 1.5707963f; // 0 .. pi/2
        float dryG = std::cos(mixAngle);
        float wetG = std::sin(mixAngle);

        outL = inL * dryG + wetL * wetG;
        outR = inR * dryG + wetR * wetG;
    }
};

// ============================================================================
// Stage 6B: FLUID STEREO DOUBLER & DUAL-GUITAR DECORRELATOR
//    Turns 1 Input + 2 Body Models into 2 Separate Players in Stereo
// ============================================================================
struct StereoFrame {
    float left;
    float right;
};

class FluidStereoDoublerDSP {
private:
    static constexpr int kMaxDoublerDelay = 2048; // ~42ms buffer @ 48kHz
    float rightTrackDelayBuf[kMaxDoublerDelay] = {0.0f};
    static constexpr int kCrossBleedBufSize = 2048; // Support up to 192kHz/384kHz
    float crossBleedLtoRBuf[kCrossBleedBufSize] = {0.0f};
    float crossBleedRtoLBuf[kCrossBleedBufSize] = {0.0f};
    int writeIdx = 0;
    int bleedWriteIdx = 0;

    // Fluid Turbulence & Human Timing Drift States
    float fluidTimeOffsetSamples = 650.0f; // Starts around ~13.5ms
    float fluidDriftVelocity = 0.0f;
    float chamberSagR = 1.0f;
    float prevInput = 0.0f;
    float pickViscosityStateR = 0.0f;

    // Non-repeating chaotic sub-audio drift (Lorenz-inspired coupled oscillators)
    float driftPhase1 = 0.13f;
    float driftPhase2 = 0.71f;

    // All-pass transient smear for Player 2 (Changes pick angle feel)
    float apX1 = 0.0f, apY1 = 0.0f;
    float apX2 = 0.0f, apY2 = 0.0f;

    float sampleRate = 48000.0f;

    // Linear interpolation helper for smooth fractional sample pitch/time drift
    inline float readFractionalDelay(const float* buf, int mask, float delaySamples) const noexcept {
        float readPos = static_cast<float>(writeIdx) - delaySamples;
        while (readPos < 0.0f) readPos += static_cast<float>(mask + 1);
        const int idx0 = static_cast<int>(readPos) & mask;
        const int idx1 = (idx0 + 1) & mask;
        const float frac = readPos - std::floor(readPos);
        return buf[idx0] + frac * (buf[idx1] - buf[idx0]);
    }

public:
    void prepare(float sr) noexcept {
        sampleRate = sr > 8000.0f ? sr : 48000.0f;
        reset();
    }

    void reset() noexcept {
        std::fill(std::begin(rightTrackDelayBuf), std::end(rightTrackDelayBuf), 0.0f);
        std::fill(std::begin(crossBleedLtoRBuf), std::end(crossBleedLtoRBuf), 0.0f);
        std::fill(std::begin(crossBleedRtoLBuf), std::end(crossBleedRtoLBuf), 0.0f);
        writeIdx = 0;
        bleedWriteIdx = 0;
        fluidTimeOffsetSamples = 0.014f * sampleRate;
        fluidDriftVelocity = 0.0f;
        chamberSagR = 1.0f;
        prevInput = 0.0f;
        pickViscosityStateR = 0.0f;
        apX1 = apY1 = apX2 = apY2 = 0.0f;
    }

    // Step 1: Call BEFORE your two separate Acoustic Body Models
    // Takes your raw Mono Piezo/Mic input and generates two human-decorrelated 
    // string excitations (Player 1 Left, Player 2 Right)
    inline StereoFrame generateDualPlayers(float monoInput, float humanSlopAmount = 0.75f) noexcept {
        // 1. Measure instantaneous pick attack velocity
        const float inputVelocity = monoInput - prevInput;
        prevInput = monoInput;
        const float attackEnergy = std::min(std::abs(inputVelocity) * 4.0f, 1.5f);

        // 2. Player 2 Fluid Chamber Sag (Hard strums push Player 2's timing & micro-pitch!)
        chamberSagR -= attackEnergy * 0.004f;
        chamberSagR += (1.0f - chamberSagR) * 0.0012f;
        chamberSagR = std::clamp(chamberSagR, 0.40f, 1.0f);

        // 3. Non-Repeating Fluid Drift (2 incommensurate slow waves + transient push)
        driftPhase1 += 0.43f / sampleRate;
        if (driftPhase1 > 1.0f) driftPhase1 -= 2.0f;
        driftPhase2 += 0.79f / sampleRate;
        if (driftPhase2 > 1.0f) driftPhase2 -= 2.0f;

        const float organicWander = (FluidMath::fast_sin(driftPhase1 * 3.14159265f) * 0.6f)
                                  + (FluidMath::fast_sin(driftPhase2 * 3.14159265f) * 0.4f);

        // Target timing offset moves dynamically between 8ms and 24ms based on how hard you play
        const float baseDelayMs = 0.011f + (humanSlopAmount * 0.007f); // 11ms to 18ms center
        const float sagPushMs   = (1.0f - chamberSagR) * 0.012f * humanSlopAmount;
        const float wanderMs    = organicWander * 0.0045f * humanSlopAmount;

        const float targetDelaySamples = (baseDelayMs + sagPushMs + wanderMs) * sampleRate;

        // Smooth with fluid inertia so pitch glides like a human finger on a fret (+- 4 cents max)
        const float springForce = (targetDelaySamples - fluidTimeOffsetSamples) * 0.00015f;
        fluidDriftVelocity = (fluidDriftVelocity * 0.996f) + springForce;
        fluidTimeOffsetSamples += fluidDriftVelocity;
        fluidTimeOffsetSamples = std::clamp(fluidTimeOffsetSamples, 64.0f, static_cast<float>(kMaxDoublerDelay - 4));

        // 4. Write Mono Input & Read Decorrelated Player 2 Stream
        rightTrackDelayBuf[writeIdx] = monoInput;
        float player2Raw = readFractionalDelay(rightTrackDelayBuf, kMaxDoublerDelay - 1, fluidTimeOffsetSamples);
        writeIdx = (writeIdx + 1) & (kMaxDoublerDelay - 1);

        // 5. Pick-Angle Viscosity & Phase Dispersion on Player 2
        // Disperses the pick transient across 1.2ms so the brain hears TWO distinct picks!
        const float g = 0.52f;
        const float d1 = -g * player2Raw + apX1 + g * apY1;
        apX1 = player2Raw; apY1 = d1;
        const float d2 =  g * d1 + apX2 - g * apY2;
        apX2 = d1; apY2 = d2;

        // Subtle dynamic pick warmth on Player 2 so left and right attacks never phase-lock
        pickViscosityStateR = (pickViscosityStateR * 0.55f) + (d2 * 0.45f);
        const float player2Output = (d2 * 0.65f) + (pickViscosityStateR * 0.35f);

        return { monoInput, player2Output };
    }

    // Step 2: Call AFTER running Left Model (e.g., Martin D-45) & Right Model (e.g., Taylor/OM)
    // Adds 3D Sympathetic Cross-Room Coupling (Guitar L vibrates Guitar R's air space ~4ft away)
    inline StereoFrame applyRoomCrossCoupling(float leftGuitarFinished, float rightGuitarFinished, 
                                              float stereoWidth = 0.85f, float roomCoupling = 0.18f) noexcept 
    {
        const int mask = kCrossBleedBufSize - 1;
        crossBleedLtoRBuf[bleedWriteIdx] = leftGuitarFinished;
        crossBleedRtoLBuf[bleedWriteIdx] = rightGuitarFinished;

        // 4.2 feet between the two guitarists = ~3.8ms acoustic travel time (182 samples @ 48kHz)
        const int crossDelay = std::clamp(static_cast<int>(0.0038f * sampleRate), 1, mask - 1);
        const int readIdx = (bleedWriteIdx - crossDelay + kCrossBleedBufSize) & mask;
        bleedWriteIdx = (bleedWriteIdx + 1) & mask;

        const float bleedFromL = crossBleedLtoRBuf[readIdx] * roomCoupling;
        const float bleedFromR = crossBleedRtoLBuf[readIdx] * roomCoupling;

        // Combine direct mics with cross-room sympathetic air waves
        float finalL = leftGuitarFinished + bleedFromR;
        float finalR = rightGuitarFinished + bleedFromL;

        // Mid/Side Width Control (Allows narrowing for tight mixes or 100% wide for solo acoustic)
        const float mid  = (finalL + finalR) * 0.5f;
        const float side = (finalL - finalR) * 0.5f * stereoWidth;

        return {
            FluidMath::fast_tanh(mid + side),
            FluidMath::fast_tanh(mid - side)
        };
    }
};

// ============================================================================
// Master Acoustic Preamp Engine
// ============================================================================
class CyberAcousticEngine {
public:
    AcousticCyberDenoiser  denoiser;
    AcousticInertialCell   inertial_cell;
    FluidAcousticCavity    cavity_engine;
    FluidAirMicDSP         air_mic_stage;
    SmartAcousticDeFizz    defizz_engine;
    Acoustic4BandEQ        eq_engine;
    AcousticFeedbackKiller feedback_killer;
    FluidReverbSpace       fluid_reverb;
    float sample_rate = 48000.0f;

    void init(float sr) {
        sample_rate = sr;
        denoiser.init(sr);
        inertial_cell.init(sr);
        cavity_engine.init(sr);
        air_mic_stage.init(sr);
        defizz_engine.init(sr);
        eq_engine.init(sr);
        feedback_killer.init(sr);
        fluid_reverb.reset();
    }

    void reset() {
        denoiser.reset();
        inertial_cell.reset();
        cavity_engine.reset();
        air_mic_stage.reset();
        defizz_engine.reset();
        eq_engine.reset();
        feedback_killer.reset();
        fluid_reverb.reset();
    }

    // Process mono acoustic signal through anti-quack, cavity model, air mic, defizz, eq, and feedback killer
    inline float processMonoBody(float in_sample,
                                 float denoise_amount,  float denoise_thresh_db,
                                 bool  harmonic_shield, bool  listen_noise,
                                 float quack_reduction, float body_depth,
                                 float mic_openness,    float mic_air_sheen,   float mic_proximity,
                                 float defizz_amount,   float air_shimmer,
                                 float eq_low_gain,     float eq_low_mid_freq, float eq_low_mid_gain,
                                 float eq_high_mid_freq,float eq_high_mid_gain,float eq_high_gain,
                                 int   body_type_preset,
                                 float custom_size,     float custom_shape) {
        // Stage 0: 10-Band Cyber-Denoiser PRO (Spectral Phase Subtraction)
        float s0 = denoiser.process(in_sample, denoise_amount, denoise_thresh_db,
                                    harmonic_shield, listen_noise);
        if (listen_noise) {
            return s0;
        }

        // Stage 1: Anti-Quack Mechanical Inertia
        float s1 = inertial_cell.process(s0, quack_reduction, 1.0f);

        // Stage 2: 2-DOF Coupled Fluid Space Cavity
        cavity_engine.setBodyGeometry(body_type_preset, custom_size, custom_shape);
        float s2_body = cavity_engine.process(s1, body_depth);

        // Stage 2B: Fluid Air & Close-Mic Radiation Engine
        float s2 = air_mic_stage.processSample(s1, s2_body, mic_openness, mic_air_sheen, mic_proximity);

        // Stage 3: Zero-Fizz Subtractive High-Band Conditioning
        float s3 = defizz_engine.process(s2, defizz_amount, air_shimmer);

        // Stage 3B: Studio 4-Band Acoustic Parametric EQ
        eq_engine.update(eq_low_gain, eq_low_mid_freq, eq_low_mid_gain,
                         eq_high_mid_freq, eq_high_mid_gain, eq_high_gain);
        float s4 = eq_engine.process(s3);

        // Stage 4: Auto-Tracking Feedback Nulls
        float s5 = feedback_killer.process(s4);

        return s5;
    }

    // Process stereo reverb stage on an incoming stereo soundfield
    inline void processStereoReverb(float inL, float inR,
                                    float reverb_size, float reverb_shape,
                                    float reverb_tone, float reverb_decay,
                                    float reverb_mix,  int   reverb_type,
                                    float& outL, float& outR) {
        fluid_reverb.process(inL, inR, reverb_size, reverb_shape, 80.0f, reverb_tone,
                             reverb_decay, reverb_mix, reverb_type, sample_rate, outL, outR);
    }

    // Process mono or stereo pickup input to stereo studio soundfield with denoiser, eq, and reverb
    inline void process(float in_sample, float& out_left, float& out_right,
                        float denoise_amount,  float denoise_thresh_db,
                        bool  harmonic_shield, bool  listen_noise,
                        float quack_reduction, float body_depth,
                        float mic_openness,    float mic_air_sheen,   float mic_proximity,
                        float defizz_amount,   float air_shimmer,
                        float eq_low_gain,     float eq_low_mid_freq, float eq_low_mid_gain,
                        float eq_high_mid_freq,float eq_high_mid_gain,float eq_high_gain,
                        float stereo_width,    int body_type_preset,
                        float custom_size,     float custom_shape,
                        float reverb_mix,      float reverb_decay,
                        float reverb_size,     float reverb_tone,
                        int   reverb_type,
                        float output_gain_linear) {
        // Stage 0: 10-Band Cyber-Denoiser PRO (Spectral Phase Subtraction)
        float s0 = denoiser.process(in_sample, denoise_amount, denoise_thresh_db,
                                    harmonic_shield, listen_noise);

        if (listen_noise) {
            out_left  = flush_denormal(s0 * output_gain_linear);
            out_right = flush_denormal(s0 * output_gain_linear);
            return;
        }

        // Stage 1: Anti-Quack Mechanical Inertia
        float s1 = inertial_cell.process(s0, quack_reduction, 1.0f);

        // Stage 2: 2-DOF Coupled Fluid Space Cavity
        cavity_engine.setBodyGeometry(body_type_preset, custom_size, custom_shape);
        float s2_body = cavity_engine.process(s1, body_depth);

        // Stage 2B: Fluid Air & Close-Mic Radiation Engine (Simulates Close Mic vs Pickup Openness)
        float s2 = air_mic_stage.processSample(s1, s2_body, mic_openness, mic_air_sheen, mic_proximity);

        // Stage 3: Zero-Fizz Subtractive High-Band Conditioning
        float s3 = defizz_engine.process(s2, defizz_amount, air_shimmer);

        // Stage 3B: Studio 4-Band Acoustic Parametric EQ (Active 3D Tone Matrix)
        eq_engine.update(eq_low_gain, eq_low_mid_freq, eq_low_mid_gain,
                         eq_high_mid_freq, eq_high_mid_gain, eq_high_gain);
        float s4 = eq_engine.process(s3);

        // Stage 4: Auto-Tracking Feedback Nulls
        float s5 = feedback_killer.process(s4);

        // Stage 5: Stereo Acoustic Imager (Virtual Mic Pair: 12th Fret & Lower Bout)
        float mid  = s5;
        float side = (s2 - s1) * 0.45f * stereo_width;

        float dryL = mid + side;
        float dryR = mid - side;

        // Stage 6: Pure Fluid Dynamics Reverb Space (From Euphoria Amp)
        float revL = dryL, revR = dryR;
        fluid_reverb.process(dryL, dryR, reverb_size, custom_shape, 80.0f, reverb_tone,
                             reverb_decay, reverb_mix, reverb_type, sample_rate, revL, revR);

        out_left  = flush_denormal(revL * output_gain_linear);
        out_right = flush_denormal(revR * output_gain_linear);
    }
};

} // namespace CyberAcoustic
