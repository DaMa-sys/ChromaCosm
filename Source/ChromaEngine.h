#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <array>
#include <vector>

namespace ChromaCosm
{

//==============================================================================
// Unconditionally Stable Cytomic TPT State-Variable Filter
// Cannot blow up even under rapid audio-rate cutoff/resonance modulation
//==============================================================================
struct StableSVF
{
    float ic1eq = 0.0f;
    float ic2eq = 0.0f;

    void reset()
    {
        ic1eq = 0.0f;
        ic2eq = 0.0f;
    }

    inline void process (float v0, float cutoffHz, float res01, float sampleRate,
                         float& low, float& band, float& high)
    {
        if (! std::isfinite (v0))
            v0 = 0.0f;

        const float maxCutoff = juce::jmin (18000.0f, sampleRate * 0.42f);
        const float fc = juce::jlimit (25.0f, maxCutoff, cutoffHz);
        const float g  = std::tan (juce::MathConstants<float>::pi * fc / sampleRate);
        // k in [0.15 .. 2.0]: 0.15 = high resonance without self-oscillation blowup
        const float k  = juce::jmap (juce::jlimit (0.0f, 1.0f, res01), 1.85f, 0.18f);

        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;

        const float v3 = v0 - ic2eq;
        const float v1 = a1 * ic1eq + a2 * v3;
        const float v2 = ic2eq + a2 * ic1eq + a3 * v3;

        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;

        // Soft-saturate internal integrator states to prevent extreme resonant spikes
        if (! std::isfinite (ic1eq) || ! std::isfinite (ic2eq))
        {
            ic1eq = 0.0f;
            ic2eq = 0.0f;
        }
        else if (std::fabs (ic1eq) > 8.0f || std::fabs (ic2eq) > 8.0f)
        {
            ic1eq = 8.0f * std::tanh (ic1eq * 0.125f);
            ic2eq = 8.0f * std::tanh (ic2eq * 0.125f);
        }

        low  = v2;
        band = v1;
        high = v0 - k * v1 - v2;
    }
};

inline float sanitizeAudio (float x, float limit = 4.0f)
{
    if (! std::isfinite (x) || std::fabs (x) < 1.0e-20f)
        return 0.0f;
    if (x > limit)  return limit;
    if (x < -limit) return -limit;
    return x;
}

inline int safeWrap (int index, int size)
{
    if (size <= 0) return 0;
    int r = index % size;
    return (r < 0) ? (r + size) : r;
}

//==============================================================================
// CHROMA CONSOLE DSP ENGINE
// 4 Re-orderable Modules:
//   0: CHARACTER (Drive, Sweeten, Fuzz, Howl, Swell, Off)
//   1: MOVEMENT  (Doubler, Vibrato, Phaser, Tremolo, Pitch, Off)
//   2: DIFFUSION (Cascade, Reels, Space, Collage, Reverse, Off)
//   3: TEXTURE   (Filter, Squash, Cassette, Broken, Interference, Off)
//==============================================================================
class ChromaEngine
{
public:
    enum CharacterType { Char_Off = 0, Char_Drive, Char_Sweeten, Char_Fuzz, Char_Howl, Char_Swell };
    enum MovementType  { Move_Off = 0, Move_Doubler, Move_Vibrato, Move_Phaser, Move_Tremolo, Move_Pitch };
    enum DiffusionType { Diff_Off = 0, Diff_Cascade, Diff_Reels, Diff_Space, Diff_Collage, Diff_Reverse };
    enum TextureType   { Text_Off = 0, Text_Filter, Text_Squash, Text_Cassette, Text_Broken, Text_Interference };

    struct Parameters
    {
        int routingPreset = 0;
        int calibration = 1;

        // Module 1: CHARACTER (Red)
        int charType = Char_Drive;
        float charTilt = 0.5f;
        float charAmount = 0.45f;
        float charVol = 0.75f;

        // Module 2: MOVEMENT (Yellow)
        int moveType = Move_Vibrato;
        float moveRate = 0.4f;
        float moveAmount = 0.4f;
        float moveDrift = 0.3f;
        float moveVol = 0.75f;

        // Module 3: DIFFUSION (Green)
        int diffType = Diff_Reels;
        float diffTime = 0.45f;
        float diffAmount = 0.45f;
        float diffDrift = 0.35f;
        float diffVol = 0.75f;

        // Module 4: TEXTURE (Blue)
        int textType = Text_Cassette;
        float textTilt = 0.5f;
        float textAmount = 0.35f;
        float textVol = 0.75f;

        // Master Chroma Controls
        float masterMix = 1.0f;
        float outputLevel = 0.75f;
        bool bypass = false;
        bool captureActive = false;
        double bpm = 120.0;
    };

