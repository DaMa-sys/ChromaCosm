#pragma once
#include "ChromaEngine.h"
#include <atomic>

namespace ChromaCosm
{

//==============================================================================
// MICROCOSM DSP ENGINE
// 11 Effects x 4 Variations (A, B, C, D) = 44 Granular / Micro-Loop / Glitch / Multidelay Algorithms
// Plus 4-Mode Space Reverb, Unconditionally Stable Resonant Filter, Pitch Mod, Hold Sampler & 60s Phrase Looper
//==============================================================================
class MicrocosmEngine
{
public:
    enum EffectType
    {
        FX_Mosaic = 0,
        FX_Seq,
        FX_Glide,
        FX_Haze,
        FX_Tunnel,
        FX_Strum,
        FX_Blocks,
        FX_Interrupt,
        FX_Arp,
        FX_Pattern,
        FX_Warp,
        FX_Count
    };

    enum LooperState
    {
        Looper_Empty = 0,
        Looper_Recording,
        Looper_Playing,
        Looper_Overdubbing,
        Looper_Stopped
    };

    struct GrainVisual
    {
        float posNorm = 0.0f;
        float pitch = 1.0f;
        float env = 0.0f;
        float pan = 0.5f;
        bool active = false;
    };

    struct Parameters
    {
        int effectType = FX_Mosaic;
        int variation = 0;
        float time = 0.5f;
        int subdiv = 2;
        bool tempoSync = true;
        float activity = 0.55f;
        float repeats = 0.5f;
        float shape = 0.5f;
        float filter = 0.65f;
        float filterRes = 0.25f;
        float mix = 0.5f;
        float space = 0.45f;
        int spaceMode = 1;
        float pitchModRate = 0.35f;
        float pitchModDepth = 0.25f;
        bool reverse = false;
        bool holdSampler = false;
        bool bypass = false;

        // Phrase Looper
        float loopLevel = 0.8f;
        int loopSpeed = 1;
        bool loopReverse = false;
        bool loopPreFX = false;
        double bpm = 120.0;
    };

    static constexpr int kMaxGrains = 16;

    void prepare (double newSampleRate, int /*maxBlockSize*/)
    {
        sampleRate = (newSampleRate > 1000.0) ? newSampleRate : 44100.0;

        historySize = static_cast<int> (sampleRate * 8.0) + 128;
        historyL.assign (historySize, 0.0f);
        historyR.assign (historySize, 0.0f);
        historyWritePos = 0;

        maxLooperSamples = static_cast<int> (sampleRate * 40.0);
        looperBufL.assign (maxLooperSamples, 0.0f);
        looperBufR.assign (maxLooperSamples, 0.0f);
        looperLength = 0;
        looperWritePos = 0;
        looperReadPos = 0.0;
        looperState.store (Looper_Empty);

        for (auto& g : grains)
            g.active = false;

        juce::Reverb::Parameters rp;
        rp.roomSize = 0.85f;
        rp.damping = 0.3f;
        rp.wetLevel = 1.0f;
        rp.dryLevel = 0.0f;
        rp.width = 1.0f;
        reverbEngine.setParameters (rp);
        reverbEngine.reset();

        microFilterL.reset();
        microFilterR.reset();

        shimmerPhase = 0.0;
        spawnCounter = 0;
        stepCounter = 0;
        currentStep = 0;
        pitchModPhase = 0.0;
        shapeLfoPhase = 0.0;
        onsetEnvFast = onsetEnvSlow = 0.0f;
        onsetCooldown = 0;
        fbSampleL = fbSampleR = 0.0f;
        duckEnv = 0.0f;
    }

    void triggerLooperRecordOrPlay()
    {
        int st = looperState.load();
        if (st == Looper_Empty || st == Looper_Stopped)
        {
            if (looperLength == 0)
            {
                looperWritePos = 0;
                looperReadPos = 0.0;
                looperState.store (Looper_Recording);
            }
            else
            {
                looperReadPos = 0.0;
                looperState.store (Looper_Playing);
            }
        }
        else if (st == Looper_Recording)
        {
            looperLength = juce::jmax (1024, looperWritePos);
            looperReadPos = 0.0;
            looperState.store (Looper_Playing);
        }
        else if (st == Looper_Playing)
        {
            looperState.store (Looper_Overdubbing);
        }
        else if (st == Looper_Overdubbing)
        {
            looperState.store (Looper_Playing);
        }
    }

