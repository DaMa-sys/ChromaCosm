#pragma once
#include "ChromaEngine.h"
#include <atomic>

namespace ChromaCosm
{

//==============================================================================
// AUTHENTIC MICROCOSM DSP ENGINE
// Faithfully modeled from Hologram Electronics architecture & official demos:
// - 4 Shape Contours: Square (Chopped) -> Ramp (Decay) -> Triangle -> Sine (Ambient Pad)
// - True Macro Controls: Activity (How much / how often) vs Repeats (Lifespan / Persistence)
// - Polyrhythmic Mosaic (Synchronized 0.5x, 1x, 2x playback heads)
// - Dynamic Strum (Transient-triggered harp-like cascade bursts)
// - Clean, musical buffer slicing with zero clicks and no chaotic clutter
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
        int subdiv = 2; // 0=1/4, 1=1/2, 2=1x, 3=2x, 4=4x, 5=TRIP
        bool tempoSync = true;
        float activity = 0.55f;
        float repeats = 0.5f;
        float shape = 0.5f; // 0..0.25 Square, 0.25..0.50 Ramp, 0.50..0.75 Triangle, 0.75..1.0 Sine
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

    static constexpr int kMaxGrains = 24;

    void prepare (double newSampleRate, int /*maxBlockSize*/)
    {
        sampleRate = (newSampleRate > 1000.0) ? newSampleRate : 44100.0;

        // 8-second circular sample buffer
        historySize = static_cast<int> (sampleRate * 8.0) + 128;
        historyL.assign (historySize, 0.0f);
        historyR.assign (historySize, 0.0f);
        historyWritePos = 0;

        // 40-second phrase looper
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
        rp.damping = 0.35f;
        rp.wetLevel = 1.0f;
        rp.dryLevel = 0.0f;
        rp.width = 1.0f;
        reverbEngine.setParameters (rp);
        reverbEngine.reset();

        microFilterL.reset();
        microFilterR.reset();

        shimmerPhase = 0.0;
        stepCounter = 0;
        currentStep = 0;
        pitchModPhase = 0.0;
        onsetEnvFast = onsetEnvSlow = 0.0f;
        onsetCooldown = 0;
        duckEnv = 0.0f;
        strumCounter = 0;
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

    std::array<GrainVisual, 16> getGrainVisuals() const
    {
        std::array<GrainVisual, 16> out;
        for (int i = 0; i < 16; ++i)
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

        // Rhythmic base beat in seconds
        const float subdivMults[6] = { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 0.6667f };
        const float subdivMult = subdivMults[juce::jlimit (0, 5, p.subdiv)];
        double beatSec = p.tempoSync
                       ? (60.0 / juce::jlimit (40.0, 260.0, p.bpm)) * subdivMult
                       : (0.08 + juce::jlimit (0.0f, 1.0f, p.time) * 1.8) * subdivMult;
        beatSec = juce::jlimit (0.05, 3.0, beatSec);

        // Update Reverb Space Parameters
        {
            juce::Reverb::Parameters rp;
            const float modeRoom[4] = { 0.45f, 0.68f, 0.84f, 0.94f };
            const float modeDamp[4] = { 0.55f, 0.38f, 0.24f, 0.16f };
            int sm = juce::jlimit (0, 3, p.spaceMode);
            rp.roomSize = juce::jlimit (0.25f, 0.96f, modeRoom[sm] * (0.75f + juce::jlimit (0.0f, 1.0f, p.space) * 0.25f));
            rp.damping  = modeDamp[sm];
            rp.wetLevel = 1.0f;
            rp.dryLevel = 0.0f;
            rp.width    = 1.0f;
            reverbEngine.setParameters (rp);
        }

        // Clock Grid interval (1/16th note or triplet subdivision)
        int stepInterval = juce::jmax (128, static_cast<int> (sampleRate * beatSec * 0.25));

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
                left[i] = inL;
                if (right != nullptr) right[i] = inR;
                continue;
            }

            // High-precision transient onset detector (for Strum, Glitch, Interrupt)
            float absIn = 0.5f * (std::fabs (inL) + std::fabs (inR));
            onsetEnvFast = sanitizeAudio (0.85f * onsetEnvFast + 0.15f * absIn);
            onsetEnvSlow = sanitizeAudio (0.999f * onsetEnvSlow + 0.001f * absIn);
            bool onsetTriggered = (onsetEnvFast > onsetEnvSlow * 2.6f + 0.018f) && (onsetCooldown == 0);
            if (onsetCooldown > 0) --onsetCooldown;
            if (onsetTriggered)
            {
                onsetCooldown = static_cast<int> (sampleRate * 0.08);
                if (p.effectType == FX_Strum)
                    triggerStrumCascade (p, beatSec);
            }

            // Write incoming audio cleanly into rolling buffer (unless Hold is frozen)
            if (! p.holdSampler)
            {
                historyWritePos = safeWrap (historyWritePos, historySize);
                historyL[historyWritePos] = inL;
                historyR[historyWritePos] = inR;
                historyWritePos = safeWrap (historyWritePos + 1, historySize);
            }

            // Advance Step Clock & Spawner
            if (++stepCounter >= stepInterval)
            {
                stepCounter = 0;
                currentStep = (currentStep + 1) & 15;
                spawnGrainsOnGrid (p, beatSec);
            }

            // Pitch Modulation LFO
            float pmHz = 0.15f + juce::jlimit (0.0f, 1.0f, p.pitchModRate) * 4.0f;
            pitchModPhase += pmHz / sampleRate;
            pitchModPhase -= std::floor (pitchModPhase);
            float pitchModRatio = 1.0f + std::sin (static_cast<float> (pitchModPhase * juce::MathConstants<double>::twoPi))
                                       * (juce::jlimit (0.0f, 1.0f, p.pitchModDepth) * 0.022f);

            // Render all active grains with Shape envelope contour
            float wetL = 0.0f, wetR = 0.0f;
            renderGrains (wetL, wetR, p, pitchModRatio);

            // If Pattern / Warp, render dedicated rhythmic delay taps
            if (p.effectType == FX_Pattern || p.effectType == FX_Warp)
                renderMultiDelay (wetL, wetR, p, beatSec, pitchModRatio);

            wetL = sanitizeAudio (wetL);
            wetR = sanitizeAudio (wetR);

            // Shape Resonant Low-Pass / High-Pass Tone Filter
            // Tuned like the real Microcosm filter: warm lowpass from 180Hz to 11kHz
            float filtNorm = juce::jlimit (0.02f, 0.98f, p.filter);
            float cutoffHz = 160.0f * std::pow (68.0f, filtNorm);
            float res = juce::jlimit (0.0f, 0.85f, p.filterRes * 0.80f);

            float lowL, bandL, highL, lowR, bandR, highR;
            microFilterL.process (wetL, cutoffHz, res, static_cast<float> (sampleRate), lowL, bandL, highL);
            microFilterR.process (wetR, cutoffHz, res, static_cast<float> (sampleRate), lowR, bandR, highR);

            if (filtNorm < 0.70f)
            {
                wetL = lowL + bandL * 0.15f;
                wetR = lowR + bandR * 0.15f;
            }
            else
            {
                float t = (filtNorm - 0.70f) / 0.30f;
                wetL = (lowL + bandL * 0.15f) * (1.0f - t) + highL * t;
                wetR = (lowR + bandR * 0.15f) * (1.0f - t) + highR * t;
            }

            // Space Reverb Processing
            const float spaceAmt = juce::jlimit (0.0f, 1.0f, p.space);
            if (spaceAmt > 0.005f)
            {
                float revL = wetL + inL * 0.18f;
                float revR = wetR + inR * 0.18f;
                reverbEngine.processStereo (&revL, &revR, 1);

                if (p.spaceMode == 3) // Shimmer Mode
                {
                    shimmerPhase += 1.0 / (0.06 * sampleRate);
                    shimmerPhase -= std::floor (shimmerPhase);
                    float sWin = std::sin (static_cast<float> (shimmerPhase * juce::MathConstants<double>::pi));
                    revL += revR * sWin * 0.28f;
                    revR += revL * sWin * 0.28f;
                }

                wetL = wetL * (1.0f - spaceAmt * 0.40f) + revL * (spaceAmt * 0.95f);
                wetR = wetR * (1.0f - spaceAmt * 0.40f) + revR * (spaceAmt * 0.95f);
            }

            // Dynamic Blooming & Auto-Ducking
            // Dips the wet cloud slightly when a new note strikes, allowing clean transients
            float dryEnv = 0.5f * (std::fabs (inL) + std::fabs (inR));
            duckEnv = sanitizeAudio (0.994f * duckEnv + 0.006f * dryEnv);
            float bloomGain = juce::jlimit (0.55f, 1.0f, 1.0f - duckEnv * 0.45f);
            wetL *= bloomGain;
            wetR *= bloomGain;

            float dryDuck = 1.0f;
            if (p.effectType == FX_Interrupt)
            {
                float wetMag = 0.5f * (std::fabs (wetL) + std::fabs (wetR));
                dryDuck = juce::jlimit (0.25f, 1.0f, 1.0f - wetMag * 0.85f);
            }

            const float mix = juce::jlimit (0.0f, 1.0f, p.mix);
            float outL = inL * (1.0f - mix) * dryDuck + wetL * mix;
            float outR = inR * (1.0f - mix) * dryDuck + wetR * mix;

            if (! p.loopPreFX)
                processLooperSample (outL, outR, p);

            left[i]  = sanitizeAudio (outL, 2.0f);
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
        int remainingRepeats = 0;
        double sliceDurationSec = 0.25;
        double sliceDelaySec = 0.25;
    };