    void prepare (double newSampleRate, int /*maxBlockSize*/)
    {
        sampleRate = (newSampleRate > 1000.0) ? newSampleRate : 44100.0;

        const int maxMoveSamples = static_cast<int> (sampleRate * 0.25) + 64;
        moveDelayL.assign (maxMoveSamples, 0.0f);
        moveDelayR.assign (maxMoveSamples, 0.0f);
        moveWritePos = 0;

        const int maxDiffSamples = static_cast<int> (sampleRate * 4.0) + 128;
        diffDelayL.assign (maxDiffSamples, 0.0f);
        diffDelayR.assign (maxDiffSamples, 0.0f);
        diffWritePos = 0;

        const int maxRevSamples = static_cast<int> (sampleRate * 2.5) + 64;
        revBufL.assign (maxRevSamples, 0.0f);
        revBufR.assign (maxRevSamples, 0.0f);
        revWritePos = 0;
        revPhase = 0.0;

        const int maxCaptureSamples = static_cast<int> (sampleRate * 3.0) + 64;
        captureBufL.assign (maxCaptureSamples, 0.0f);
        captureBufR.assign (maxCaptureSamples, 0.0f);
        captureWritePos = 0;
        captureReadPos = 0.0;
        captureLength = static_cast<int> (sampleRate * 0.45);
        wasCapturing = false;

        const int maxBrokenSamples = static_cast<int> (sampleRate * 0.5) + 64;
        brokenBufL.assign (maxBrokenSamples, 0.0f);
        brokenBufR.assign (maxBrokenSamples, 0.0f);
        brokenWritePos = 0;

        juce::Reverb::Parameters revParams;
        revParams.roomSize = 0.82f;
        revParams.damping = 0.38f;
        revParams.wetLevel = 1.0f;
        revParams.dryLevel = 0.0f;
        revParams.width = 1.0f;
        spaceReverb.setParameters (revParams);
        spaceReverb.reset();

        resetState();
    }

    void resetState()
    {
        envFollower = 0.0f;
        swellEnv = 0.0f;
        squashEnv = 0.0f;
        howlFilterL.reset();
        howlFilterR.reset();
        textFilterL.reset();
        textFilterR.reset();
        charTiltLP_L = charTiltLP_R = 0.0f;
        dcBlockX_L = dcBlockY_L = dcBlockX_R = dcBlockY_R = 0.0f;

        lfoPhase = 0.0;
        driftPhase1 = 0.0;
        driftPhase2 = 0.37;
        pitchPhase = 0.0;
        phaserStagesL.fill (0.0f);
        phaserStagesR.fill (0.0f);
        phaserFbL = phaserFbR = 0.0f;

        diffLpL = diffLpR = 0.0f;
        diffHpL = diffHpR = 0.0f;
        diffPrevInL = diffPrevInR = 0.0f;
        collageSlicePhase = 0.0;

        cassetteLpL = cassetteLpR = 0.0f;
        cassetteFlutterPhase = 0.0;
        brokenHoldL = brokenHoldR = 0.0f;
        brokenCounter = 0;
        interferencePhase = 0.0;
        noiseBurstEnv = 0.0f;
    }

    static std::array<int, 4> getRoutingOrder (int preset)
    {
        switch (preset)
        {
            case 0:  return { 0, 1, 2, 3 };
            case 1:  return { 3, 0, 1, 2 };
            case 2:  return { 2, 0, 1, 3 };
            case 3:  return { 1, 2, 0, 3 };
            case 4:  return { 0, 2, 1, 3 };
            case 5:  return { 3, 2, 1, 0 };
            default: return { 0, 1, 2, 3 };
        }
    }

    void processBlock (juce::AudioBuffer<float>& buffer, const Parameters& params)
    {
        if (params.bypass)
            return;

        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0)
            return;

        if (moveDelayL.empty() || diffDelayL.empty() || revBufL.empty() || captureBufL.empty() || brokenBufL.empty())
            prepare (sampleRate, numSamples);

        float* left  = buffer.getWritePointer (0);
        float* right = (numChannels > 1) ? buffer.getWritePointer (1) : nullptr;

        const float calGains[4] = { 0.65f, 1.0f, 1.55f, 2.2f };
        const float calGain = calGains[juce::jlimit (0, 3, params.calibration)];
        const float outGain = juce::jlimit (0.0f, 1.5f, params.outputLevel / 0.75f);
        const auto order = getRoutingOrder (params.routingPreset);

        if (params.captureActive && ! wasCapturing)
        {
            captureReadPos = 0.0;
            double beatSec = 60.0 / juce::jlimit (40.0, 260.0, params.bpm);
            captureLength = juce::jlimit (2048, static_cast<int> (captureBufL.size()) - 16,
                                          static_cast<int> (sampleRate * beatSec * 0.5));
        }
        wasCapturing = params.captureActive;

        // Update Space reverb parameters once per block (avoids per-sample zipper/recalc)
        if (params.diffType == Diff_Space)
        {
            juce::Reverb::Parameters rp;
            rp.roomSize = juce::jlimit (0.30f, 0.94f, 0.40f + juce::jlimit (0.0f, 1.0f, params.diffTime) * 0.52f);
            rp.damping  = juce::jlimit (0.18f, 0.75f, 0.45f - juce::jlimit (0.0f, 1.0f, params.diffDrift) * 0.22f);
            rp.wetLevel = 1.0f;
            rp.dryLevel = 0.0f;
            rp.width    = 1.0f;
            spaceReverb.setParameters (rp);
        }