    void triggerLooperStop()
    {
        int st = looperState.load();
        if (st == Looper_Recording)
            looperLength = juce::jmax (1024, looperWritePos);
        if (looperLength > 0)
            looperState.store (Looper_Stopped);
        else
            looperState.store (Looper_Empty);
    }

    void triggerLooperClear()
    {
        looperState.store (Looper_Empty);
        looperLength = 0;
        looperWritePos = 0;
        looperReadPos = 0.0;
        std::fill (looperBufL.begin(), looperBufL.end(), 0.0f);
        std::fill (looperBufR.begin(), looperBufR.end(), 0.0f);
    }

    int getLooperState() const { return looperState.load(); }
    float getLooperProgress() const
    {
        if (looperLength <= 0) return 0.0f;
        return juce::jlimit (0.0f, 1.0f, static_cast<float> (looperReadPos / static_cast<double> (looperLength)));
    }

    std::array<GrainVisual, kMaxGrains> getGrainVisuals() const
    {
        std::array<GrainVisual, kMaxGrains> out;
        for (int i = 0; i < kMaxGrains; ++i)
        {
            out[i].active = grains[i].active;
            out[i].posNorm = grains[i].posNorm;
            out[i].pitch = grains[i].rate;
            out[i].env = grains[i].lastEnv;
            out[i].pan = grains[i].pan;
        }
        return out;
    }

    void processBlock (juce::AudioBuffer<float>& buffer, const Parameters& p)
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0)
            return;

        if (historyL.empty() || looperBufL.empty())
            prepare (sampleRate, numSamples);

        float* left  = buffer.getWritePointer (0);
        float* right = (numChannels > 1) ? buffer.getWritePointer (1) : nullptr;

        const float subdivMults[6] = { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 0.6667f };
        const float subdivMult = subdivMults[juce::jlimit (0, 5, p.subdiv)];
        double beatSec = p.tempoSync
                       ? (60.0 / juce::jlimit (40.0, 260.0, p.bpm)) * subdivMult
                       : (0.08 + juce::jlimit (0.0f, 1.0f, p.time) * 1.8) * subdivMult;
        beatSec = juce::jlimit (0.045, 3.2, beatSec);

        // Configure 4-mode Space reverb parameters once per block
        {
            juce::Reverb::Parameters rp;
            const float modeRoom[4] = { 0.50f, 0.74f, 0.88f, 0.94f };
            const float modeDamp[4] = { 0.55f, 0.38f, 0.24f, 0.16f };
            int sm = juce::jlimit (0, 3, p.spaceMode);
            rp.roomSize = juce::jlimit (0.25f, 0.95f, modeRoom[sm] * (0.75f + juce::jlimit (0.0f, 1.0f, p.space) * 0.25f));
            rp.damping  = modeDamp[sm];
            rp.wetLevel = 1.0f;
            rp.dryLevel = 0.0f;
            rp.width    = 1.0f;
            reverbEngine.setParameters (rp);
        }

        const float act = juce::jlimit (0.0f, 1.0f, p.activity);
        const double densityFactor = 0.20 + (1.0 - act) * 1.25;
        int spawnInterval = juce::jmax (192, static_cast<int> (sampleRate * beatSec * 0.25 * densityFactor));
        int stepInterval  = juce::jmax (256, static_cast<int> (sampleRate * beatSec * 0.25));