    // Helper: allocate grain slot
    int findFreeGrainSlot()
    {
        int slot = -1;
        double maxPhase = -1.0;
        int oldestSlot = 0;
        for (int i = 0; i < kMaxGrains; ++i)
        {
            if (! grains[i].active)
                return i;
            if (grains[i].phase > maxPhase)
            {
                maxPhase = grains[i].phase;
                oldestSlot = i;
            }
        }
        return oldestSlot;
    }

    void spawnGrainsOnGrid (const Parameters& p, double beatSec)
    {
        if (p.activity <= 0.01f || p.effectType == FX_Strum)
            return;

        // Activity scales trigger probability and frequency on the musical grid
        float act = juce::jlimit (0.0f, 1.0f, p.activity);
        int repeatCount = static_cast<int> (1.0f + p.repeats * 8.0f); // Repeats determines how many times it loops!
        int var = juce::jlimit (0, 3, p.variation);

        // Decide if a grain spawns this step based on Activity
        bool shouldSpawn = false;
        if (act > 0.85f)
            shouldSpawn = true; // Every 1/16th
        else if (act > 0.55f)
            shouldSpawn = (currentStep & 1) == 0; // Every 1/8th
        else if (act > 0.28f)
            shouldSpawn = (currentStep & 3) == 0; // Every 1/4
        else
            shouldSpawn = (currentStep & 7) == 0; // Every 1/2 bar

        if (! shouldSpawn && nextRandom01() > act * 0.4f)
            return;

        static const float panTable[8] = { 0.15f, 0.85f, 0.28f, 0.72f, 0.10f, 0.90f, 0.35f, 0.65f };

        switch (p.effectType)
        {
            case FX_Mosaic:
            {
                // In official video: Mosaic spawns 3 synchronized playback heads at 1x, 2x, and 0.5x
                const float speedBanks[4][3] = {
                    { 1.0f, 2.0f, 0.5f },  // A: Octave Mosaic
                    { 1.0f, 2.0f, 1.5f },  // B: Shimmer 5th & Octave
                    { 0.5f, 1.0f, 0.5f },  // C: Deep Sub
                    { 1.0f, 1.5f, 2.0f }   // D: Fifth Tapestry
                };

                for (int h = 0; h < 3; ++h)
                {
                    if (h > 0 && act < 0.35f) continue; // Lower activity plays simpler single head
                    int slot = findFreeGrainSlot();
                    auto& g = grains[slot];
                    g.active = true;
                    g.phase = 0.0;
                    g.rate = (p.reverse ? -1.0f : 1.0f) * speedBanks[var][h];
                    g.glideTargetRate = g.rate;
                    g.pan = panTable[(currentStep + h * 2) & 7];
                    g.gain = (h == 0) ? 0.65f : 0.45f;
                    g.remainingRepeats = repeatCount;

                    g.sliceDurationSec = beatSec * (h == 1 ? 0.25 : 0.50);
                    g.sliceDelaySec    = beatSec * (0.25 * (1 + (currentStep & 3)));

                    double durSamples = juce::jmax (256.0, g.sliceDurationSec * sampleRate);
                    g.phaseInc = 1.0 / durSamples;

                    double offsetSamples = juce::jlimit (64.0, static_cast<double> (historySize - 64),
                                                         g.sliceDelaySec * sampleRate);
                    g.readPos = static_cast<double> (historyWritePos) - offsetSamples;
                    while (g.readPos < 0.0) g.readPos += static_cast<double> (historySize);
                }
                break;
            }

            case FX_Seq:
            {
                // 8-step quantized sequenced slice
                const float seqPitches[4][8] = {
                    { 1.0f, 1.0f, 2.0f, 1.0f, 0.5f, 1.0f, 1.5f, 1.0f },
                    { 1.0f, 1.5f, 2.0f, 1.5f, 1.0f, 0.5f, 1.0f, 2.0f },
                    { 2.0f, 1.0f, 0.5f, 1.0f, 2.0f, 1.5f, 1.0f, 0.5f },
                    { 1.0f, 1.25f, 1.5f, 2.0f, 1.5f, 1.25f, 1.0f, 0.5f }
                };
                int slot = findFreeGrainSlot();
                auto& g = grains[slot];
                g.active = true;
                g.phase = 0.0;
                float pRatio = seqPitches[var][currentStep & 7];
                g.rate = (p.reverse ? -1.0f : 1.0f) * pRatio;
                g.glideTargetRate = g.rate;
                g.pan = (currentStep & 1) ? 0.2f : 0.8f;
                g.gain = 0.65f;
                g.remainingRepeats = repeatCount;
                g.sliceDurationSec = beatSec * 0.25;
                g.sliceDelaySec    = beatSec * (0.125 * (1 + ((currentStep + var) & 7)));

                double durSamples = juce::jmax (256.0, g.sliceDurationSec * sampleRate);
                g.phaseInc = 1.0 / durSamples;
                double offsetSamples = juce::jlimit (64.0, static_cast<double> (historySize - 64), g.sliceDelaySec * sampleRate);
                g.readPos = static_cast<double> (historyWritePos) - offsetSamples;
                while (g.readPos < 0.0) g.readPos += static_cast<double> (historySize);
                break;
            }

            case FX_Glide:
            {
                const float startRates[4] = { 1.0f, 0.5f, 1.0f, 2.0f };
                const float endRates[4]   = { 1.5f, 1.0f, 0.5f, 1.0f };
                int slot = findFreeGrainSlot();
                auto& g = grains[slot];
                g.active = true;
                g.phase = 0.0;
                double dir = p.reverse ? -1.0 : 1.0;
                g.rate = static_cast<float> (dir * startRates[var]);
                g.glideTargetRate = static_cast<float> (dir * endRates[var]);
                g.pan = panTable[currentStep & 7];
                g.gain = 0.60f;
                g.remainingRepeats = repeatCount;
                g.sliceDurationSec = beatSec * (0.50 + 0.25 * var);
                g.sliceDelaySec    = beatSec * (0.25 * (1 + (currentStep & 3)));

                double durSamples = juce::jmax (256.0, g.sliceDurationSec * sampleRate);
                g.phaseInc = 1.0 / durSamples;
                double offsetSamples = juce::jlimit (64.0, static_cast<double> (historySize - 64), g.sliceDelaySec * sampleRate);
                g.readPos = static_cast<double> (historyWritePos) - offsetSamples;
                while (g.readPos < 0.0) g.readPos += static_cast<double> (historySize);
                break;
            }

            case FX_Haze:
            {
                // Multi-cluster ambient micro-wash
                const float hazeDetunes[4] = { 0.005f, 0.015f, 0.5f, 1.0f };
                int clusterCount = (act > 0.6f) ? 3 : 2;
                for (int c = 0; c < clusterCount; ++c)
                {
                    int slot = findFreeGrainSlot();
                    auto& g = grains[slot];
                    g.active = true;
                    g.phase = 0.0;
                    float detune = (var == 2 && c == 1) ? 0.5f : (var == 3 && c == 1) ? 2.0f : (1.0f + (nextRandom01() - 0.5f) * hazeDetunes[var]);
                    g.rate = (p.reverse ? -1.0f : 1.0f) * detune;
                    g.glideTargetRate = g.rate;
                    g.pan = panTable[(currentStep + c * 3) & 7];
                    g.gain = 0.45f;
                    g.remainingRepeats = repeatCount + 2;
                    g.sliceDurationSec = beatSec * (0.45 + 0.25 * c);
                    g.sliceDelaySec    = beatSec * (0.125 * (1 + ((currentStep + c) & 7)));

                    double durSamples = juce::jmax (256.0, g.sliceDurationSec * sampleRate);
                    g.phaseInc = 1.0 / durSamples;
                    double offsetSamples = juce::jlimit (64.0, static_cast<double> (historySize - 64), g.sliceDelaySec * sampleRate);
                    g.readPos = static_cast<double> (historyWritePos) - offsetSamples;
                    while (g.readPos < 0.0) g.readPos += static_cast<double> (historySize);
                }
                break;
            }

            case FX_Blocks:
            case FX_Interrupt:
            case FX_Arp:
            default:
            {
                int slot = findFreeGrainSlot();
                auto& g = grains[slot];
                g.active = true;
                g.phase = 0.0;
                float pRate = 1.0f;
                if (p.effectType == FX_Arp)
                {
                    const float arpChords[4][4] = {
                        { 1.0f, 1.25f, 1.5f, 2.0f },
                        { 1.0f, 1.1892f, 1.4983f, 1.7818f },
                        { 0.5f, 1.0f, 1.5f, 2.0f },
                        { 1.0f, 1.1225f, 1.4983f, 2.0f }
                    };
                    pRate = arpChords[var][currentStep & 3];
                }
                else if (p.effectType == FX_Interrupt)
                {
                    const float intRates[4] = { 2.0f, 0.5f, 1.5f, 1.0f };
                    pRate = intRates[(currentStep + var) & 3];
                }

                g.rate = (p.reverse ? -1.0f : 1.0f) * pRate;
                g.glideTargetRate = g.rate;
                g.pan = panTable[currentStep & 7];
                g.gain = 0.65f;
                g.remainingRepeats = repeatCount;
                g.sliceDurationSec = beatSec * 0.25;
                g.sliceDelaySec    = beatSec * (0.125 * (1 + (currentStep & 7)));

                double durSamples = juce::jmax (256.0, g.sliceDurationSec * sampleRate);
                g.phaseInc = 1.0 / durSamples;
                double offsetSamples = juce::jlimit (64.0, static_cast<double> (historySize - 64), g.sliceDelaySec * sampleRate);
                g.readPos = static_cast<double> (historyWritePos) - offsetSamples;
                while (g.readPos < 0.0) g.readPos += static_cast<double> (historySize);
                break;
            }
        }
    }