        for (int i = 0; i < numSamples; ++i)
        {
            const float dryL = sanitizeAudio (left[i]);
            const float dryR = (right != nullptr) ? sanitizeAudio (right[i]) : dryL;

            float sigL = dryL * calGain;
            float sigR = dryR * calGain;

            const int capSize = static_cast<int> (captureBufL.size());
            if (! params.captureActive)
            {
                captureWritePos = safeWrap (captureWritePos, capSize);
                captureBufL[captureWritePos] = sigL;
                captureBufR[captureWritePos] = sigR;
                captureWritePos = safeWrap (captureWritePos + 1, capSize);
            }
            else if (captureLength > 64)
            {
                int baseIdx = safeWrap (captureWritePos - captureLength, capSize);
                int rInt = static_cast<int> (captureReadPos);
                int posA = safeWrap (baseIdx + rInt, capSize);
                int posB = safeWrap (baseIdx + ((rInt + captureLength / 2) % captureLength), capSize);

                float phaseNorm = juce::jlimit (0.0f, 1.0f, static_cast<float> (captureReadPos) / static_cast<float> (captureLength));
                float winA = std::sin (phaseNorm * juce::MathConstants<float>::pi);
                winA *= winA;
                float winB = 1.0f - winA;

                float capL = captureBufL[posA] * winA + captureBufL[posB] * winB;
                float capR = captureBufR[posA] * winA + captureBufR[posB] * winB;

                captureReadPos += 1.0;
                if (captureReadPos >= captureLength || captureReadPos < 0.0)
                    captureReadPos = 0.0;

                sigL = sigL * 0.35f + capL * 0.85f;
                sigR = sigR * 0.35f + capR * 0.85f;
            }

            // Organic Drift LFO
            driftPhase1 += 0.43 / sampleRate;
            if (driftPhase1 >= 1.0) driftPhase1 -= 1.0;
            driftPhase2 += 1.17 / sampleRate;
            if (driftPhase2 >= 1.0) driftPhase2 -= 1.0;

            const float driftVal = 0.65f * std::sin (static_cast<float> (driftPhase1 * juce::MathConstants<double>::twoPi))
                                 + 0.35f * std::sin (static_cast<float> (driftPhase2 * juce::MathConstants<double>::twoPi));

            for (int slot = 0; slot < 4; ++slot)
            {
                switch (order[slot])
                {
                    case 0: processCharacter (sigL, sigR, params); break;
                    case 1: processMovement  (sigL, sigR, params, driftVal); break;
                    case 2: processDiffusion (sigL, sigR, params, driftVal); break;
                    case 3: processTexture   (sigL, sigR, params, driftVal); break;
                    default: break;
                }
                sigL = sanitizeAudio (sigL);
                sigR = sanitizeAudio (sigR);
            }

            // Soft console transformer output limiting
            sigL = std::tanh (sigL * 0.85f) * (1.15f * outGain);
            sigR = std::tanh (sigR * 0.85f) * (1.15f * outGain);

            const float mix = juce::jlimit (0.0f, 1.0f, params.masterMix);
            left[i] = sanitizeAudio (dryL * (1.0f - mix) + sigL * mix, 2.0f);
            if (right != nullptr)
                right[i] = sanitizeAudio (dryR * (1.0f - mix) + sigR * mix, 2.0f);
        }
    }

private:
    static inline float readInterpSafe (const std::vector<float>& buf, int writePos, float delaySamples)
    {
        const int bufSize = static_cast<int> (buf.size());
        if (bufSize < 4) return 0.0f;
        if (! std::isfinite (delaySamples)) delaySamples = 1.0f;
        delaySamples = juce::jlimit (1.0f, static_cast<float> (bufSize - 4), delaySamples);

        float readPos = static_cast<float> (writePos) - delaySamples;
        while (readPos < 0.0f) readPos += static_cast<float> (bufSize);
        int idx0 = safeWrap (static_cast<int> (readPos), bufSize);
        int idx1 = safeWrap (idx0 + 1, bufSize);
        float frac = juce::jlimit (0.0f, 1.0f, readPos - std::floor (readPos));
        return sanitizeAudio (buf[idx0] * (1.0f - frac) + buf[idx1] * frac);
    }

