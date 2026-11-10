#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ChromaEngine.h"
#include "MicrocosmEngine.h"

class ChromaCosmAudioProcessor : public juce::AudioProcessor
{
public:
    ChromaCosmAudioProcessor();
    ~ChromaCosmAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

    // Factory Presets
    struct PresetData
    {
        juce::String name;
        int comboRouting; // 0: Chroma->Micro, 1: Micro->Chroma, 2: Parallel
        // Chroma
        int chromaOrder;
        int charType; float charTilt, charAmt;
        int moveType; float moveRate, moveAmt, moveDrift;
        int diffType; float diffTime, diffAmt, diffDrift;
        int textType; float textTilt, textAmt;
        float chromaMix;
        // Microcosm
        int microFX, microVar, microSubdiv;
        float microTime, microActivity, microRepeats, microShape, microFilter, microRes;
        float microSpace; int microSpaceMode;
        float microPitchRate, microPitchDepth;
        bool microRev;
        float microMix;
    };

    const std::vector<PresetData>& getPresets() const { return factoryPresets; }
    void loadPreset (int index);

    // Tap Tempo & Looper access
    void triggerTapTempo();
    double getEffectiveBpm() const { return currentBpm.load(); }

    ChromaCosm::MicrocosmEngine& getMicrocosmEngine() { return microcosmEngine; }
    const ChromaCosm::MicrocosmEngine& getMicrocosmEngine() const { return microcosmEngine; }

    // Visual meters & oscilloscope
    float getInputLevel() const  { return inputMeter.load(); }
    float getOutputLevel() const { return outputMeter.load(); }
    void getScopeData (std::array<float, 128>& outScope) const;

private:
    void initFactoryPresets();

    ChromaCosm::ChromaEngine chromaEngine;
    ChromaCosm::MicrocosmEngine microcosmEngine;
    juce::AudioBuffer<float> parallelBuffer;

    std::vector<PresetData> factoryPresets;
    int currentPresetIndex = 0;

    std::atomic<double> currentBpm { 120.0 };
    std::atomic<double> manualTapBpm { 0.0 };
    double lastTapTimeSec = 0.0;

    std::atomic<float> inputMeter { 0.0f };
    std::atomic<float> outputMeter { 0.0f };

    std::array<float, 128> scopeRing { 0.0f };
    std::atomic<int> scopeWriteIdx { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChromaCosmAudioProcessor)
};
