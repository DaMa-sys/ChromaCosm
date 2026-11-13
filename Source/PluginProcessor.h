#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ChromaEngine.h"

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

    // Factory Presets for Chroma Console
    struct PresetData
    {
        juce::String name;
        int chromaOrder;
        int calibration; // 0: Lo-Gain, 1: Line, 2: Hot/Inst, 3: Studio
        // Character
        int charType; float charTilt, charAmt, charVol;
        // Movement
        int moveType; float moveRate, moveAmt, moveDrift, moveVol;
        // Diffusion
        int diffType; float diffTime, diffAmt, diffDrift, diffVol;
        // Texture
        int textType; float textTilt, textAmt, textVol;
        // Master
        float chromaMix;
        float globalTilt;
        float outputLevel;
    };

    const std::vector<PresetData>& getPresets() const { return factoryPresets; }
    void loadPreset (int index);

    // Tap Tempo & Clock
    void triggerTapTempo();
    double getEffectiveBpm() const { return currentBpm.load(); }

    ChromaCosm::ChromaEngine& getChromaEngine() { return chromaEngine; }
    const ChromaCosm::ChromaEngine& getChromaEngine() const { return chromaEngine; }

    // Visual meters & oscilloscope
    float getInputLevel() const  { return inputMeter.load(); }
    float getOutputLevel() const { return outputMeter.load(); }
    void getScopeData (std::array<float, 128>& outScope) const;

private:
    void initFactoryPresets();

    ChromaCosm::ChromaEngine chromaEngine;

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