    //==============================================================================
    // MODULE 1: CHARACTER (Red)
    //==============================================================================
    void processCharacter (float& l, float& r, const Parameters& p)
    {
        if (p.charType == Char_Off || p.charAmount <= 0.001f)
            return;

        const float amt     = juce::jlimit (0.0f, 1.0f, p.charAmount);
        const float tilt    = juce::jlimit (0.0f, 1.0f, p.charTilt);
        const float volGain = juce::jlimit (0.0f, 1.4f, p.charVol / 0.75f);

        const float tiltAlpha = std::exp (-2.0f * juce::MathConstants<float>::pi * 950.0f / static_cast<float> (sampleRate));
        charTiltLP_L = sanitizeAudio ((1.0f - tiltAlpha) * l + tiltAlpha * charTiltLP_L);
        charTiltLP_R = sanitizeAudio ((1.0f - tiltAlpha) * r + tiltAlpha * charTiltLP_R);
        const float hpL = l - charTiltLP_L;
        const float hpR = r - charTiltLP_R;

        const float lowGain  = juce::jmap (tilt, 1.45f, 0.55f);
        const float highGain = juce::jmap (tilt, 0.55f, 1.55f);

        float inL = sanitizeAudio (charTiltLP_L * lowGain + hpL * highGain);
        float inR = sanitizeAudio (charTiltLP_R * lowGain + hpR * highGain);

        float outL = inL;
        float outR = inR;

        switch (p.charType)
        {
            case Char_Drive:
            {
                float drive = 1.0f + amt * 8.5f;
                auto tubeClip = [drive] (float x)
                {
                    float d = juce::jlimit (-12.0f, 12.0f, x * drive);
                    // Bounded asymmetric warmth (no unbounded d*d blowup!)
                    float asym = d + 0.18f * std::tanh (d) * std::tanh (d);
                    float norm = 1.0f / juce::jmax (0.4f, std::tanh (1.0f + drive * 0.25f));
                    return std::tanh (asym) * norm * 0.85f;
                };
                outL = tubeClip (inL);
                outR = tubeClip (inR);
                break;
            }
            case Char_Sweeten:
            {
                float absPeak = 0.5f * (std::fabs (inL) + std::fabs (inR));
                float att = (absPeak > envFollower) ? 0.015f : 0.0004f;
                envFollower = sanitizeAudio (envFollower + att * (absPeak - envFollower));

                float compGain = 1.0f / (1.0f + amt * 2.4f * envFollower);
                float makeup = 1.0f + amt * 0.75f;
                float sL = inL * compGain * makeup;
                float sR = inR * compGain * makeup;
                outL = std::tanh (sL * (1.0f + amt * 1.3f)) + std::tanh (hpL) * amt * 0.3f;
                outR = std::tanh (sR * (1.0f + amt * 1.3f)) + std::tanh (hpR) * amt * 0.3f;
                break;
            }
            case Char_Fuzz:
            {
                float fuzzGain = 2.5f + amt * 28.0f;
                auto fuzzShape = [fuzzGain, amt] (float x)
                {
                    float gate = (std::fabs (x) < 0.003f * amt) ? 0.2f : 1.0f;
                    float v = juce::jlimit (-25.0f, 25.0f, x * fuzzGain * gate);
                    float stage1 = v / (1.0f + std::fabs (v));
                    return std::tanh (stage1 * (2.2f + amt * 2.5f)) * 0.65f;
                };
                outL = fuzzShape (inL);
                outR = fuzzShape (inR);
                break;
            }
            case Char_Howl:
            {
                // Unconditionally stable TPT SVF resonant feedback distortion
                float absPeak = 0.5f * (std::fabs (l) + std::fabs (r));
                envFollower = sanitizeAudio (envFollower + 0.004f * (absPeak - envFollower));

                float sweep = juce::jlimit (0.0f, 1.0f, tilt * 0.72f + envFollower * 0.55f);
                float cutoffHz = 260.0f * std::pow (11.0f, sweep);
                float res = 0.65f + amt * 0.28f;

                float lpL, bpL, hpHowlL;
                float lpR, bpR, hpHowlR;
                howlFilterL.process (l, cutoffHz, res, static_cast<float> (sampleRate), lpL, bpL, hpHowlL);
                howlFilterR.process (r, cutoffHz, res, static_cast<float> (sampleRate), lpR, bpR, hpHowlR);

                float drive = 1.6f + amt * 10.0f;
                outL = std::tanh ((l + bpL * (1.2f + amt * 2.2f)) * drive) * 0.7f;
                outR = std::tanh ((r + bpR * (1.2f + amt * 2.2f)) * drive) * 0.7f;
                break;
            }
            case Char_Swell:
            {
                float absPeak = 0.5f * (std::fabs (l) + std::fabs (r));
                // Lowered threshold so even at max Tilt/Amount your signal never gets stuck gated out!
                float threshold = juce::jmap (1.0f - tilt, 0.0015f, 0.045f);
                float target = (absPeak > threshold) ? 1.0f : 0.0f;

                float attackTimeSec = 0.03f + amt * 0.95f;
                float releaseTimeSec = 0.12f;
                float coeff = (target > swellEnv)
                            ? (1.0f - std::exp (-1.0f / (attackTimeSec * static_cast<float> (sampleRate))))
                            : (1.0f - std::exp (-1.0f / (releaseTimeSec * static_cast<float> (sampleRate))));
                swellEnv = juce::jlimit (0.0f, 1.0f, swellEnv + coeff * (target - swellEnv));

                // Keep a subtle dry floor at extreme settings so it never feels dead
                float gain = juce::jmax (0.04f, swellEnv * swellEnv);
                outL = l * gain;
                outR = r * gain;
                break;
            }
            default: break;
        }

        // DC blocker
        float dcL = sanitizeAudio (outL - dcBlockX_L + 0.995f * dcBlockY_L);
        dcBlockX_L = outL; dcBlockY_L = dcL;
        float dcR = sanitizeAudio (outR - dcBlockX_R + 0.995f * dcBlockY_R);
        dcBlockX_R = outR; dcBlockY_R = dcR;

        l = (l * (1.0f - amt * 0.35f) + dcL * (0.35f + amt * 0.65f)) * volGain;
        r = (r * (1.0f - amt * 0.35f) + dcR * (0.35f + amt * 0.65f)) * volGain;
    }

