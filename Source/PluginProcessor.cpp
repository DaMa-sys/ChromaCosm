#include "PluginProcessor.h"
#include "PluginEditor.h"

ChromaCosmAudioProcessor::ChromaCosmAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    initFactoryPresets();
}

ChromaCosmAudioProcessor::~ChromaCosmAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout ChromaCosmAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    //==========================================================================
    // GLOBAL / COMBO ROUTING
    //==========================================================================
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "comboRouting", "Combo Routing",
        juce::StringArray { "Chroma -> Microcosm", "Microcosm -> Chroma", "Parallel Blend" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "masterOutput", "Master Output",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.80f));

    //==========================================================================
    // CHROMA CONSOLE PARAMETERS
    //==========================================================================
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "chromaOrder", "Chroma Signal Order",
        juce::StringArray {
            "CHAR -> MOVE -> DIFF -> TEXT",
            "TEXT -> CHAR -> MOVE -> DIFF",
            "DIFF -> CHAR -> MOVE -> TEXT",
            "MOVE -> DIFF -> CHAR -> TEXT",
            "CHAR -> DIFF -> MOVE -> TEXT",
            "TEXT -> DIFF -> MOVE -> CHAR"
        }, 0));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "chromaCalib", "Input Calibration",
        juce::StringArray { "LOW", "MED", "HIGH", "SYNTH" }, 1));

    // Module A: CHARACTER (Red)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "charType", "Character Mode",
        juce::StringArray { "OFF", "DRIVE", "SWEETEN", "FUZZ", "HOWL", "SWELL" }, 1));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "charTilt", "Character Tilt/Sens", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.55f));
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

    // Chroma Master Controls
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "chromaMix", "Chroma Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.85f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "chromaLevel", "Chroma Output", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.75f));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "chromaBypass", "Chroma Bypass", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "chromaCapture", "Chroma Capture", false));

    //==========================================================================
    // MICROCOSM PARAMETERS
    //==========================================================================
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "microEffect", "Microcosm Effect",
        juce::StringArray {
            "MOSAIC", "SEQ", "GLIDE",
            "HAZE", "TUNNEL", "STRUM",
            "BLOCKS", "INTERRUPT", "ARP",
            "PATTERN", "WARP"
        }, 0));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "microVar", "Microcosm Variation",
        juce::StringArray { "A", "B", "C", "D" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "microTime", "Microcosm Time", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.50f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "microSubdiv", "Microcosm Subdiv",
        juce::StringArray { "1/4", "1/2", "1x", "2x", "4x", "TRIP" }, 2));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "microSync", "Microcosm Tempo Sync", true));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "microActivity", "Microcosm Activity", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.58f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "microRepeats", "Microcosm Repeats", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.52f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "microShape", "Microcosm Shape", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.50f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "microFilter", "Microcosm Filter", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.62f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "microFilterRes", "Microcosm Filter Resonance", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.25f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "microMix", "Microcosm Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.52f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "microSpace", "Microcosm Space", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.48f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "microSpaceMode", "Space Reverb Mode",
        juce::StringArray { "I - ROOM", "II - HALL", "III - CATHEDRAL", "IV - SHIMMER ABYSS" }, 1));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "microPitchRate", "Pitch Mod Rate", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "microPitchDepth", "Pitch Mod Depth", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.25f));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "microReverse", "Microcosm Reverse", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "microHold", "Hold Sampler", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "microBypass", "Microcosm Bypass", false));

    // Phrase Looper Controls
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "loopLevel", "Looper Level", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.80f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "loopSpeed", "Looper Speed", juce::StringArray { "0.5x", "1.0x", "2.0x" }, 1));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "loopReverse", "Looper Reverse", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "loopPreFX", "Looper Pre-FX", false));

    return { params.begin(), params.end() };
}