    // Strum mode: Onset-triggered harp-like cascade across the stereo field
    void triggerStrumCascade (const Parameters& p, double beatSec)
    {
        const float strumPitches[4][4] = {
            { 1.0f, 1.25f, 1.5f, 2.0f },  // Maj arpeggio
            { 1.0f, 1.5f, 2.0f, 2.5f },   // Strum 5th & Oct
            { 0.5f, 1.0f, 1.5f, 2.0f },   // Sub & Oct
            { 2.0f, 1.5f, 1.25f, 1.0f }   // Downward strum
        };
        int var = juce::jlimit (0, 3, p.variation);
        int repeatCount = static_cast<int> (1.0f + p.repeats * 6.0f);

        for (int note = 0; note < 4; ++note)
        {
            int slot = findFreeGrainSlot();
            auto& g = grains[slot];
            g.active = true;
            g.phase = 0.0;
            g.rate = (p.reverse ? -1.0f : 1.0f) * strumPitches[var][note];
            g.glideTargetRate = g.rate;
            g.pan = 0.15f + 0.70f * (static_cast<float> (note) / 3.0f); // Wide left-to-right strum sweep
            g.gain = 0.65f;
            g.remainingRepeats = repeatCount;
            g.sliceDurationSec = beatSec * 0.35;
            // Staggered cascade delay (harp effect)
            g.sliceDelaySec = beatSec * (0.04 * (note + 1));

            double durSamples = juce::jmax (256.0, g.sliceDurationSec * sampleRate);
            g.phaseInc = 1.0 / durSamples;
            double offsetSamples = juce::jlimit (64.0, static_cast<double> (historySize - 64), g.sliceDelaySec * sampleRate);
            g.readPos = static_cast<double> (historyWritePos) - offsetSamples;
            while (g.readPos < 0.0) g.readPos += static_cast<double> (historySize);
        }
    }