    //==============================================================================
    // MODULE 2: MOVEMENT (Yellow)
    //==============================================================================
    void processMovement (float& l, float& r, const Parameters& p, float driftVal)
    {
        if (p.moveType == Move_Off || p.moveAmount <= 0.001f)
            return;

        const float amt      = juce::jlimit (0.0f, 1.0f, p.moveAmount);
        const float rateNorm = juce::jlimit (0.0f, 1.0f, p.moveRate);
        const float drift    = juce::jlimit (0.0f, 1.0f, p.moveDrift) * driftVal;
        const float volGain  = juce::jlimit (0.0f, 1.4f, p.moveVol / 0.75f);

        const int bufSize = static_cast<int> (moveDelayL.size());
        moveWritePos = safeWrap (moveWritePos, bufSize);
        moveDelayL[moveWritePos] = sanitizeAudio (l);
        moveDelayR[moveWritePos] = sanitizeAudio (r);

        float lfoHz = juce::jlimit (0.05f, 18.0f, 0.12f * std::pow (55.0f, rateNorm) * (1.0f + drift * 0.2f));
        lfoPhase += lfoHz / sampleRate;
        lfoPhase -= std::floor (lfoPhase);

        float lfoSinL = std::sin (static_cast<float> (lfoPhase * juce::MathConstants<double>::twoPi));
        float lfoSinR = std::cos (static_cast<float> (lfoPhase * juce::MathConstants<double>::twoPi));

        float outL = l;
        float outR = r;

        switch (p.moveType)
        {
            case Move_Doubler:
            {
                float baseDelayMs = 8.0f + rateNorm * 26.0f;
                float modMs = (1.2f + amt * 3.0f) * (lfoSinL * 0.7f + drift * 0.4f);
                float dSampL = (baseDelayMs + modMs) * 0.001f * static_cast<float> (sampleRate);
                float dSampR = (baseDelayMs * 1.18f - modMs) * 0.001f * static_cast<float> (sampleRate);

                float dblL = readInterpSafe (moveDelayL, moveWritePos, dSampL);
                float dblR = readInterpSafe (moveDelayR, moveWritePos, dSampR);

                outL = l * (1.0f - amt * 0.35f) + dblL * (amt * 0.85f);
                outR = r * (1.0f - amt * 0.35f) + dblR * (amt * 0.85f);
                break;
            }
            case Move_Vibrato:
            {
                float centerMs = 7.0f;
                float depthMs = amt * 4.0f;
                float modL = (centerMs + depthMs * (lfoSinL + drift * 0.3f)) * 0.001f * static_cast<float> (sampleRate);
                float modR = (centerMs + depthMs * (lfoSinL * 0.85f + lfoSinR * 0.15f + drift * 0.3f)) * 0.001f * static_cast<float> (sampleRate);

                outL = readInterpSafe (moveDelayL, moveWritePos, modL);
                outR = readInterpSafe (moveDelayR, moveWritePos, modR);
                break;
            }
            case Move_Phaser:
            {
                float sweepNorm = juce::jlimit (0.02f, 0.98f, 0.5f + 0.48f * (lfoSinL * 0.85f + drift * 0.2f));
                float minFreq = 180.0f, maxFreq = 2600.0f;
                float fSweep = minFreq * std::pow (maxFreq / minFreq, sweepNorm);
                float tanVal = std::tan (juce::MathConstants<float>::pi * juce::jmin (fSweep, 8000.0f) / static_cast<float> (sampleRate));
                float coeff  = juce::jlimit (-0.95f, 0.95f, (1.0f - tanVal) / (1.0f + tanVal));

                float fb = amt * 0.58f;
                float xL = sanitizeAudio (l + phaserFbL * fb);
                float xR = sanitizeAudio (r + phaserFbR * fb);

                for (int s = 0; s < 6; ++s)
                {
                    float yL = -coeff * xL + phaserStagesL[s];
                    phaserStagesL[s] = sanitizeAudio (xL + coeff * yL);
                    xL = yL;

                    float yR = -coeff * xR + phaserStagesR[s];
                    phaserStagesR[s] = sanitizeAudio (xR + coeff * yR);
                    xR = yR;
                }
                phaserFbL = std::tanh (xL);
                phaserFbR = std::tanh (xR);

                outL = l * (1.0f - amt * 0.45f) + xL * (amt * 0.75f);
                outR = r * (1.0f - amt * 0.45f) + xR * (amt * 0.75f);
                break;
            }
            case Move_Tremolo:
            {
                float modL = 0.5f + 0.5f * std::tanh (1.6f * lfoSinL);
                float modR = 0.5f + 0.5f * std::tanh (1.6f * (lfoSinL * (1.0f - amt * 0.5f) + lfoSinR * (amt * 0.5f)));
                // Keep a 10% minimum floor at max Amount so audio never cuts out completely
                float gainL = 1.0f - (amt * 0.9f) * (1.0f - modL);
                float gainR = 1.0f - (amt * 0.9f) * (1.0f - modR);
                outL = l * gainL;
                outR = r * gainR;
                break;
            }
            case Move_Pitch:
            {
                const float semitonesTable[7] = { -12.0f, -7.0f, -5.0f, 0.12f, 5.0f, 7.0f, 12.0f };
                int idx = juce::jlimit (0, 6, static_cast<int> (rateNorm * 6.99f));
                float semitones = semitonesTable[idx] + drift * 0.25f;
                float ratio = std::pow (2.0f, semitones / 12.0f);

                const float grainLen = 0.045f * static_cast<float> (sampleRate);
                pitchPhase += (1.0 - static_cast<double> (ratio)) / grainLen;
                pitchPhase -= std::floor (pitchPhase);

                float phA = juce::jlimit (0.0f, 1.0f, static_cast<float> (pitchPhase));
                float phB = std::fmod (phA + 0.5f, 1.0f);

                float winA = std::sin (phA * juce::MathConstants<float>::pi);
                winA *= winA;
                float winB = 1.0f - winA;

                float pL = readInterpSafe (moveDelayL, moveWritePos, phA * grainLen + 4.0f) * winA
                         + readInterpSafe (moveDelayL, moveWritePos, phB * grainLen + 4.0f) * winB;
                float pR = readInterpSafe (moveDelayR, moveWritePos, phA * grainLen + 4.0f) * winA
                         + readInterpSafe (moveDelayR, moveWritePos, phB * grainLen + 4.0f) * winB;

                outL = l * (1.0f - amt) + pL * amt;
                outR = r * (1.0f - amt) + pR * amt;
                break;
            }
            default: break;
        }

        moveWritePos = safeWrap (moveWritePos + 1, bufSize);
        l = outL * volGain;
        r = outR * volGain;
    }