        for (int i = 0; i < numSamples; ++i)
        {
            float inL = sanitizeAudio (left[i]);
            float inR = (right != nullptr) ? sanitizeAudio (right[i]) : inL;

            if (p.loopPreFX)
                processLooperSample (inL, inR, p);

            if (p.bypass)
            {
                if (! p.loopPreFX)
                    processLooperSample (inL, inR, p);
                left[i] = sanitizeAudio (inL, 2.0f);
                if (right != nullptr) right[i] = sanitizeAudio (inR, 2.0f);
                continue;
            }

            // Transient onset detector
            float absIn = 0.5f * (std::fabs (inL) + std::fabs (inR));
            onsetEnvFast = sanitizeAudio (0.92f * onsetEnvFast + 0.08f * absIn);
            onsetEnvSlow = sanitizeAudio (0.9992f * onsetEnvSlow + 0.0008f * absIn);
            bool onsetTriggered = (onsetEnvFast > onsetEnvSlow * 2.4f + 0.015f) && (onsetCooldown == 0);
            if (onsetCooldown > 0) --onsetCooldown;
            if (onsetTriggered) onsetCooldown = static_cast<int> (sampleRate * 0.075);

            // Safe feedback gain (capped at 0.78 with soft clipping so Repeats=100% sustains lushly without runaway!)
            float fbGain = juce::jlimit (0.0f, 0.78f, p.repeats * 0.78f);
            if (! p.holdSampler)
            {
                historyWritePos = safeWrap (historyWritePos, historySize);
                historyL[historyWritePos] = std::tanh (inL + sanitizeAudio (fbSampleL) * fbGain);
                historyR[historyWritePos] = std::tanh (inR + sanitizeAudio (fbSampleR) * fbGain);
                historyWritePos = safeWrap (historyWritePos + 1, historySize);
            }

            if (++stepCounter >= stepInterval)
            {
                stepCounter = 0;
                currentStep = (currentStep + 1) & 7;
            }

            if (++spawnCounter >= spawnInterval || (onsetTriggered && (p.effectType == FX_Strum || p.effectType == FX_Interrupt)))
            {
                spawnCounter = 0;
                spawnGrainForEffect (p, beatSec, onsetTriggered);
            }

            float pmHz = 0.15f + juce::jlimit (0.0f, 1.0f, p.pitchModRate) * 4.5f;
            pitchModPhase += pmHz / sampleRate;
            pitchModPhase -= std::floor (pitchModPhase);
            float pitchModRatio = juce::jlimit (0.95f, 1.05f,
                1.0f + std::sin (static_cast<float> (pitchModPhase * juce::MathConstants<double>::twoPi))
                     * (juce::jlimit (0.0f, 1.0f, p.pitchModDepth) * 0.024f));

            shapeLfoPhase += (0.25 / beatSec) / sampleRate;
            shapeLfoPhase -= std::floor (shapeLfoPhase);

            float wetL = 0.0f;
            float wetR = 0.0f;
            renderActiveGrains (wetL, wetR, p, pitchModRatio);

            if (p.effectType == FX_Pattern || p.effectType == FX_Warp)
                renderMultiDelayTaps (wetL, wetR, p, beatSec, pitchModRatio);

            wetL = sanitizeAudio (wetL);
            wetR = sanitizeAudio (wetR);

            // Unconditionally Stable Cytomic TPT SVF (Microcosm Filter Knob + Shape modulation)
            float shapeMod = (juce::jlimit (0.0f, 1.0f, p.shape) - 0.5f) * 0.22f
                           * std::sin (static_cast<float> (shapeLfoPhase * juce::MathConstants<double>::twoPi));
            float filtNorm = juce::jlimit (0.02f, 0.98f, p.filter + shapeMod);
            float cutoffHz = 120.0f * std::pow (110.0f, filtNorm); // [120 Hz .. 13,200 Hz]
            float res      = juce::jlimit (0.0f, 0.85f, p.filterRes * 0.82f);

            float lowL, bandL, highL;
            float lowR, bandR, highR;
            microFilterL.process (wetL, cutoffHz, res, static_cast<float> (sampleRate), lowL, bandL, highL);
            microFilterR.process (wetR, cutoffHz, res, static_cast<float> (sampleRate), lowR, bandR, highR);

            if (filtNorm < 0.65f)
            {
                float t = filtNorm / 0.65f;
                wetL = (lowL + bandL * 0.2f) * (1.0f - t * 0.25f) + wetL * (t * 0.25f);
                wetR = (lowR + bandR * 0.2f) * (1.0f - t * 0.25f) + wetR * (t * 0.25f);
            }
            else
            {
                float t = (filtNorm - 0.65f) / 0.35f;
                wetL = wetL * (1.0f - t) + (highL + bandL * 0.35f) * t;
                wetR = wetR * (1.0f - t) + (highR + bandR * 0.35f) * t;
            }

            wetL = std::tanh (sanitizeAudio (wetL) * 0.85f) * 1.15f;
            wetR = std::tanh (sanitizeAudio (wetR) * 0.85f) * 1.15f;

            fbSampleL = wetL;
            fbSampleR = wetR;

            // Built-in 4-Mode Space Reverb
            const float spaceAmt = juce::jlimit (0.0f, 1.0f, p.space);
            if (spaceAmt > 0.005f)
            {
                float revL = sanitizeAudio (wetL + inL * 0.22f);
                float revR = sanitizeAudio (wetR + inR * 0.22f);
                reverbEngine.processStereo (&revL, &revR, 1);
                revL = sanitizeAudio (revL);
                revR = sanitizeAudio (revR);

                if (p.spaceMode == 3)
                {
                    shimmerPhase += 1.0 / (0.055 * sampleRate);
                    shimmerPhase -= std::floor (shimmerPhase);
                    float sWin = std::sin (static_cast<float> (shimmerPhase * juce::MathConstants<double>::pi));
                    float origL = revL;
                    revL = sanitizeAudio (revL + revR * sWin * 0.30f);
                    revR = sanitizeAudio (revR + origL * sWin * 0.30f);
                }

                wetL = wetL * (1.0f - spaceAmt * 0.35f) + revL * (spaceAmt * 0.90f);
                wetR = wetR * (1.0f - spaceAmt * 0.35f) + revR * (spaceAmt * 0.90f);
            }

            // Smooth envelope-followed ducking for Interrupt mode
            float dryDuck = 1.0f;
            if (p.effectType == FX_Interrupt)
            {
                float wetMag = 0.5f * (std::fabs (wetL) + std::fabs (wetR));
                duckEnv = sanitizeAudio (0.992f * duckEnv + 0.008f * wetMag);
                dryDuck = juce::jlimit (0.35f, 1.0f, 1.0f - duckEnv * act * 0.65f);
            }

            const float mix = juce::jlimit (0.0f, 1.0f, p.mix);
            float outL = inL * (1.0f - mix) * dryDuck + wetL * mix;
            float outR = inR * (1.0f - mix) * dryDuck + wetR * mix;

            if (! p.loopPreFX)
                processLooperSample (outL, outR, p);

            left[i] = sanitizeAudio (outL, 2.0f);
            if (right != nullptr)
                right[i] = sanitizeAudio (outR, 2.0f);
        }
    }

