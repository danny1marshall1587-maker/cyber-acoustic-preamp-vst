#pragma once

#include <cmath>
#include <algorithm>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace CyberAcoustic {

inline float flush_denormal(float v) {
    if (std::abs(v) < 1.0e-15f) return 0.0f;
    return v;
}

inline float bounded_tanh(float x) {
    return std::tanh(x);
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

struct FluidReverbSpace {
    static constexpr int FDN_LINES = 4;
    static constexpr int BUF_SIZE = 16384;

    float delayBuffers[FDN_LINES][BUF_SIZE] = {};
    int writeIndices[FDN_LINES] = {};

    FluidBiquad sendHp;
    FluidBiquad sendLp;
    float lastLowCut = -1.0f;
    float lastHighCut = -1.0f;

    float apBuf1[211] = {};
    float apBuf2[379] = {};
    int apIdx1 = 0;
    int apIdx2 = 0;

    float dampingLp[FDN_LINES] = {};

    void reset() {
        for (int i = 0; i < FDN_LINES; ++i) {
            std::fill(delayBuffers[i], delayBuffers[i] + BUF_SIZE, 0.0f);
            writeIndices[i] = 0;
            dampingLp[i] = 0.0f;
        }
        std::fill(apBuf1, apBuf1 + 211, 0.0f);
        std::fill(apBuf2, apBuf2 + 379, 0.0f);
        apIdx1 = apIdx2 = 0;
        sendHp.reset();
        sendLp.reset();
        lastLowCut = -1.0f;
        lastHighCut = -1.0f;
    }

    inline void process(
        float inL, float inR,
        float curSize, float curShape,
        float sendLowCutHz, float sendHighCutHz,
        float dwellNorm, float mixNorm,
        float sampleRate,
        float& outL, float& outR
    ) {
        if (mixNorm <= 0.001f) {
            outL = inL;
            outR = inR;
            return;
        }

        // 1. Abbey Road Pre-Tank Send Conditioning Filter
        if (std::fabs(sendLowCutHz - lastLowCut) > 0.25f) {
            lastLowCut = sendLowCutHz;
            sendHp.setHighPass(std::clamp(sendLowCutHz, 20.0f, 1000.0f), 0.7071f, sampleRate);
        }
        if (std::fabs(sendHighCutHz - lastHighCut) > 1.0f) {
            lastHighCut = sendHighCutHz;
            sendLp.setLowPass(std::clamp(sendHighCutHz, 1500.0f, 18000.0f), 0.7071f, sampleRate);
        }

        float inMono = (inL + inR) * 0.5f;
        float conditioned = sendLp.process(sendHp.process(inMono));

        // 2. 12AT7 Tube Pre-Driver Stage (high-headroom glass driver)
        float driveGain = 0.38f + dwellNorm * 0.42f;
        float driven = std::tanh(conditioned * driveGain);

        // 3. Dual Allpass Diffuser Stage
        float b1 = apBuf1[apIdx1];
        float v1 = driven - 0.55f * b1;
        float diff1 = b1 + 0.55f * v1;
        apBuf1[apIdx1] = v1;
        if (++apIdx1 >= 211) apIdx1 = 0;

        float b2 = apBuf2[apIdx2];
        float v2 = diff1 - 0.50f * b2;
        float diff2 = b2 + 0.50f * v2;
        apBuf2[apIdx2] = v2;
        if (++apIdx2 >= 379) apIdx2 = 0;

        // 4. Multi-Chamber Waveguide Delay Reads (432 Hz Natural Acoustic Period)
        float t432 = sampleRate / 432.0f; // ~111.1 samples at 48kHz
        static const float s_primeRatios[FDN_LINES] = { 1.000f, 1.4142f, 1.8371f, 2.3607f };

        float readOutputs[FDN_LINES];
        for (int k = 0; k < FDN_LINES; ++k) {
            float targetDelay = std::clamp(t432 * s_primeRatios[k] * curSize * 2.85f, 8.0f, (float)(BUF_SIZE - 16));

            // Hermite 4-point cubic fractional delay read (zero pitch clicks)
            float rPos = (float)writeIndices[k] - targetDelay;
            while (rPos < 0.0f) rPos += (float)BUF_SIZE;
            int iPos = (int)rPos;
            float frac = rPos - (float)iPos;

            int i0 = (iPos - 1 + BUF_SIZE) % BUF_SIZE;
            int i1 = iPos % BUF_SIZE;
            int i2 = (iPos + 1) % BUF_SIZE;
            int i3 = (iPos + 2) % BUF_SIZE;

            float y0 = delayBuffers[k][i0], y1 = delayBuffers[k][i1];
            float y2 = delayBuffers[k][i2], y3 = delayBuffers[k][i3];
            float c0 = y1;
            float c1 = 0.5f * (y2 - y0);
            float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            readOutputs[k] = ((c3 * frac + c2) * frac + c1) * frac + c0;
        }

        // 5. 4x4 Lossless Orthogonal Hadamard Feedback Matrix
        float s0 = 0.5f * ( readOutputs[0] + readOutputs[1] + readOutputs[2] + readOutputs[3]);
        float s1 = 0.5f * ( readOutputs[0] - readOutputs[1] + readOutputs[2] - readOutputs[3]);
        float s2 = 0.5f * ( readOutputs[0] + readOutputs[1] - readOutputs[2] - readOutputs[3]);
        float s3 = 0.5f * ( readOutputs[0] - readOutputs[1] - readOutputs[2] + readOutputs[3]);
        float mixed[FDN_LINES] = { s0, s1, s2, s3 };

        // 6. Fluid Boundary Dispersion & Corner Eddy Turbulence (Shape Modulation)
        float feedbackFactor = 0.50f + 0.44f * dwellNorm;
        float dampAlpha = 0.20f + 0.35f * (1.0f - curShape * 0.45f);

        for (int k = 0; k < FDN_LINES; ++k) {
            dampingLp[k] += dampAlpha * (mixed[k] - dampingLp[k]);
            float damped = dampingLp[k];

            // Corner eddy turbulence: x^2(1 - x) bounded non-linearity
            float xNorm = std::tanh(damped);
            float eddy = curShape * 0.12f * (xNorm * xNorm * (1.0f - xNorm));
            float fbSignal = std::tanh(damped * feedbackFactor) - eddy;

            delayBuffers[k][writeIndices[k]] = diff2 + fbSignal;
            if (++writeIndices[k] >= BUF_SIZE) writeIndices[k] = 0;
        }

        // 7. Stereo Spatial Extraction
        float wetL = 1.25f * (readOutputs[0] + readOutputs[2]);
        float wetR = 1.25f * (readOutputs[1] + readOutputs[3]);

        // 8. Equal-Power Dry/Wet Blend
        float mixAngle = mixNorm * 1.5707963f; // 0 .. pi/2
        float dryG = std::cos(mixAngle);
        float wetG = std::sin(mixAngle);

        outL = inL * dryG + wetL * wetG;
        outR = inR * dryG + wetR * wetG;
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
        defizz_engine.init(sr);
        eq_engine.init(sr);
        feedback_killer.init(sr);
        fluid_reverb.reset();
    }

    void reset() {
        denoiser.reset();
        inertial_cell.reset();
        cavity_engine.reset();
        defizz_engine.reset();
        eq_engine.reset();
        feedback_killer.reset();
        fluid_reverb.reset();
    }

    // Process mono or stereo pickup input to stereo studio soundfield with denoiser, eq, and reverb
    inline void process(float in_sample, float& out_left, float& out_right,
                        float denoise_amount,  float denoise_thresh_db,
                        bool  harmonic_shield, bool  listen_noise,
                        float quack_reduction, float body_depth,
                        float defizz_amount,   float air_shimmer,
                        float eq_low_gain,     float eq_low_mid_freq, float eq_low_mid_gain,
                        float eq_high_mid_freq,float eq_high_mid_gain,float eq_high_gain,
                        float stereo_width,    int body_type_preset,
                        float custom_size,     float custom_shape,
                        float reverb_mix,      float reverb_decay,
                        float reverb_size,     float reverb_tone,
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
        float s2 = cavity_engine.process(s1, body_depth);

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
                             reverb_decay, reverb_mix, sample_rate, revL, revR);

        out_left  = flush_denormal(revL * output_gain_linear);
        out_right = flush_denormal(revR * output_gain_linear);
    }
};

} // namespace CyberAcoustic