void ChromaCosmAudioProcessor::initFactoryPresets()
{
    factoryPresets = {
        { "01. Hologram Dreamscape (Reels + Mosaic)", 0,
          0,  2, 0.58f, 0.42f,   2, 0.36f, 0.35f, 0.32f,   2, 0.46f, 0.42f, 0.38f,   3, 0.56f, 0.30f,  0.85f,
          0, 0, 2,  0.50f, 0.62f, 0.55f, 0.50f, 0.62f, 0.25f,  0.52f, 2,  0.35f, 0.25f, false, 0.52f },

        { "02. Vintage Cassette Haze (Sweeten + Haze B)", 0,
          0,  2, 0.50f, 0.55f,   1, 0.30f, 0.45f, 0.40f,   3, 0.55f, 0.38f, 0.30f,   3, 0.45f, 0.52f,  0.90f,
          3, 1, 3,  0.60f, 0.72f, 0.65f, 0.68f, 0.52f, 0.20f,  0.65f, 2,  0.40f, 0.38f, false, 0.58f },

        { "03. Shoegaze Fuzz Tunnel (Diff->Fuzz + Tunnel)", 0,
          2,  3, 0.62f, 0.68f,   3, 0.25f, 0.40f, 0.25f,   3, 0.68f, 0.58f, 0.40f,   2, 0.55f, 0.45f,  0.90f,
          4, 2, 2,  0.55f, 0.68f, 0.70f, 0.60f, 0.48f, 0.35f,  0.68f, 3,  0.30f, 0.30f, false, 0.50f },

        { "04. Microcosm Into Tape Console (Arp -> Reels)", 1,
          0,  1, 0.54f, 0.48f,   2, 0.42f, 0.38f, 0.45f,   2, 0.48f, 0.45f, 0.45f,   3, 0.52f, 0.42f,  0.88f,
          8, 0, 1,  0.45f, 0.70f, 0.58f, 0.35f, 0.68f, 0.28f,  0.45f, 1,  0.35f, 0.22f, false, 0.60f },

        { "05. Glitch Blocks & Broken Radio", 0,
          1,  1, 0.60f, 0.45f,   5, 0.50f, 0.35f, 0.40f,   4, 0.38f, 0.50f, 0.50f,   4, 0.48f, 0.48f,  0.82f,
          6, 2, 1,  0.40f, 0.75f, 0.48f, 0.22f, 0.60f, 0.40f,  0.35f, 0,  0.50f, 0.35f, false, 0.56f },

        { "06. Swell Shimmer Abyss (Swell + Glide C)", 0,
          0,  5, 0.45f, 0.65f,   1, 0.28f, 0.50f, 0.30f,   3, 0.72f, 0.55f, 0.35f,   1, 0.42f, 0.35f,  0.95f,
          2, 2, 3,  0.65f, 0.64f, 0.68f, 0.78f, 0.56f, 0.22f,  0.78f, 3,  0.28f, 0.32f, true,  0.62f },

        { "07. Strum Harp & Console Doubler", 0,
          0,  2, 0.64f, 0.45f,   1, 0.40f, 0.58f, 0.35f,   1, 0.42f, 0.40f, 0.25f,   2, 0.55f, 0.38f,  0.85f,
          5, 1, 2,  0.48f, 0.68f, 0.56f, 0.32f, 0.70f, 0.25f,  0.54f, 2,  0.32f, 0.20f, false, 0.54f },

        { "08. Reverse Collage into Warp Delay", 0,
          4,  1, 0.52f, 0.38f,   3, 0.33f, 0.45f, 0.38f,   5, 0.52f, 0.60f, 0.42f,   3, 0.50f, 0.32f,  0.85f,
          10, 1, 2, 0.52f, 0.60f, 0.64f, 0.55f, 0.58f, 0.30f,  0.58f, 2,  0.45f, 0.40f, true,  0.55f },

        { "09. Howling Resonant Interrupt", 0,
          0,  4, 0.58f, 0.58f,   4, 0.48f, 0.42f, 0.30f,   1, 0.35f, 0.45f, 0.35f,   5, 0.62f, 0.36f,  0.80f,
          7, 0, 1,  0.42f, 0.72f, 0.50f, 0.30f, 0.64f, 0.42f,  0.40f, 1,  0.40f, 0.30f, false, 0.58f },

        { "10. Parallel Hologram Wall (50/50 Blend)", 2,
          0,  1, 0.55f, 0.52f,   2, 0.35f, 0.48f, 0.42f,   2, 0.50f, 0.52f, 0.45f,   3, 0.48f, 0.44f,  0.90f,
          0, 1, 2,  0.50f, 0.70f, 0.65f, 0.55f, 0.60f, 0.25f,  0.62f, 3,  0.36f, 0.32f, false, 0.75f },

        { "11. Sequenced Analog Synth Board (Seq D)", 1,
          1,  2, 0.56f, 0.50f,   3, 0.28f, 0.52f, 0.32f,   1, 0.38f, 0.44f, 0.28f,   2, 0.52f, 0.48f,  0.88f,
          1, 3, 1,  0.45f, 0.76f, 0.58f, 0.40f, 0.65f, 0.32f,  0.48f, 1,  0.30f, 0.20f, false, 0.60f },

        { "12. Golden Ratio Pattern & Warm Drive", 0,
          0,  1, 0.48f, 0.54f,   1, 0.32f, 0.42f, 0.28f,   3, 0.54f, 0.40f, 0.30f,   1, 0.46f, 0.28f,  0.85f,
          9, 2, 2,  0.52f, 0.65f, 0.62f, 0.50f, 0.62f, 0.22f,  0.56f, 2,  0.28f, 0.24f, false, 0.52f }
    };
}