    // Evaluate Shape Envelope Contour:
    // 0.00..0.25 = Square (Chopped & Staccato)
    // 0.25..0.50 = Ramp (Sharp Transient Decay)
    // 0.50..0.75 = Triangle (Linear Symmetrical Crossfade)
    // 0.75..1.00 = Sine (Ambient Smooth Pad Bloom)
    float evaluateShapeEnvelope (float phaseNorm, float shapeNorm)
    {
        float ph = juce::jlimit (0.0f, 1.0f, phaseNorm);
        float s = juce::jlimit (0.0f, 1.0f, shapeNorm);

        if (s <= 0.25f)
        {
            // Square Wave / Staccato Gating with tiny 4ms edge taper (zero clicks)
            float edge = 0.04f;
            if (ph < edge) return ph / edge;
            if (ph > (1.0f - edge)) return (1.0f - ph) / edge;
            return 1.0f;
        }
        else if (s <= 0.50f)
        {
            // Ramp / Saw: fast attack, exponential natural decay
            float t = (s - 0.25f) / 0.25f;
            float att = 0.03f;
            float rawDecay = (ph < att) ? (ph / att) : std::exp (-3.5f * (ph - att));
            float squareVal = (ph < 0.04f) ? (ph / 0.04f) : (ph > 0.96f) ? (1.0f - ph) / 0.04f : 1.0f;
            return squareVal * (1.0f - t) + rawDecay * t;
        }
        else if (s <= 0.75f)
        {
            // Triangle: symmetrical linear crossfade
            float t = (s - 0.50f) / 0.25f;
            float tri = (ph < 0.5f) ? (ph * 2.0f) : (2.0f - ph * 2.0f);
            float rampVal = (ph < 0.03f) ? (ph / 0.03f) : std::exp (-3.5f * (ph - 0.03f));
            return rampVal * (1.0f - t) + tri * t;
        }
        else
        {
            // Sine / Bell curve: lush ambient pad bloom
            float t = (s - 0.75f) / 0.25f;
            float tri = (ph < 0.5f) ? (ph * 2.0f) : (2.0f - ph * 2.0f);
            float sineVal = std::sin (ph * juce::MathConstants<float>::pi);
            sineVal = sineVal * sineVal; // Smooth raised cosine bell
            return tri * (1.0f - t) + sineVal * t;
        }
    }

