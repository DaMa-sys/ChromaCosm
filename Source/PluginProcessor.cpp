#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ChromaCosmAudioProcessor::ChromaCosmAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    initFactoryPresets();
}

ChromaCosmAudioProcessor::~ChromaCosmAudioProcessor()
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout ChromaCosmAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Global Order & Calibration
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "chromaOrder", "Routing Order",
        juce::StringArray {
            "CHAR > MOVE > DIFF > TEXT",
            "TEXT > CHAR > MOVE > DIFF",
            "DIFF > CHAR > MOVE > TEXT",
            "MOVE > DIFF > CHAR > TEXT",
            "CHAR > DIFF > MOVE > TEXT",
            "CHAR > MOVE > TEXT > DIFF",
            "MOVE > CHAR > DIFF > TEXT",
            "TEXT > DIFF > MOVE > CHAR"
        }, 0));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "chromaCalib", "Input Calibration",
        juce::StringArray { "LO-GAIN", "LINE", "HOT / INST", "HIGH HEADROOM" }, 1));

    // Module A: CHARACTER (Red)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "charType", "Character Mode",
        juce::StringArray { "OFF", "DRIVE", "SWEETEN", "FUZZ", "HOWL", "SWELL" }, 1));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "charTilt", "Character Tilt", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.50f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "charAmount", "Character Amount", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.40f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "charVol", "Character Effect Vol", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.75f));

    // Module B: MOVEMENT (Yellow)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "moveType", "Movement Mode",
        juce::StringArray { "OFF", "DOUBLER", "VIBRATO", "PHASER", "TREMOLO", "PITCH" }, 2));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "moveRate", "Movement Rate", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.38f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "moveAmount", "Movement Amount", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "moveDrift", "Movement Drift", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.30f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "moveVol", "Movement Effect Vol", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.75f));

    // Module C: DIFFUSION (Green)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "diffType", "Diffusion Mode",
        juce::StringArray { "OFF", "CASCADE", "REELS", "SPACE", "COLLAGE", "REVERSE" }, 2));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "diffTime", "Diffusion Time", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.45f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "diffAmount", "Diffusion Amount", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.42f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "diffDrift", "Diffusion Drift", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "diffVol", "Diffusion Effect Vol", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.75f));

    // Module D: TEXTURE (Blue)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "textType", "Texture Mode",
        juce::StringArray { "OFF", "FILTER", "SQUASH", "CASSETTE", "BROKEN", "INTERFERENCE" }, 3));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "textTilt", "Texture Tilt/Freq", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.55f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "textAmount", "Texture Amount", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.32f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "textVol", "Texture Effect Vol", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.75f));

    // Master Controls
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "chromaMix", "Master Dry/Wet", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.85f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "globalTilt", "Master Tilt EQ", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.50f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "chromaLevel", "Master Output", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.75f));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "chromaBypass", "Master Bypass", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "chromaCapture", "Capture Looper", false));

    return { params.begin(), params.end() };
}