private:
    struct Grain
    {
        bool active = false;
        double readPos = 0.0;
        double phase = 0.0;
        double phaseInc = 0.001;
        float rate = 1.0f;
        float glideTargetRate = 1.0f;
        float pan = 0.5f;
        float gain = 0.7f;
        float posNorm = 0.5f;
        float lastEnv = 0.0f;
    };

    void spawnGrainForEffect (const Parameters& p, double beatSec, bool isOnset)
    {
        if (p.activity <= 0.005f)
            return;

        int slot = -1;
        double maxPhase = -1.0;
        int stealSlot = 0;
        for (int i = 0; i < kMaxGrains; ++i)
        {
            if (! grains[i].active)
            {
                slot = i;
                break;
            }
            if (grains[i].phase > maxPhase)
            {
                maxPhase = grains[i].phase;
                stealSlot = i;
            }
        }
        if (slot < 0)
            slot = stealSlot;

        auto& g = grains[slot];
        g.active = true;
        g.phase = 0.0;

        const int var = juce::jlimit (0, 3, p.variation);
        float rnd1 = nextRandom01();
        float rnd2 = nextRandom01();

        double grainDurSec = 0.12 + beatSec * 0.45;
        double delayOffsetSec = 0.04 + rnd1 * beatSec * (0.35 + var * 0.25);
        float pitchRate = 1.0f;
        float targetRate = 1.0f;
        float pan = 0.15f + 0.7f * rnd2;
        float gain = 0.50f + 0.30f * juce::jlimit (0.0f, 1.0f, p.activity);

        switch (p.effectType)
        {
            case FX_Mosaic:
            {
                const float mosaicRates[4][4] = {
                    { 1.0f, 2.0f, 1.0f, 0.5f },
                    { 2.0f, 2.0f, 1.0f, 1.5f },
                    { 0.5f, 1.0f, 0.5f, 2.0f },
                    { 0.5f, 0.75f, 1.0f, 1.5f }
                };
                pitchRate = mosaicRates[var][currentStep & 3];
                targetRate = pitchRate;
                grainDurSec = beatSec * (0.35 + 0.15 * (currentStep & 1));
                delayOffsetSec = beatSec * (0.25 + 0.25 * (currentStep & 3));
                break;
            }
            case FX_Seq:
            {
                const float seqRates[4][8] = {
                    { 1.0f, 1.0f, 2.0f, 1.0f, 0.5f, 1.0f, 1.5f, 1.0f },
                    { 1.0f, 1.5f, 2.0f, 1.5f, 1.0f, 0.5f, 1.0f, 2.0f },
                    { 2.0f, 1.0f, 0.5f, 1.0f, 2.0f, 1.5f, 1.0f, 0.5f },
                    { 1.0f, 1.25f, 1.5f, 2.0f, 1.5f, 1.25f, 1.0f, 0.5f }
                };
                pitchRate = seqRates[var][currentStep & 7];
                targetRate = pitchRate;
                grainDurSec = beatSec * 0.28;
                delayOffsetSec = beatSec * (0.125 * (1 + ((currentStep + var) & 7)));
                pan = (currentStep & 1) ? 0.2f : 0.8f;
                break;
            }
            case FX_Glide:
            {
                const float startRates[4] = { 1.0f, 0.5f, 1.0f, 2.0f };
                const float endRates[4]   = { 1.5f, 1.0f, 0.5f, 1.0f };
                pitchRate = startRates[var];
                targetRate = endRates[var];
                grainDurSec = beatSec * (0.55 + 0.2 * var);
                delayOffsetSec = beatSec * (0.2 + 0.3 * rnd1);
                break;
            }
            case FX_Haze:
            {
                const float hazeDetune[4] = { 0.004f, 0.012f, 0.5f, 1.0f };
                pitchRate = (var == 2 && (rnd1 > 0.65f)) ? 0.5f
                          : (var == 3 && (rnd1 > 0.6f))  ? 2.0f
                          : (1.0f + (rnd1 - 0.5f) * hazeDetune[var]);
                targetRate = pitchRate;
                grainDurSec = 0.22 + beatSec * (0.65 + 0.35 * rnd2);
                delayOffsetSec = 0.05 + rnd1 * beatSec * 1.4;
                break;
            }
            case FX_Tunnel:
            {
                const double tunnelFreqs[4] = { 55.0, 82.41, 110.0, 146.83 };
                grainDurSec = (1.0 / tunnelFreqs[var]) * (4.0 + (currentStep & 3) * 2.0);
                pitchRate = (var & 1) ? 0.5f : 1.0f;
                targetRate = pitchRate;
                delayOffsetSec = grainDurSec * (1.0 + rnd1 * 2.0);
                break;
            }
            case FX_Strum:
            {
                const float strumIntervals[4] = { 1.0f, 1.5f, 2.0f, 0.5f };
                pitchRate = strumIntervals[(slot + var) & 3];
                targetRate = pitchRate;
                grainDurSec = 0.14 + 0.08 * (slot & 3);
                delayOffsetSec = 0.03 + 0.06 * (slot & 7);
                if (isOnset) gain *= 1.2f;
                break;
            }
            case FX_Blocks:
            {
                grainDurSec = beatSec * (0.125 * (1 + (var & 1)));
                delayOffsetSec = grainDurSec * (1 + (currentStep & 3));
                pitchRate = (var >= 2 && (currentStep & 2)) ? 2.0f : 1.0f;
                targetRate = pitchRate;
                break;
            }
            case FX_Interrupt:
            {
                const float intRates[4] = { 2.0f, 0.5f, 1.5f, 1.0f };
                pitchRate = intRates[(currentStep + var) & 3];
                targetRate = pitchRate * (var == 3 ? 0.75f : 1.0f);
                grainDurSec = beatSec * (0.18 + 0.12 * rnd1);
                delayOffsetSec = 0.02 + rnd2 * beatSec * 0.4;
                break;
            }
            case FX_Arp:
            {
                const float arpChords[4][4] = {
                    { 1.0f, 1.25f, 1.5f, 2.0f },
                    { 1.0f, 1.1892f, 1.4983f, 1.7818f },
                    { 0.5f, 1.0f, 1.5f, 2.0f },
                    { 1.0f, 1.1225f, 1.4983f, 2.0f }
                };
                pitchRate = arpChords[var][currentStep & 3];
                targetRate = pitchRate;
                grainDurSec = beatSec * 0.25;
                delayOffsetSec = beatSec * 0.25 * (1 + (currentStep & 3));
                break;
            }
            case FX_Pattern:
            case FX_Warp:
            default:
            {
                pitchRate = (p.effectType == FX_Warp) ? (0.98f + 0.04f * rnd1) : 1.0f;
                targetRate = (p.effectType == FX_Warp) ? (pitchRate * (var & 1 ? 1.5f : 0.99f)) : 1.0f;
                grainDurSec = beatSec * 0.45;
                delayOffsetSec = beatSec * (0.333 * (1 + var));
                gain *= 0.6f;
                break;
            }
        }

        double dir = p.reverse ? -1.0 : 1.0;
        g.rate = static_cast<float> (dir) * pitchRate;
        g.glideTargetRate = static_cast<float> (dir) * targetRate;
        g.pan = juce::jlimit (0.0f, 1.0f, pan);
        g.gain = juce::jlimit (0.0f, 1.0f, gain);

        double durSamples = juce::jmax (256.0, grainDurSec * sampleRate);
        g.phaseInc = 1.0 / durSamples;

        double offsetSamples = juce::jlimit (64.0, static_cast<double> (historySize - 64),
                                             delayOffsetSec * sampleRate);
        g.readPos = static_cast<double> (historyWritePos) - offsetSamples;
        while (g.readPos < 0.0)
            g.readPos += static_cast<double> (historySize);
        while (g.readPos >= historySize)
            g.readPos -= static_cast<double> (historySize);

        g.posNorm = juce::jlimit (0.0f, 1.0f, static_cast<float> (g.readPos / static_cast<double> (historySize)));
    }

    void renderActiveGrains (float& outL, float& outR, const Parameters& p, float pitchModRatio)
    {
        for (int i = 0; i < kMaxGrains; ++i)
        {
            auto& g = grains[i];
            if (! g.active)
                continue;

            float ph = juce::jlimit (0.0f, 1.0f, static_cast<float> (g.phase));
            float s = std::sin (ph * juce::MathConstants<float>::pi);
            float env = s * s;
            if (p.shape < 0.35f)
            {
                float a = 0.08f;
                env = (ph < a) ? (ph / a) : std::cos ((ph - a) / (1.0f - a) * juce::MathConstants<float>::halfPi);
            }
            else if (p.shape > 0.65f)
            {
                env = std::pow (juce::jmax (0.0f, s), 2.2f);
            }
            g.lastEnv = env;

            if (! std::isfinite (g.readPos))
                g.readPos = 0.0;

            int idx0 = safeWrap (static_cast<int> (g.readPos), historySize);
            int idx1 = safeWrap (idx0 + 1, historySize);
            float frac = juce::jlimit (0.0f, 1.0f, static_cast<float> (g.readPos - std::floor (g.readPos)));

            float sampL = historyL[idx0] * (1.0f - frac) + historyL[idx1] * frac;
            float sampR = historyR[idx0] * (1.0f - frac) + historyR[idx1] * frac;

            float amp = env * g.gain * 0.38f;
            float panL = std::cos (g.pan * juce::MathConstants<float>::halfPi);
            float panR = std::sin (g.pan * juce::MathConstants<float>::halfPi);

            outL += sampL * amp * panL;
            outR += sampR * amp * panR;

            float curRate = g.rate + (g.glideTargetRate - g.rate) * ph;
            g.readPos += static_cast<double> (curRate * pitchModRatio);
            while (g.readPos >= historySize) g.readPos -= historySize;
            while (g.readPos < 0.0)          g.readPos += historySize;

            g.posNorm = juce::jlimit (0.0f, 1.0f, static_cast<float> (g.readPos / static_cast<double> (historySize)));

            g.phase += g.phaseInc;
            if (g.phase >= 1.0)
            {
                g.active = false;
                g.lastEnv = 0.0f;
            }
        }
    }

    void renderMultiDelayTaps (float& outL, float& outR, const Parameters& p, double beatSec, float pitchModRatio)
    {
        const float tapRatios[4][4] = {
            { 0.25f, 0.50f, 0.75f, 1.00f },
            { 0.333f, 0.666f, 1.00f, 1.333f },
            { 0.382f, 0.618f, 1.00f, 1.618f },
            { 0.1875f, 0.375f, 0.75f, 1.125f }
        };
        int var = juce::jlimit (0, 3, p.variation);
        float warpOffset = (p.effectType == FX_Warp)
                         ? std::sin (static_cast<float> (shapeLfoPhase * juce::MathConstants<double>::twoPi)) * 0.012f
                         : 0.0f;
        float safeRatio = juce::jmax (0.5f, pitchModRatio);

        for (int t = 0; t < 4; ++t)
        {
            double tapDelaySamples = beatSec * (tapRatios[var][t] + warpOffset * (t + 1)) * sampleRate / safeRatio;
            tapDelaySamples = juce::jlimit (16.0, static_cast<double> (historySize - 32), tapDelaySamples);

            double rPos = static_cast<double> (historyWritePos) - tapDelaySamples;
            while (rPos < 0.0) rPos += historySize;
            int i0 = safeWrap (static_cast<int> (rPos), historySize);
            int i1 = safeWrap (i0 + 1, historySize);
            float frac = juce::jlimit (0.0f, 1.0f, static_cast<float> (rPos - std::floor (rPos)));

            float tL = historyL[i0] * (1.0f - frac) + historyL[i1] * frac;
            float tR = historyR[i0] * (1.0f - frac) + historyR[i1] * frac;

            float tapGain = (0.38f - t * 0.06f) * (0.4f + 0.6f * juce::jlimit (0.0f, 1.0f, p.activity));
            if (t & 1)
            {
                outL += tR * tapGain * 0.7f;
                outR += tL * tapGain;
            }
            else
            {
                outL += tL * tapGain;
                outR += tR * tapGain * 0.7f;
            }
        }
    }

    void processLooperSample (float& l, float& r, const Parameters& p)
    {
        int st = looperState.load();
        if (st == Looper_Empty || st == Looper_Stopped)
            return;

        if (st == Looper_Recording)
        {
            if (looperWritePos < maxLooperSamples)
            {
                looperBufL[looperWritePos] = sanitizeAudio (l);
                looperBufR[looperWritePos] = sanitizeAudio (r);
                ++looperWritePos;
                looperLength = looperWritePos;
                looperReadPos = static_cast<double> (looperWritePos);
            }
            else
            {
                looperLength = maxLooperSamples;
                looperReadPos = 0.0;
                looperState.store (Looper_Playing);
            }
            return;
        }

        if (looperLength <= 0)
            return;

        if (! std::isfinite (looperReadPos))
            looperReadPos = 0.0;

        int i0 = safeWrap (static_cast<int> (looperReadPos), looperLength);
        int i1 = safeWrap (i0 + 1, looperLength);
        float frac = juce::jlimit (0.0f, 1.0f, static_cast<float> (looperReadPos - std::floor (looperReadPos)));

        float loopL = looperBufL[i0] * (1.0f - frac) + looperBufL[i1] * frac;
        float loopR = looperBufR[i0] * (1.0f - frac) + looperBufR[i1] * frac;

        if (st == Looper_Overdubbing)
        {
            looperBufL[i0] = std::tanh (looperBufL[i0] * 0.92f + sanitizeAudio (l));
            looperBufR[i0] = std::tanh (looperBufR[i0] * 0.92f + sanitizeAudio (r));
        }

        float lvl = juce::jlimit (0.0f, 1.2f, p.loopLevel);
        l += loopL * lvl;
        r += loopR * lvl;

        const double speeds[3] = { 0.5, 1.0, 2.0 };
        double inc = speeds[juce::jlimit (0, 2, p.loopSpeed)] * (p.loopReverse ? -1.0 : 1.0);
        looperReadPos += inc;
        while (looperReadPos >= looperLength) looperReadPos -= looperLength;
        while (looperReadPos < 0.0)           looperReadPos += looperLength;
    }

    float nextRandom01()
    {
        rngState = rngState * 1664525u + 1013904223u;
        return static_cast<float> (rngState & 0x00FFFFFFu) / static_cast<float> (0x01000000u);
    }

    double sampleRate = 44100.0;
    uint32_t rngState = 987654321u;

    std::vector<float> historyL, historyR;
    int historySize = 352800;
    int historyWritePos = 0;

    std::array<Grain, kMaxGrains> grains;
    juce::Reverb reverbEngine;
    StableSVF microFilterL, microFilterR;
    double shimmerPhase = 0.0;

    int spawnCounter = 0;
    int stepCounter = 0;
    int currentStep = 0;

    double pitchModPhase = 0.0;
    double shapeLfoPhase = 0.0;
    float onsetEnvFast = 0.0f, onsetEnvSlow = 0.0f;
    int onsetCooldown = 0;
    float fbSampleL = 0.0f, fbSampleR = 0.0f;
    float duckEnv = 0.0f;

    std::vector<float> looperBufL, looperBufR;
    int maxLooperSamples = 1764000;
    int looperLength = 0;
    int looperWritePos = 0;
    double looperReadPos = 0.0;
    std::atomic<int> looperState { Looper_Empty };
};

} // namespace ChromaCosm