    void renderGrains (float& outL, float& outR, const Parameters& p, float pitchModRatio)
    {
        for (int i = 0; i < kMaxGrains; ++i)
        {
            auto& g = grains[i];
            if (! g.active)
                continue;

            float ph = static_cast<float> (g.phase);
            float env = evaluateShapeEnvelope (ph, p.shape);
            g.lastEnv = env;

            int idx0 = safeWrap (static_cast<int> (g.readPos), historySize);
            int idx1 = safeWrap (idx0 + 1, historySize);
            float frac = static_cast<float> (g.readPos - std::floor (g.readPos));

            float sL = historyL[idx0] * (1.0f - frac) + historyL[idx1] * frac;
            float sR = historyR[idx0] * (1.0f - frac) + historyR[idx1] * frac;

            float amp = env * g.gain * 0.45f;
            float panL = std::cos (g.pan * juce::MathConstants<float>::halfPi);
            float panR = std::sin (g.pan * juce::MathConstants<float>::halfPi);

            outL += sL * amp * panL;
            outR += sR * amp * panR;

            float curRate = g.rate + (g.glideTargetRate - g.rate) * ph;
            g.readPos += static_cast<double> (curRate * pitchModRatio);
            while (g.readPos >= historySize) g.readPos -= historySize;
            while (g.readPos < 0.0)          g.readPos += historySize;

            g.posNorm = static_cast<float> (g.readPos / static_cast<double> (historySize));

            g.phase += g.phaseInc;
            if (g.phase >= 1.0)
            {
                // Repeats loop mechanic: retrigger if repeats remain!
                if (--g.remainingRepeats > 0 && ! p.holdSampler)
                {
                    g.phase = 0.0;
                    g.gain *= 0.82f; // Natural musical feedback decay
                    double offsetSamples = juce::jlimit (64.0, static_cast<double> (historySize - 64),
                                                         g.sliceDelaySec * sampleRate);
                    g.readPos = static_cast<double> (historyWritePos) - offsetSamples;
                    while (g.readPos < 0.0) g.readPos += static_cast<double> (historySize);
                }
                else
                {
                    g.active = false;
                    g.lastEnv = 0.0f;
                }
            }
        }
    }