//==============================================================================
void ChromaCosmAudioProcessor::initFactoryPresets()
{
    factoryPresets.clear();

    // 1. Warm Tape & Reels
    factoryPresets.push_back ({
        "1. Warm Tape & Reels", 0, 1,
        ChromaCosm::ChromaEngine::Char_Drive,    0.48f, 0.35f, 0.75f,
        ChromaCosm::ChromaEngine::Move_Vibrato,  0.30f, 0.28f, 0.40f, 0.75f,
        ChromaCosm::ChromaEngine::Diff_Reels,    0.42f, 0.45f, 0.40f, 0.75f,
        ChromaCosm::ChromaEngine::Text_Cassette, 0.45f, 0.38f, 0.75f,
        0.90f, 0.48f, 0.75f
    });

    // 2. Lo-Fi VHS Drift
    factoryPresets.push_back ({
        "2. Lo-Fi VHS Drift", 1, 1,
        ChromaCosm::ChromaEngine::Char_Sweeten,  0.50f, 0.38f, 0.75f,
        ChromaCosm::ChromaEngine::Move_Doubler,  0.25f, 0.45f, 0.55f, 0.75f,
        ChromaCosm::ChromaEngine::Diff_Space,    0.55f, 0.50f, 0.50f, 0.75f,
        ChromaCosm::ChromaEngine::Text_Broken,   0.55f, 0.42f, 0.75f,
        0.85f, 0.42f, 0.75f
    });

    // 3. Cosmic Space Plate
    factoryPresets.push_back ({
        "3. Cosmic Space Plate", 0, 1,
        ChromaCosm::ChromaEngine::Char_Howl,     0.60f, 0.30f, 0.75f,
        ChromaCosm::ChromaEngine::Move_Vibrato,  0.20f, 0.30f, 0.45f, 0.75f,
        ChromaCosm::ChromaEngine::Diff_Space,    0.85f, 0.65f, 0.35f, 0.75f,
        ChromaCosm::ChromaEngine::Text_Filter,   0.68f, 0.22f, 0.75f,
        0.80f, 0.55f, 0.75f
    });

    // 4. Crisp Drive & Tremolo
    factoryPresets.push_back ({
        "4. Crisp Drive & Tremolo", 0, 2,
        ChromaCosm::ChromaEngine::Char_Drive,    0.55f, 0.55f, 0.75f,
        ChromaCosm::ChromaEngine::Move_Tremolo,  0.55f, 0.55f, 0.20f, 0.75f,
        ChromaCosm::ChromaEngine::Diff_Cascade,  0.35f, 0.35f, 0.25f, 0.75f,
        ChromaCosm::ChromaEngine::Text_Squash,   0.50f, 0.35f, 0.75f,
        1.00f, 0.52f, 0.75f
    });

    // 5. Deep Ambient Echo
    factoryPresets.push_back ({
        "5. Deep Ambient Echo", 2, 1,
        ChromaCosm::ChromaEngine::Char_Sweeten,  0.50f, 0.35f, 0.75f,
        ChromaCosm::ChromaEngine::Move_Pitch,    0.25f, 0.35f, 0.30f, 0.75f,
        ChromaCosm::ChromaEngine::Diff_Cascade,  0.70f, 0.60f, 0.45f, 0.75f,
        ChromaCosm::ChromaEngine::Text_Cassette, 0.40f, 0.30f, 0.75f,
        0.80f, 0.48f, 0.75f
    });

    // 6. Broken Radio 1970
    factoryPresets.push_back ({
        "6. Broken Radio 1970", 0, 1,
        ChromaCosm::ChromaEngine::Char_Fuzz,         0.40f, 0.45f, 0.75f,
        ChromaCosm::ChromaEngine::Move_Phaser,       0.30f, 0.40f, 0.30f, 0.75f,
        ChromaCosm::ChromaEngine::Diff_Reels,        0.25f, 0.30f, 0.40f, 0.75f,
        ChromaCosm::ChromaEngine::Text_Interference, 0.65f, 0.45f, 0.75f,
        0.85f, 0.35f, 0.75f
    });

    // 7. Tape Flanger & Cascade
    factoryPresets.push_back ({
        "7. Tape Flanger & Cascade", 6, 1,
        ChromaCosm::ChromaEngine::Char_Drive,    0.50f, 0.40f, 0.75f,
        ChromaCosm::ChromaEngine::Move_Doubler,  0.45f, 0.60f, 0.35f, 0.75f,
        ChromaCosm::ChromaEngine::Diff_Cascade,  0.50f, 0.45f, 0.30f, 0.75f,
        ChromaCosm::ChromaEngine::Text_Squash,   0.50f, 0.40f, 0.75f,
        0.90f, 0.50f, 0.75f
    });

    // 8. Dreamy Chorus Echo
    factoryPresets.push_back ({
        "8. Dreamy Chorus Echo", 0, 1,
        ChromaCosm::ChromaEngine::Char_Sweeten,  0.52f, 0.30f, 0.75f,
        ChromaCosm::ChromaEngine::Move_Vibrato,  0.40f, 0.45f, 0.40f, 0.75f,
        ChromaCosm::ChromaEngine::Diff_Reels,    0.55f, 0.50f, 0.45f, 0.75f,
        ChromaCosm::ChromaEngine::Text_Filter,   0.60f, 0.20f, 0.75f,
        0.85f, 0.52f, 0.75f
    });

    // 9. Vintage Rotary Organ
    factoryPresets.push_back ({
        "9. Vintage Rotary Organ", 3, 1,
        ChromaCosm::ChromaEngine::Char_Swell,    0.50f, 0.30f, 0.75f,
        ChromaCosm::ChromaEngine::Move_Tremolo,  0.70f, 0.65f, 0.30f, 0.75f,
        ChromaCosm::ChromaEngine::Diff_Space,    0.40f, 0.40f, 0.30f, 0.75f,
        ChromaCosm::ChromaEngine::Text_Cassette, 0.48f, 0.35f, 0.75f,
        0.85f, 0.48f, 0.75f
    });

    // 10. Fuzz Swell Shimmer
    factoryPresets.push_back ({
        "10. Fuzz Swell Shimmer", 0, 1,
        ChromaCosm::ChromaEngine::Char_Fuzz,     0.55f, 0.70f, 0.75f,
        ChromaCosm::ChromaEngine::Move_Pitch,    0.35f, 0.40f, 0.45f, 0.75f,
        ChromaCosm::ChromaEngine::Diff_Space,    0.75f, 0.60f, 0.40f, 0.75f,
        ChromaCosm::ChromaEngine::Text_Cassette, 0.45f, 0.35f, 0.75f,
        0.80f, 0.45f, 0.75f
    });

    // 11. Studio Master Preamp
    factoryPresets.push_back ({
        "11. Studio Master Preamp", 1, 3,
        ChromaCosm::ChromaEngine::Char_Sweeten,  0.52f, 0.45f, 0.75f,
        ChromaCosm::ChromaEngine::Move_Off,      0.20f, 0.00f, 0.10f, 0.75f,
        ChromaCosm::ChromaEngine::Diff_Off,      0.30f, 0.00f, 0.10f, 0.75f,
        ChromaCosm::ChromaEngine::Text_Squash,   0.50f, 0.30f, 0.75f,
        1.00f, 0.50f, 0.80f
    });

    // 12. Subtle Air Glaze
    factoryPresets.push_back ({
        "12. Subtle Air Glaze", 0, 1,
        ChromaCosm::ChromaEngine::Char_Sweeten,  0.55f, 0.25f, 0.75f,
        ChromaCosm::ChromaEngine::Move_Doubler,  0.20f, 0.25f, 0.30f, 0.75f,
        ChromaCosm::ChromaEngine::Diff_Reels,    0.35f, 0.25f, 0.30f, 0.75f,
        ChromaCosm::ChromaEngine::Text_Cassette, 0.58f, 0.20f, 0.75f,
        0.65f, 0.56f, 0.75f
    });
}