    //==============================================================================
    // MODULE 3: DIFFUSION (Green)
    //==============================================================================
    void processDiffusion (float& l, float& r, const Parameters& p, float driftVal)
    {
        if (p.diffType == Diff_Off || p.diffAmount <= 0.001f)
            return;

        const float amt      = juce::jlimit (0.0f, 1.0f, p.diffAmount);
        const float timeNorm = juce::jlimit (0.0f, 1.0f, p.diffTime);
        const float drift    = juce::jlimit (0.0f, 1.0f, p.diffDrift) * driftVal;
        const float volGain  = juce::jlimit (0.0f, 1.4f, p.diffVol / 0.75f);

        const int bufSize = static_cast<int> (diffDelayL.size());
        diffWritePos = safeWrap (diffWritePos, bufSize);

        float delaySec = 0.055f + timeNorm * timeNorm * 1.15f;
        float wowSamp  = drift * 0.0025f * static_cast<float> (sampleRate);
        float baseSamp = juce::jlimit (32.0f, static_cast<float> (bufSize - 64),
                                       delaySec * static_cast<float> (sampleRate) + wowSamp);

        float wetL = 0.0f, wetR = 0.0f;

        switch (p.diffType)
        {
            case Diff_Cascade:
            {
                float tap1L = readInterpSafe (diffDelayL, diffWritePos, baseSamp * 0.5f);
                float tap2R = readInterpSafe (diffDelayR, diffWritePos, baseSamp * 0.75f);
                float tap3L = readInterpSafe (diffDelayL, diffWritePos, baseSamp * 1.0f);
                float tap4R = readInterpSafe (diffDelayR, diffWritePos, baseSamp * 1.333f);

                wetL = (tap1L * 0.50f + tap3L * 0.40f + tap2R * 0.25f);
                wetR = (tap2R * 0.50f + tap4R * 0.40f + tap3L * 0.25f);

                float fb = juce::jlimit (0.15f, 0.84f, 0.22f + amt * 0.60f);
                diffLpL = sanitizeAudio (0.45f * wetL + 0.55f * diffLpL);
                diffLpR = sanitizeAudio (0.45f * wetR + 0.55f * diffLpR);

                diffDelayL[diffWritePos] = std::tanh (l + diffLpR * fb);
                diffDelayR[diffWritePos] = std::tanh (r + diffLpL * fb);
                break;
            }
            case Diff_Reels:
            {
                float tapeL = readInterpSafe (diffDelayL, diffWritePos, baseSamp);
                float tapeR = readInterpSafe (diffDelayR, diffWritePos, baseSamp * 1.015f);

                diffLpL = sanitizeAudio (0.28f * tapeL + 0.72f * diffLpL);
                diffLpR = sanitizeAudio (0.28f * tapeR + 0.72f * diffLpR);

                diffHpL = sanitizeAudio (0.985f * (diffHpL + diffLpL - diffPrevInL));
                diffHpR = sanitizeAudio (0.985f * (diffHpR + diffLpR - diffPrevInR));
                diffPrevInL = diffLpL;
                diffPrevInR = diffLpR;

                wetL = std::tanh (diffHpL * 1.35f);
                wetR = std::tanh (diffHpR * 1.35f);

                float fb = juce::jlimit (0.15f, 0.86f, 0.20f + amt * 0.65f);
                diffDelayL[diffWritePos] = std::tanh (l + wetL * fb);
                diffDelayR[diffWritePos] = std::tanh (r + wetR * fb);
                break;
            }
            case Diff_Space:
            {
                float preDelaySamp = (0.018f + timeNorm * 0.045f) * static_cast<float> (sampleRate) + wowSamp * 0.4f;
                diffDelayL[diffWritePos] = sanitizeAudio (l);
                diffDelayR[diffWritePos] = sanitizeAudio (r);
                wetL = sanitizeAudio (readInterpSafe (diffDelayL, diffWritePos, preDelaySamp));
                wetR = sanitizeAudio (readInterpSafe (diffDelayR, diffWritePos, preDelaySamp * 1.12f));

                spaceReverb.processStereo (&wetL, &wetR, 1);
                wetL = sanitizeAudio (wetL);
                wetR = sanitizeAudio (wetR);
                break;
            }
            case Diff_Collage:
            {
                collageSlicePhase += (1.5 / juce::jmax (0.08f, delaySec)) / sampleRate;
                collageSlicePhase -= std::floor (collageSlicePhase);

                float ph = juce::jlimit (0.0f, 1.0f, static_cast<float> (collageSlicePhase));
                float win = std::sin (ph * juce::MathConstants<float>::pi);
                win *= win;

                float tapNormL = readInterpSafe (diffDelayL, diffWritePos, baseSamp * 0.75f);
                float tapOctL  = readInterpSafe (diffDelayL, diffWritePos, baseSamp * 0.5f * (1.0f - ph * 0.45f)) * win;
                float tapNormR = readInterpSafe (diffDelayR, diffWritePos, baseSamp * 1.0f);
                float tapOctR  = readInterpSafe (diffDelayR, diffWritePos, baseSamp * 0.66f * (1.0f - ph * 0.45f)) * win;

                wetL = tapNormL * 0.55f + tapOctL * 0.65f;
                wetR = tapNormR * 0.55f + tapOctR * 0.65f;

                float fb = juce::jlimit (0.18f, 0.82f, 0.22f + amt * 0.56f);
                diffDelayL[diffWritePos] = std::tanh (l + wetR * fb);
                diffDelayR[diffWritePos] = std::tanh (r + wetL * fb);
                break;
            }
            case Diff_Reverse:
            {
                const int revSize = static_cast<int> (revBufL.size());
                revWritePos = safeWrap (revWritePos, revSize);
                revBufL[revWritePos] = sanitizeAudio (l);
                revBufR[revWritePos] = sanitizeAudio (r);

                float winSamples = juce::jlimit (2048.0f, static_cast<float> (revSize - 64), baseSamp);
                revPhase += 2.0 / winSamples;
                revPhase -= std::floor (revPhase);

                auto readRevWindow = [&] (float phaseOffset, float& oL, float& oR)
                {
                    float ph = std::fmod (static_cast<float> (revPhase) + phaseOffset, 1.0f);
                    if (ph < 0.0f) ph += 1.0f;
                    float env = std::sin (ph * juce::MathConstants<float>::pi);
                    env *= env;
                    float delay = ph * winSamples + 8.0f;
                    oL = readInterpSafe (revBufL, revWritePos, delay) * env;
                    oR = readInterpSafe (revBufR, revWritePos, delay) * env;
                };

                float aL, aR, bL, bR;
                readRevWindow (0.0f, aL, aR);
                readRevWindow (0.5f, bL, bR);
                wetL = aL + bL;
                wetR = aR + bR;

                revWritePos = safeWrap (revWritePos + 1, revSize);
                diffDelayL[diffWritePos] = sanitizeAudio (l);
                diffDelayR[diffWritePos] = sanitizeAudio (r);
                break;
            }
            default: break;
        }

        diffWritePos = safeWrap (diffWritePos + 1, bufSize);

        float wetMix = amt * 0.85f;
        l = (l * (1.0f - wetMix * 0.35f) + wetL * wetMix) * volGain;
        r = (r * (1.0f - wetMix * 0.35f) + wetR * wetMix) * volGain;
    }