void ChromaCosmAudioProcessor::loadPreset (int index)
{
    if (index < 0 || index >= static_cast<int> (factoryPresets.size()))
        return;

    currentPresetIndex = index;
    const auto& pr = factoryPresets[index];

    auto setParam = [this] (const juce::String& id, float plainVal)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (plainVal));
    };

    setParam ("comboRouting", static_cast<float> (pr.comboRouting));
    setParam ("chromaOrder",  static_cast<float> (pr.chromaOrder));

    setParam ("charType",   static_cast<float> (pr.charType));
    setParam ("charTilt",   pr.charTilt);
    setParam ("charAmount", pr.charAmt);

    setParam ("moveType",   static_cast<float> (pr.moveType));
    setParam ("moveRate",   pr.moveRate);
    setParam ("moveAmount", pr.moveAmt);
    setParam ("moveDrift",  pr.moveDrift);

    setParam ("diffType",   static_cast<float> (pr.diffType));
    setParam ("diffTime",   pr.diffTime);
    setParam ("diffAmount", pr.diffAmt);
    setParam ("diffDrift",  pr.diffDrift);

    setParam ("textType",   static_cast<float> (pr.textType));
    setParam ("textTilt",   pr.textTilt);
    setParam ("textAmount", pr.textAmt);
    setParam ("chromaMix",  pr.chromaMix);

    setParam ("microEffect",     static_cast<float> (pr.microFX));
    setParam ("microVar",        static_cast<float> (pr.microVar));
    setParam ("microSubdiv",     static_cast<float> (pr.microSubdiv));
    setParam ("microTime",       pr.microTime);
    setParam ("microActivity",   pr.microActivity);
    setParam ("microRepeats",    pr.microRepeats);
    setParam ("microShape",      pr.microShape);
    setParam ("microFilter",     pr.microFilter);
    setParam ("microFilterRes",  pr.microRes);
    setParam ("microSpace",      pr.microSpace);
    setParam ("microSpaceMode",  static_cast<float> (pr.microSpaceMode));
    setParam ("microPitchRate",  pr.microPitchRate);
    setParam ("microPitchDepth", pr.microPitchDepth);
    setParam ("microReverse",    pr.microRev ? 1.0f : 0.0f);
    setParam ("microMix",        pr.microMix);
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
double ChromaCosmAudioProcessor::getTailLengthSeconds() const { return 4.0; }

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
    microcosmEngine.prepare (sampleRate, samplesPerBlock);
    parallelBuffer.setSize (2, juce::jmax (256, samplesPerBlock));
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

    // Measure input peak
    float inPeak = 0.0f;
    for (int ch = 0; ch < totalNumInputChannels; ++ch)
        inPeak = juce::jmax (inPeak, buffer.getMagnitude (ch, 0, numSamples));
    inputMeter.store (0.75f * inputMeter.load() + 0.25f * inPeak);

    // Query DAW playhead BPM (unless overridden by manual tap tempo)
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
    cp.outputLevel   = apvts.getRawParameterValue ("chromaLevel")->load();
    cp.bypass        = apvts.getRawParameterValue ("chromaBypass")->load() > 0.5f;
    cp.captureActive = apvts.getRawParameterValue ("chromaCapture")->load() > 0.5f;
    cp.bpm           = bpm;

    // Populate Microcosm parameters
    ChromaCosm::MicrocosmEngine::Parameters mp;
    mp.effectType    = static_cast<int> (apvts.getRawParameterValue ("microEffect")->load());
    mp.variation     = static_cast<int> (apvts.getRawParameterValue ("microVar")->load());
    mp.time          = apvts.getRawParameterValue ("microTime")->load();
    mp.subdiv        = static_cast<int> (apvts.getRawParameterValue ("microSubdiv")->load());
    mp.tempoSync     = apvts.getRawParameterValue ("microSync")->load() > 0.5f;
    mp.activity      = apvts.getRawParameterValue ("microActivity")->load();
    mp.repeats       = apvts.getRawParameterValue ("microRepeats")->load();
    mp.shape         = apvts.getRawParameterValue ("microShape")->load();
    mp.filter        = apvts.getRawParameterValue ("microFilter")->load();
    mp.filterRes     = apvts.getRawParameterValue ("microFilterRes")->load();
    mp.mix           = apvts.getRawParameterValue ("microMix")->load();
    mp.space         = apvts.getRawParameterValue ("microSpace")->load();
    mp.spaceMode     = static_cast<int> (apvts.getRawParameterValue ("microSpaceMode")->load());
    mp.pitchModRate  = apvts.getRawParameterValue ("microPitchRate")->load();
    mp.pitchModDepth = apvts.getRawParameterValue ("microPitchDepth")->load();
    mp.reverse       = apvts.getRawParameterValue ("microReverse")->load() > 0.5f;
    mp.holdSampler   = apvts.getRawParameterValue ("microHold")->load() > 0.5f;
    mp.bypass        = apvts.getRawParameterValue ("microBypass")->load() > 0.5f;

    mp.loopLevel     = apvts.getRawParameterValue ("loopLevel")->load();
    mp.loopSpeed     = static_cast<int> (apvts.getRawParameterValue ("loopSpeed")->load());
    mp.loopReverse   = apvts.getRawParameterValue ("loopReverse")->load() > 0.5f;
    mp.loopPreFX     = apvts.getRawParameterValue ("loopPreFX")->load() > 0.5f;
    mp.bpm           = bpm;

    const int comboRouting = static_cast<int> (apvts.getRawParameterValue ("comboRouting")->load());
    const float masterOut  = apvts.getRawParameterValue ("masterOutput")->load() / 0.80f;

    if (comboRouting == 0)
    {
        // CHROMA CONSOLE -> MICROCOSM
        chromaEngine.processBlock (buffer, cp);
        microcosmEngine.processBlock (buffer, mp);
    }
    else if (comboRouting == 1)
    {
        // MICROCOSM -> CHROMA CONSOLE
        microcosmEngine.processBlock (buffer, mp);
        chromaEngine.processBlock (buffer, cp);
    }
    else
    {
        // PARALLEL BLEND
        if (parallelBuffer.getNumSamples() < numSamples || parallelBuffer.getNumChannels() < buffer.getNumChannels())
            parallelBuffer.setSize (buffer.getNumChannels(), numSamples, false, false, true);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            parallelBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        chromaEngine.processBlock (buffer, cp);
        microcosmEngine.processBlock (parallelBuffer, mp);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            float* mainPtr = buffer.getWritePointer (ch);
            const float* parPtr = parallelBuffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
                mainPtr[i] = 0.55f * (mainPtr[i] + parPtr[i]);
        }
    }

    buffer.applyGain (masterOut);

    // Final studio safety soft-clipper & NaN/Inf guard on all output channels
    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
    {
        float* outPtr = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            float s = ChromaCosm::sanitizeAudio (outPtr[i], 4.0f);
            // Soft knee ceiling above 0.92 so pushing knobs to 100% never harsh-clips the DAW bus
            if (std::fabs (s) > 0.92f)
                s = std::tanh (s * 0.95f);
            outPtr[i] = s;
        }
    }

    // Output level & scope capture
    float outPeak = 0.0f;
    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        outPeak = juce::jmax (outPeak, buffer.getMagnitude (ch, 0, numSamples));
    outputMeter.store (0.75f * outputMeter.load() + 0.25f * outPeak);

    const float* lPtr = buffer.getReadPointer (0);
    int w = scopeWriteIdx.load();
    for (int i = 0; i < numSamples; i += 32)
    {
        scopeRing[w & 127] = lPtr[i];
        w = (w + 1) & 127;
    }
    scopeWriteIdx.store (w);
}

bool ChromaCosmAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* ChromaCosmAudioProcessor::createEditor()
{
    return new ChromaCosmAudioProcessorEditor (*this);
}

void ChromaCosmAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void ChromaCosmAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChromaCosmAudioProcessor();
}