    void renderMultiDelay (float& outL, float& outR, const Parameters& p, double beatSec, float pitchModRatio)
    {
        const float tapRatios[4][4] = {
            { 0.25f, 0.50f, 0.75f, 1.00f },
            { 0.333f, 0.666f, 1.00f, 1.333f },
            { 0.382f, 0.618f, 1.00f, 1.618f },
            { 0.1875f, 0.375f, 0.75f, 1.125f }
        };
        int var = juce::jlimit (0, 3, p.variation);
        float safeRatio = juce::jmax (0.5f, pitchModRatio);

        for (int t = 0; t < 4; ++t)
        {
            double tapDelaySamples = beatSec * tapRatios[var][t] * sampleRate / safeRatio;
            tapDelaySamples = juce::jlimit (16.0, static_cast<double> (historySize - 32), tapDelaySamples);

            double rPos = static_cast<double> (historyWritePos) - tapDelaySamples;
            while (rPos < 0.0) rPos += historySize;
            int i0 = safeWrap (static_cast<int> (rPos), historySize);
            int i1 = safeWrap (i0 + 1, historySize);
            float frac = static_cast<float> (rPos - std::floor (rPos));

            float tL = historyL[i0] * (1.0f - frac) + historyL[i1] * frac;
            float tR = historyR[i0] * (1.0f - frac) + historyR[i1] * frac;

            float tapGain = (0.35f - t * 0.05f) * (0.35f + 0.65f * juce::jlimit (0.0f, 1.0f, p.activity));
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

    int stepCounter = 0;
    int currentStep = 0;
    double pitchModPhase = 0.0;
    float onsetEnvFast = 0.0f, onsetEnvSlow = 0.0f;
    int onsetCooldown = 0;
    float duckEnv = 0.0f;
    int strumCounter = 0;

    std::vector<float> looperBufL, looperBufR;
    int maxLooperSamples = 1764000;
    int looperLength = 0;
    int looperWritePos = 0;
    double looperReadPos = 0.0;
    std::atomic<int> looperState { Looper_Empty };
};

} // namespace ChromaCosm