    //==============================================================================
    // MODULE 4: TEXTURE (Blue)
    //==============================================================================
    void processTexture (float& l, float& r, const Parameters& p, float driftVal)
    {
        if (p.textType == Text_Off || p.textAmount <= 0.001f)
            return;

        const float amt     = juce::jlimit (0.0f, 1.0f, p.textAmount);
        const float tilt    = juce::jlimit (0.0f, 1.0f, p.textTilt);
        const float volGain = juce::jlimit (0.0f, 1.4f, p.textVol / 0.75f);

        float outL = l;
        float outR = r;

        switch (p.textType)
        {
            case Text_Filter:
            {
                // Unconditionally Stable Cytomic TPT SVF (LPF -> Tilt -> HPF)
                // Keep cutoff in musical range [110 Hz .. 14,000 Hz] so even at max Tilt it never mutes audio
                float cutoffHz = 110.0f * std::pow (125.0f, tilt);
                float res = amt * 0.72f;

                float lowL, bandL, highL;
                float lowR, bandR, highR;
                textFilterL.process (l, cutoffHz, res, static_cast<float> (sampleRate), lowL, bandL, highL);
                textFilterR.process (r, cutoffHz, res, static_cast<float> (sampleRate), lowR, bandR, highR);

                // Even at extreme HPF (tilt=1.0), blend 25% bandpass/body so low instruments don't vanish
                float filtL = (tilt < 0.5f)
                            ? juce::jmap (tilt * 2.0f, lowL + bandL * 0.2f, l)
                            : juce::jmap ((tilt - 0.5f) * 2.0f, l, highL + bandL * 0.35f);
                float filtR = (tilt < 0.5f)
                            ? juce::jmap (tilt * 2.0f, lowR + bandR * 0.2f, r)
                            : juce::jmap ((tilt - 0.5f) * 2.0f, r, highR + bandR * 0.35f);

                outL = l * (1.0f - amt) + filtL * amt;
                outR = r * (1.0f - amt) + filtR * amt;
                break;
            }
            case Text_Squash:
            {
                float peak = 0.5f * (std::fabs (l) + std::fabs (r));
                float att = (peak > squashEnv) ? 0.04f : (0.0003f + tilt * 0.002f);
                squashEnv = sanitizeAudio (squashEnv + att * (peak - squashEnv));

                float thresh = juce::jmap (amt, 0.35f, 0.05f);
                float gain = (squashEnv > thresh) ? (thresh / (squashEnv + 0.001f)) : 1.0f;
                gain = std::pow (juce::jlimit (0.05f, 4.0f, gain), 0.5f + amt * 0.4f);
                float makeup = 1.0f + amt * 1.9f;

                outL = std::tanh (l * gain * makeup);
                outR = std::tanh (r * gain * makeup);
                break;
            }
            case Text_Cassette:
            {
                cassetteFlutterPhase += (5.2 + tilt * 3.5) / sampleRate;
                cassetteFlutterPhase -= std::floor (cassetteFlutterPhase);

                const int bSize = static_cast<int> (brokenBufL.size());
                brokenWritePos = safeWrap (brokenWritePos, bSize);
                brokenBufL[brokenWritePos] = sanitizeAudio (l);
                brokenBufR[brokenWritePos] = sanitizeAudio (r);

                float flutterMs = 1.5f + amt * 2.0f * (0.6f * std::sin (static_cast<float> (cassetteFlutterPhase * juce::MathConstants<double>::twoPi)) + 0.4f * driftVal);
                float dSamp = juce::jlimit (2.0f, 512.0f, flutterMs * 0.001f * static_cast<float> (sampleRate));

                float wL = readInterpSafe (brokenBufL, brokenWritePos, dSamp);
                float wR = readInterpSafe (brokenBufR, brokenWritePos, dSamp);
                brokenWritePos = safeWrap (brokenWritePos + 1, bSize);

                float lpFreq = juce::jlimit (900.0f, 14000.0f, 1800.0f + tilt * 8500.0f * (1.0f - amt * 0.45f));
                float alpha  = std::exp (-2.0f * juce::MathConstants<float>::pi * lpFreq / static_cast<float> (sampleRate));
                cassetteLpL = sanitizeAudio ((1.0f - alpha) * std::tanh (wL * (1.2f + amt * 1.6f)) + alpha * cassetteLpL);
                cassetteLpR = sanitizeAudio ((1.0f - alpha) * std::tanh (wR * (1.2f + amt * 1.6f)) + alpha * cassetteLpR);

                float hiss = nextWhiteNoise() * 0.0015f * amt;
                outL = cassetteLpL + hiss;
                outR = cassetteLpR + hiss;
                break;
            }
            case Text_Broken:
            {
                const int bSize = static_cast<int> (brokenBufL.size());
                brokenWritePos = safeWrap (brokenWritePos, bSize);
                brokenBufL[brokenWritePos] = sanitizeAudio (l);
                brokenBufR[brokenWritePos] = sanitizeAudio (r);

                int holdPeriod = juce::jmax (1, static_cast<int> (1.0f + (1.0f - tilt) * amt * 24.0f));
                if (++brokenCounter >= holdPeriod)
                {
                    brokenCounter = 0;
                    float steps = juce::jmax (8.0f, 64.0f - amt * 52.0f);
                    brokenHoldL = std::round (l * steps) / steps;
                    brokenHoldR = std::round (r * steps) / steps;
                }

                // Sputter never completely silences the signal (min 0.28f)
                float sputter = (std::sin (driftVal * 9.0f) < (-0.85f + amt * 0.35f)) ? 0.28f : 1.0f;
                outL = (l * (1.0f - amt) + brokenHoldL * amt) * sputter;
                outR = (r * (1.0f - amt) + brokenHoldR * amt) * sputter;
                brokenWritePos = safeWrap (brokenWritePos + 1, bSize);
                break;
            }
            case Text_Interference:
            {
                float carrierHz = juce::jlimit (80.0f, 4000.0f, 180.0f + tilt * 2400.0f + driftVal * 95.0f);
                interferencePhase += carrierHz / sampleRate;
                interferencePhase -= std::floor (interferencePhase);

                float carrier = std::sin (static_cast<float> (interferencePhase * juce::MathConstants<double>::twoPi));
                float sigEnv = 0.5f * (std::fabs (l) + std::fabs (r));
                noiseBurstEnv = sanitizeAudio (0.992f * noiseBurstEnv + 0.008f * sigEnv);

                float staticNoise = nextWhiteNoise() * noiseBurstEnv * 0.40f;
                outL = l * (1.0f - amt * 0.42f + carrier * amt * 0.42f) + staticNoise * amt;
                outR = r * (1.0f - amt * 0.42f + carrier * amt * 0.42f) + staticNoise * amt;
                break;
            }
            default: break;
        }

        l = outL * volGain;
        r = outR * volGain;
    }