void ChromaCosmAudioProcessor::loadPreset (int index)
{
    if (index < 0 || index >= static_cast<int> (factoryPresets.size()))
        return;

    currentPresetIndex = index;
    const auto& pr = factoryPresets[index];

    auto setParam = [this] (const juce::String& id, float val)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (val));
    };

    setParam ("chromaOrder", static_cast<float> (pr.chromaOrder));
    setParam ("chromaCalib", static_cast<float> (pr.calibration));

    setParam ("charType",   static_cast<float> (pr.charType));
    setParam ("charTilt",   pr.charTilt);
    setParam ("charAmount", pr.charAmt);
    setParam ("charVol",    pr.charVol);

    setParam ("moveType",   static_cast<float> (pr.moveType));
    setParam ("moveRate",   pr.moveRate);
    setParam ("moveAmount", pr.moveAmt);
    setParam ("moveDrift",  pr.moveDrift);
    setParam ("moveVol",    pr.moveVol);

    setParam ("diffType",   static_cast<float> (pr.diffType));
    setParam ("diffTime",   pr.diffTime);
    setParam ("diffAmount", pr.diffAmt);
    setParam ("diffDrift",  pr.diffDrift);
    setParam ("diffVol",    pr.diffVol);

    setParam ("textType",   static_cast<float> (pr.textType));
    setParam ("textTilt",   pr.textTilt);
    setParam ("textAmount", pr.textAmt);
    setParam ("textVol",    pr.textVol);

    setParam ("chromaMix",   pr.chromaMix);
    setParam ("globalTilt",  pr.globalTilt);
    setParam ("chromaLevel", pr.outputLevel);
}

void ChromaCosmAudioProcessor::triggerTapTempo()
{
    double nowSec = juce::Time::getMillisecondCounterHiRes() * 0.001;
    double delta = nowSec - lastTapTimeSec;
    lastTapTimeSec = nowSec;

    if (delta > 0.18 && delta < 2.2)
    {
        double tappedBpm = juce::jlimit (40.0, 240.0, 60.0 / delta);
        manualTapBpm.store (tappedBpm);
        currentBpm.store (tappedBpm);
    }
}

void ChromaCosmAudioProcessor::getScopeData (std::array<float, 128>& outScope) const
{
    int w = scopeWriteIdx.load();
    for (int i = 0; i < 128; ++i)
        outScope[i] = scopeRing[(w + i) & 127];
}

const juce::String ChromaCosmAudioProcessor::getName() const { return JucePlugin_Name; }
bool ChromaCosmAudioProcessor::acceptsMidi() const  { return true; }
bool ChromaCosmAudioProcessor::producesMidi() const { return false; }
bool ChromaCosmAudioProcessor::isMidiEffect() const { return false; }
double ChromaCosmAudioProcessor::getTailLengthSeconds() const { return 3.0; }

int ChromaCosmAudioProcessor::getNumPrograms() { return static_cast<int> (factoryPresets.size()); }
int ChromaCosmAudioProcessor::getCurrentProgram() { return currentPresetIndex; }
void ChromaCosmAudioProcessor::setCurrentProgram (int index) { loadPreset (index); }
const juce::String ChromaCosmAudioProcessor::getProgramName (int index)
{
    if (index >= 0 && index < static_cast<int> (factoryPresets.size()))
        return factoryPresets[index].name;
    return {};
}
void ChromaCosmAudioProcessor::changeProgramName (int, const juce::String&) {}

void ChromaCosmAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    chromaEngine.prepare (sampleRate, samplesPerBlock);
}

void ChromaCosmAudioProcessor::releaseResources() {}

bool ChromaCosmAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void ChromaCosmAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    // Input meter
    float inPeak = 0.0f;
    for (int ch = 0; ch < totalNumInputChannels; ++ch)
        inPeak = juce::jmax (inPeak, buffer.getMagnitude (ch, 0, numSamples));
    inputMeter.store (0.75f * inputMeter.load() + 0.25f * inPeak);

    // DAW Tempo or manual Tap
    double bpm = 120.0;
    if (manualTapBpm.load() > 30.0)
    {
        bpm = manualTapBpm.load();
    }
    else if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
            if (auto hostBpm = pos->getBpm())
                if (*hostBpm > 20.0)
                    bpm = *hostBpm;
    }
    currentBpm.store (bpm);

    // Populate Chroma Console parameters
    ChromaCosm::ChromaEngine::Parameters cp;
    cp.routingPreset = static_cast<int> (apvts.getRawParameterValue ("chromaOrder")->load());
    cp.calibration   = static_cast<int> (apvts.getRawParameterValue ("chromaCalib")->load());

    cp.charType      = static_cast<int> (apvts.getRawParameterValue ("charType")->load());
    cp.charTilt      = apvts.getRawParameterValue ("charTilt")->load();
    cp.charAmount    = apvts.getRawParameterValue ("charAmount")->load();
    cp.charVol       = apvts.getRawParameterValue ("charVol")->load();

    cp.moveType      = static_cast<int> (apvts.getRawParameterValue ("moveType")->load());
    cp.moveRate      = apvts.getRawParameterValue ("moveRate")->load();
    cp.moveAmount    = apvts.getRawParameterValue ("moveAmount")->load();
    cp.moveDrift     = apvts.getRawParameterValue ("moveDrift")->load();
    cp.moveVol       = apvts.getRawParameterValue ("moveVol")->load();

    cp.diffType      = static_cast<int> (apvts.getRawParameterValue ("diffType")->load());
    cp.diffTime      = apvts.getRawParameterValue ("diffTime")->load();
    cp.diffAmount    = apvts.getRawParameterValue ("diffAmount")->load();
    cp.diffDrift     = apvts.getRawParameterValue ("diffDrift")->load();
    cp.diffVol       = apvts.getRawParameterValue ("diffVol")->load();

    cp.textType      = static_cast<int> (apvts.getRawParameterValue ("textType")->load());
    cp.textTilt      = apvts.getRawParameterValue ("textTilt")->load();
    cp.textAmount    = apvts.getRawParameterValue ("textAmount")->load();
    cp.textVol       = apvts.getRawParameterValue ("textVol")->load();

    cp.masterMix     = apvts.getRawParameterValue ("chromaMix")->load();
    cp.globalTilt    = apvts.getRawParameterValue ("globalTilt")->load();
    cp.outputLevel   = apvts.getRawParameterValue ("chromaLevel")->load();
    cp.bypass        = apvts.getRawParameterValue ("chromaBypass")->load() > 0.5f;
    cp.captureActive = apvts.getRawParameterValue ("chromaCapture")->load() > 0.5f;
    cp.bpm           = bpm;

    // Process Chroma Engine
    chromaEngine.processBlock (buffer, cp);

    // Output meter & oscilloscope update
    float outPeak = 0.0f;
    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        outPeak = juce::jmax (outPeak, buffer.getMagnitude (ch, 0, numSamples));
    outputMeter.store (0.75f * outputMeter.load() + 0.25f * outPeak);

    const float* readL = buffer.getReadPointer (0);
    const float* readR = (totalNumOutputChannels > 1) ? buffer.getReadPointer (1) : readL;
    int w = scopeWriteIdx.load();
    const int step = juce::jmax (1, numSamples / 16);
    for (int s = 0; s < numSamples; s += step)
    {
        float monoVal = 0.5f * (readL[s] + readR[s]);
        scopeRing[w & 127] = monoVal;
        w = (w + 1) & 127;
    }
    scopeWriteIdx.store (w);
}

//==============================================================================
bool ChromaCosmAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* ChromaCosmAudioProcessor::createEditor()
{
    return new ChromaCosmAudioProcessorEditor (*this);
}

//==============================================================================
void ChromaCosmAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void ChromaCosmAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChromaCosmAudioProcessor();
}