    float nextWhiteNoise()
    {
        rngState = rngState * 1664525u + 1013904223u;
        return (static_cast<float> (rngState & 0x00FFFFFFu) / static_cast<float> (0x00800000u)) - 1.0f;
    }

    double sampleRate = 44100.0;
    uint32_t rngState = 22222u;

    std::vector<float> moveDelayL, moveDelayR;
    int moveWritePos = 0;

    std::vector<float> diffDelayL, diffDelayR;
    int diffWritePos = 0;

    std::vector<float> revBufL, revBufR;
    int revWritePos = 0;
    double revPhase = 0.0;

    std::vector<float> captureBufL, captureBufR;
    int captureWritePos = 0;
    double captureReadPos = 0.0;
    int captureLength = 16000;
    bool wasCapturing = false;

    std::vector<float> brokenBufL, brokenBufR;
    int brokenWritePos = 0;

    juce::Reverb spaceReverb;
    StableSVF howlFilterL, howlFilterR;
    StableSVF textFilterL, textFilterR;

    float envFollower = 0.0f;
    float swellEnv = 0.0f;
    float squashEnv = 0.0f;
    float charTiltLP_L = 0.0f, charTiltLP_R = 0.0f;
    float dcBlockX_L = 0.0f, dcBlockY_L = 0.0f, dcBlockX_R = 0.0f, dcBlockY_R = 0.0f;

    double lfoPhase = 0.0;
    double driftPhase1 = 0.0, driftPhase2 = 0.37;
    double pitchPhase = 0.0;
    std::array<float, 6> phaserStagesL { 0.0f }, phaserStagesR { 0.0f };
    float phaserFbL = 0.0f, phaserFbR = 0.0f;

    float diffLpL = 0.0f, diffLpR = 0.0f;
    float diffHpL = 0.0f, diffHpR = 0.0f;
    float diffPrevInL = 0.0f, diffPrevInR = 0.0f;
    double collageSlicePhase = 0.0;

    float cassetteLpL = 0.0f, cassetteLpR = 0.0f;
    double cassetteFlutterPhase = 0.0;
    float brokenHoldL = 0.0f, brokenHoldR = 0.0f;
    int brokenCounter = 0;
    double interferencePhase = 0.0;
    float noiseBurstEnv = 0.0f;
};

} // namespace ChromaCosm
