#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ChromaCosmAudioProcessorEditor::ChromaCosmAudioProcessorEditor (ChromaCosmAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);
    setSize (960, 580);

    tooltipWindow = std::make_unique<juce::TooltipWindow> (this, 600);

    //==========================================================================
    // Top Bar: Presets & Routing
    //==========================================================================
    const auto& presets = audioProcessor.getPresets();
    for (int i = 0; i < static_cast<int> (presets.size()); ++i)
        presetBox.addItem (presets[i].name, i + 1);

    presetBox.setSelectedId (audioProcessor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        int idx = presetBox.getSelectedId() - 1;
        audioProcessor.setCurrentProgram (idx);
    };
    addAndMakeVisible (presetBox);
    registerControlDescription (presetBox, "PRESET FACTORY", "Seleziona uno dei 12 preset accuratamente rifiniti per chitarra, synth o mastering console.");

    prevPresetBtn.onClick = [this]
    {
        int cur = presetBox.getSelectedId() - 1;
        if (cur > 0)
            presetBox.setSelectedId (cur, juce::sendNotificationSync);
    };
    addAndMakeVisible (prevPresetBtn);

    nextPresetBtn.onClick = [this]
    {
        int cur = presetBox.getSelectedId() - 1;
        if (cur < static_cast<int> (audioProcessor.getPresets().size()) - 1)
            presetBox.setSelectedId (cur + 2, juce::sendNotificationSync);
    };
    addAndMakeVisible (nextPresetBtn);

    // Routing Order
    chromaOrderBox.addItemList ({
        "1. CHAR > MOVE > DIFF > TEXT (Standard Studio)",
        "2. TEXT > CHAR > MOVE > DIFF (Pre-Filter / Squash)",
        "3. DIFF > CHAR > MOVE > TEXT (Reverb into Drive)",
        "4. MOVE > DIFF > CHAR > TEXT (Vintage Vibe Organ)",
        "5. CHAR > DIFF > MOVE > TEXT (Post-Echo Modulation)",
        "6. CHAR > MOVE > TEXT > DIFF (Tape before Space)",
        "7. MOVE > CHAR > DIFF > TEXT (Phaser into Drive)",
        "8. TEXT > DIFF > MOVE > CHAR (Reverse Chain)"
    }, 1);
    addAndMakeVisible (chromaOrderBox);
    comboAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, "chromaOrder", chromaOrderBox));
    registerControlDescription (chromaOrderBox, "ROUTING ORDER DEI 4 MODULI",
        "Modifica in tempo reale l'ordine della catena audio tra Character, Movement, Diffusion e Texture.");

    // Input Calibration
    chromaCalibBox.addItemList ({ "LO-GAIN", "LINE", "HOT / INST", "HIGH HEADROOM" }, 1);
    addAndMakeVisible (chromaCalibBox);
    comboAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, "chromaCalib", chromaCalibBox));
    registerControlDescription (chromaCalibBox, "INPUT CALIBRATION",
        "Adatta l'headroom analogico d'ingresso: per strumenti passivi, linea o mix di mastering ad alto volume.");

    tapTempoBtn.onClick = [this] { audioProcessor.triggerTapTempo(); };
    addAndMakeVisible (tapTempoBtn);
    registerControlDescription (tapTempoBtn, "TAP TEMPO", "Clicca ripetutamente per impostare il tempo a mano oppure sincronizzati automaticamente al clock della DAW.");

    infoTooltipsBtn.setToggleState (true, juce::dontSendNotification);
    infoTooltipsBtn.onClick = [this] { applyTooltipState(); };
    addAndMakeVisible (infoTooltipsBtn);
    registerControlDescription (infoTooltipsBtn, "INFO MOUSE", "Abilita o disabilita il banner interattivo con le spiegazioni dettagliate al passaggio del cursore.");

    openGuideBtn.onClick = [this]
    {
        guideOverlay.setVisible (true);
        guideOverlay.toFront (true);
    };
    addAndMakeVisible (openGuideBtn);
    registerControlDescription (openGuideBtn, "MANUALE INTEGRATO", "Apre la guida interattiva completa con spiegazione dei 20 algoritmi e consigli da studio.");

    //==========================================================================
    // Module A: CHARACTER (Red)
    //==========================================================================
    const juce::Colour redColour (0xFFE24A3B);
    setupKnob (charTiltKnob, "charTilt", redColour, "Regola il bilanciamento timbrico Tilt del Character: scuro e corposo a sinistra, brillante e tagliente a destra.");
    setupKnob (charAmtKnob,  "charAmount", redColour, "Intensita di saturazione, gain del Fuzz, preamp o profondita dello Swell d'inviluppo.");
    setupKnob (charVolKnob,  "charVol", redColour, "Livello d'uscita dedicato per il modulo Character.");

    charCycleBtn.onClick = [this]
    {
        if (auto* p = audioProcessor.apvts.getParameter ("charType"))
        {
            float cur = p->getValue();
            float next = (cur >= 5.0f / 5.0f) ? 0.0f : cur + (1.0f / 5.0f);
            p->setValueNotifyingHost (next);
        }
    };
    addAndMakeVisible (charCycleBtn);
    registerControlDescription (charCycleBtn, "SELETTORE CHARACTER", "Clicca per ciclare tra gli algoritmi Character: Off, Drive, Sweeten, Fuzz, Howl, Swell.");

    //==========================================================================
    // Module B: MOVEMENT (Yellow)
    //==========================================================================
    const juce::Colour yellowColour (0xFFF59E0B);
    setupKnob (moveRateKnob,  "moveRate", yellowColour, "Velocita di modulazione LFO (da frazioni ritmiche lente fino a velocità audio vibrato/tremolo).");
    setupKnob (moveAmtKnob,   "moveAmount", yellowColour, "Profondita dell'effetto di modulazione, escursione del chorus o intensita del phaser.");
    setupKnob (moveDriftKnob, "moveDrift", yellowColour, "Inietta instabilita organica Pink/Perlin noise: wow & flutter vintage da registratore a nastro.");
    setupKnob (moveVolKnob,   "moveVol", yellowColour, "Livello d'uscita dedicato per il modulo Movement.");

    moveCycleBtn.onClick = [this]
    {
        if (auto* p = audioProcessor.apvts.getParameter ("moveType"))
        {
            float cur = p->getValue();
            float next = (cur >= 5.0f / 5.0f) ? 0.0f : cur + (1.0f / 5.0f);
            p->setValueNotifyingHost (next);
        }
    };
    addAndMakeVisible (moveCycleBtn);
    registerControlDescription (moveCycleBtn, "SELETTORE MOVEMENT", "Clicca per ciclare tra gli algoritmi Movement: Off, Doubler, Vibrato, Phaser, Tremolo, Pitch.");

    //==========================================================================
    // Module C: DIFFUSION (Green)
    //==========================================================================
    const juce::Colour greenColour (0xFF10B981);
    setupKnob (diffTimeKnob,  "diffTime", greenColour, "Tempo di ritardo per i delay o dimensione spaziale e decadimento per il riverbero Space.");
    setupKnob (diffAmtKnob,   "diffAmount", greenColour, "Feedback delle ripetizioni o ampiezza e diffusione della coda ambientale.");
    setupKnob (diffDriftKnob, "diffDrift", greenColour, "Modulazione casuale del tempo di ritardo: crea code riverberanti cangianti e tape flutter.");
    setupKnob (diffVolKnob,   "diffVol", greenColour, "Livello d'uscita dedicato per il modulo Diffusion.");

    diffCycleBtn.onClick = [this]
    {
        if (auto* p = audioProcessor.apvts.getParameter ("diffType"))
        {
            float cur = p->getValue();
            float next = (cur >= 5.0f / 5.0f) ? 0.0f : cur + (1.0f / 5.0f);
            p->setValueNotifyingHost (next);
        }
    };
    addAndMakeVisible (diffCycleBtn);
    registerControlDescription (diffCycleBtn, "SELETTORE DIFFUSION", "Clicca per ciclare tra gli algoritmi Diffusion: Off, Cascade, Reels, Space, Collage, Reverse.");

    //==========================================================================
    // Module D: TEXTURE (Blue)
    //==========================================================================
    const juce::Colour blueColour (0xFF3B82F6);
    setupKnob (textTiltKnob, "textTilt", blueColour, "Frequenza di taglio del filtro risonante, o tono timbrico dell'interferenza / cassetta.");
    setupKnob (textAmtKnob,  "textAmount", blueColour, "Quantita di texture: compressione ottica, saturazione cassetta o disturbo analogico.");
    setupKnob (textVolKnob,  "textVol", blueColour, "Livello d'uscita dedicato per il modulo Texture.");

    textCycleBtn.onClick = [this]
    {
        if (auto* p = audioProcessor.apvts.getParameter ("textType"))
        {
            float cur = p->getValue();
            float next = (cur >= 5.0f / 5.0f) ? 0.0f : cur + (1.0f / 5.0f);
            p->setValueNotifyingHost (next);
        }
    };
    addAndMakeVisible (textCycleBtn);
    registerControlDescription (textCycleBtn, "SELETTORE TEXTURE", "Clicca per ciclare tra gli algoritmi Texture: Off, Filter, Squash, Cassette, Broken, Interference.");

    //==========================================================================
    // Master Controls & Footswitches
    //==========================================================================
    const juce::Colour whiteColour (0xFFF3F4F6);
    setupKnob (chromaMixKnob,   "chromaMix",   whiteColour, "Bilanciamento Dry / Wet globale tra il segnale originale e il processore Chroma Console.");
    setupKnob (globalTiltKnob,  "globalTilt",  whiteColour, "Master Console Tilt EQ: colorazione globale dolce (caldo e scuro a sinistra, aperto e arioso a destra).");
    setupKnob (chromaLevelKnob, "chromaLevel", whiteColour, "Volume d'uscita master generale della console.");

    addAndMakeVisible (chromaCaptureBtn);
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, "chromaCapture", chromaCaptureBtn));
    registerControlDescription (chromaCaptureBtn, "FOOTSWITCH CAPTURE",
        "Congela istantaneamente l'ultimo frammento audio in un drone/micro-loop infinito e morbido su cui continuare a suonare.");

    addAndMakeVisible (chromaBypassBtn);
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, "chromaBypass", chromaBypassBtn));
    registerControlDescription (chromaBypassBtn, "FOOTSWITCH BYPASS",
        "Disattiva tutti i moduli con commutazione morbida analogica a zero click.");

    // Add Guide Overlay (initially hidden)
    addChildComponent (guideOverlay);

    startTimerHz (30);
}

ChromaCosmAudioProcessorEditor::~ChromaCosmAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ChromaCosmAudioProcessorEditor::setupKnob (juce::Slider& s, const juce::String& paramID, juce::Colour accentColour, const juce::String& desc)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
    s.setColour (juce::Slider::rotarySliderFillColourId, accentColour);
    s.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xFFE5E7EB));
    s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x22000000));
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (s);

    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.apvts, paramID, s));

    registerControlDescription (s, paramID.toUpperCase(), desc);
}

void ChromaCosmAudioProcessorEditor::registerControlDescription (juce::Component& comp, const juce::String& title, const juce::String& desc)
{
    compDescriptions.push_back ({ &comp, title, desc });
}

void ChromaCosmAudioProcessorEditor::applyTooltipState()
{
    tooltipsEnabled = infoTooltipsBtn.getToggleState();
    if (! tooltipsEnabled)
    {
        currentHoverTitle = "INFO MOUSE DISATTIVATA";
        currentHoverText  = "Clicca su 'INFO MOUSE' per riattivare i suggerimenti rapidi al passaggio del cursore.";
        repaint();
    }
}

void ChromaCosmAudioProcessorEditor::timerCallback()
{
    // Update preset dropdown selection if changed externally
    int pIdx = audioProcessor.getCurrentProgram();
    if (presetBox.getSelectedId() - 1 != pIdx)
        presetBox.setSelectedId (pIdx + 1, juce::dontSendNotification);

    repaint();
}

void ChromaCosmAudioProcessorEditor::mouseMove (const juce::MouseEvent& event)
{
    updateHoverBannerFromPoint (event.getPosition());
}

void ChromaCosmAudioProcessorEditor::updateHoverBannerFromPoint (juce::Point<int> pt)
{
    if (! tooltipsEnabled)
        return;

    // Check interactive boxes
    for (const auto& box : interactiveBoxes)
    {
        if (box.bounds.contains (pt))
        {
            currentHoverTitle = box.hoverTitle;
            currentHoverText  = box.hoverDesc;
            repaint();
            return;
        }
    }

    // Check components
    for (const auto& info : compDescriptions)
    {
        if (info.comp != nullptr && info.comp->isVisible())
        {
            auto boundsInEditor = getLocalArea (info.comp, info.comp->getLocalBounds());
            if (boundsInEditor.contains (pt))
            {
                currentHoverTitle = info.title;
                currentHoverText  = info.desc;
                repaint();
                return;
            }
        }
    }
}

void ChromaCosmAudioProcessorEditor::mouseDown (const juce::MouseEvent& event)
{
    // Check if clicked directly on an algorithm label box
    for (const auto& box : interactiveBoxes)
    {
        if (box.bounds.contains (event.getPosition()))
        {
            if (auto* p = audioProcessor.apvts.getParameter (box.paramID))
            {
                p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (box.choiceIndex)));
                repaint();
                return;
            }
        }
    }
}

//==============================================================================
void ChromaCosmAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Brushed dark chassis
    g.fillAll (juce::Colour (0xFF14151B));

    juce::ColourGradient bgGrad (juce::Colour (0xFF1F2029), 0.0f, 0.0f,
                                 juce::Colour (0xFF101116), 0.0f, static_cast<float> (getHeight()), false);
    g.setGradientFill (bgGrad);
    g.fillRect (getLocalBounds());

    // Header Title
    g.setColour (juce::Colour (0xFFF3F4F6));
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText ("CHROMA CONSOLE", 16, 12, 220, 22, juce::Justification::left);

    g.setColour (juce::Colour (0xFF9CA3AF));
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("4-MODULE ANALOG COLOR & TEXTURE ENGINE", 16, 32, 260, 16, juce::Justification::left);

    // Tempo & DAW BPM display badge
    auto bpmRect = juce::Rectangle<int> (380, 14, 88, 32);
    g.setColour (juce::Colour (0xFF242632));
    g.fillRoundedRectangle (bpmRect.toFloat(), 6.0f);
    g.setColour (juce::Colour (0xFF3E4153));
    g.drawRoundedRectangle (bpmRect.toFloat(), 6.0f, 1.0f);

    g.setColour (juce::Colour (0xFFF59E0B));
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (juce::String (audioProcessor.getEffectiveBpm(), 1) + " BPM", bpmRect, juce::Justification::centred);

    // Visual Oscilloscope Display (Middle top)
    auto scopeRect = juce::Rectangle<int> (476, 14, 250, 32);
    g.setColour (juce::Colour (0xFF181A22));
    g.fillRoundedRectangle (scopeRect.toFloat(), 6.0f);
    g.setColour (juce::Colour (0xFF2D303E));
    g.drawRoundedRectangle (scopeRect.toFloat(), 6.0f, 1.0f);

    std::array<float, 128> scopeData;
    audioProcessor.getScopeData (scopeData);
    juce::Path wavePath;
    float sx = static_cast<float> (scopeRect.getX());
    float sy = static_cast<float> (scopeRect.getCentreY());
    float sw = static_cast<float> (scopeRect.getWidth());
    float sh = static_cast<float> (scopeRect.getHeight()) * 0.45f;

    for (int i = 0; i < 128; ++i)
    {
        float px = sx + (static_cast<float> (i) / 127.0f) * sw;
        float py = sy - juce::jlimit (-1.0f, 1.0f, scopeData[i]) * sh;
        if (i == 0) wavePath.startNewSubPath (px, py);
        else        wavePath.lineTo (px, py);
    }
    g.setColour (juce::Colour (0xCC10B981));
    g.strokePath (wavePath, juce::PathStrokeType (1.5f));

    // Rebuild interactive hit boxes for the 4 module strips
    interactiveBoxes.clear();

    const int curOrder = static_cast<int> (audioProcessor.apvts.getRawParameterValue ("chromaOrder")->load());
    const auto orderSlots = ChromaCosm::ChromaEngine::getRoutingOrder (curOrder);

    // Strip Definitions
    struct StripConfig
    {
        int slotIndex;
        int moduleKind; // 0: Char, 1: Move, 2: Diff, 3: Text
        juce::String letter;
        juce::String name;
        juce::Colour accent;
        juce::String paramID;
        std::vector<juce::String> algoNames;
        std::vector<juce::String> algoDescs;
    };

    std::vector<StripConfig> strips = {
        {
            0, 0, "A", "CHARACTER", juce::Colour (0xFFE24A3B), "charType",
            { "OFF", "DRIVE", "SWEETEN", "FUZZ", "HOWL", "SWELL" },
            {
                "Modulo Character disattivato.",
                "Overdrive analogico caldo e dinamico, risponde alla forza del plettrato.",
                "Preamp valvolare da studio con compressione armonica morbida e rotonda.",
                "Silicio ad alto guadagno vintage: sustain infinito e armoniche ricche.",
                "Preamplificatore aggressivo con filtro risonante e colore scuro.",
                "Inviluppo slow-gear automatico che elimina l'attacco per creare archi e pad."
            }
        },
        {
            1, 1, "B", "MOVEMENT", juce::Colour (0xFFF59E0B), "moveType",
            { "OFF", "DOUBLER", "VIBRATO", "PHASER", "TREMOLO", "PITCH" },
            {
                "Modulo Movement disattivato.",
                "Chorus e flanger stereo a nastro con micro-ritardo sfasato.",
                "Vibrato d'intonazione organico con wow & flutter da nastro analogico.",
                "Phaser caldo a 6 stadi all-pass con feedback profondo e rotazione stereo.",
                "Tremolo ritmico con modulazione d'ampiezza morbida e pulsante.",
                "Doppiatore con trasposizione d'ottava e modulazione fluttuante shimmer."
            }
        },
        {
            2, 2, "C", "DIFFUSION", juce::Colour (0xFF10B981), "diffType",
            { "OFF", "CASCADE", "REELS", "SPACE", "COLLAGE", "REVERSE" },
            {
                "Modulo Diffusion disattivato.",
                "Delay ritmico a stadi con diffusione cross-coupled e feedback esteso.",
                "Tape echo Reel-to-Reel a bobine aperte con saturazione organica progressiva.",
                "Riverbero plate/cavern tridimensionale, profondo e vellutato.",
                "Delay granulare asimmetrico che riproduce frammenti fluttuanti nello spazio stereo.",
                "Eco inverso che campiona e capovolge le code di ritardo in tempo reale."
            }
        },
        {
            3, 3, "D", "TEXTURE", juce::Colour (0xFF3B82F6), "textType",
            { "OFF", "FILTER", "SQUASH", "CASSETTE", "BROKEN", "INTERFERENCE" },
            {
                "Modulo Texture disattivato.",
                "Filtro State-Variable Cytomic TPT risonante senza alcun rischio di blow-up.",
                "Compressore ottico LA-2A con rilascio caldo e punch immediato.",
                "Nastro a cassetta Type-I con roll-off morbido degli acuti e rumore di fondo.",
                "Degrado digitale/analogico, drop-out e imperfezioni da cavo vintage liso.",
                "Radiofrequenze AM, ronzio a induzione magnetica e anelli dolci."
            }
        }
    };

    const int startX = 16;
    const int startY = 62;
    const int stripW = 222;
    const int stripH = 376;
    const int gapX   = 10;

    for (int i = 0; i < 4; ++i)
    {
        const auto& strip = strips[i];
        int x = startX + i * (stripW + gapX);
        auto rect = juce::Rectangle<int> (x, startY, stripW, stripH);

        // Strip background panel
        g.setColour (juce::Colour (0xFF181A22));
        g.fillRoundedRectangle (rect.toFloat(), 10.0f);

        // Border highlighted with module accent colour
        g.setColour (strip.accent.withAlpha (0.45f));
        g.drawRoundedRectangle (rect.toFloat(), 10.0f, 1.4f);

        // Header pill
        auto headerRect = juce::Rectangle<int> (x + 8, startY + 8, stripW - 16, 26);
        g.setColour (strip.accent.withAlpha (0.22f));
        g.fillRoundedRectangle (headerRect.toFloat(), 6.0f);
        g.setColour (strip.accent);
        g.drawRoundedRectangle (headerRect.toFloat(), 6.0f, 1.2f);

        g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
        g.drawText ("[" + strip.letter + "] " + strip.name, headerRect.reduced (6, 0), juce::Justification::centredLeft);

        // Active Routing Position indicator badge (e.g. "POS 1", "POS 2")
        int posInChain = 1;
        for (int s = 0; s < 4; ++s)
            if (orderSlots[s] == strip.moduleKind)
                posInChain = s + 1;

        auto posBadge = juce::Rectangle<int> (headerRect.getRight() - 56, headerRect.getY() + 3, 50, 20);
        g.setColour (juce::Colour (0xFF262836));
        g.fillRoundedRectangle (posBadge.toFloat(), 4.0f);
        g.setColour (juce::Colour (0xFFE5E7EB));
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText ("#" + juce::String (posInChain), posBadge, juce::Justification::centred);

        // Algorithm Pills / Radio Buttons
        int curAlgo = static_cast<int> (audioProcessor.apvts.getRawParameterValue (strip.paramID)->load());
        const int algoStartY = startY + 40;
        const int algoH = 22;
        const int algoGap = 3;

        for (int a = 0; a < static_cast<int> (strip.algoNames.size()); ++a)
        {
            auto pillRect = juce::Rectangle<int> (x + 10, algoStartY + a * (algoH + algoGap), stripW - 20, algoH);
            bool isSelected = (curAlgo == a);

            if (isSelected)
            {
                g.setColour (strip.accent.withAlpha (0.85f));
                g.fillRoundedRectangle (pillRect.toFloat(), 5.0f);
                g.setColour (juce::Colour (0xFF0E0F14));
                g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            }
            else
            {
                g.setColour (juce::Colour (0xFF20222D));
                g.fillRoundedRectangle (pillRect.toFloat(), 5.0f);
                g.setColour (juce::Colour (0xFFB3B8C8));
                g.setFont (juce::FontOptions (11.0f));
            }

            // Status LED dot
            auto ledRect = juce::Rectangle<float> (static_cast<float> (pillRect.getX() + 8),
                                                   static_cast<float> (pillRect.getCentreY() - 3.5f), 7.0f, 7.0f);
            if (isSelected)
            {
                g.setColour (juce::Colour (0xFFFBF8F0));
                g.fillEllipse (ledRect);
            }
            else
            {
                g.setColour (juce::Colour (0xFF4A4E62));
                g.fillEllipse (ledRect);
            }

            g.drawText (strip.algoNames[a], pillRect.withTrimmedLeft (22), juce::Justification::centredLeft);

            // Register clickable hit-box
            interactiveBoxes.push_back ({
                pillRect, strip.paramID, a,
                strip.name + " > " + strip.algoNames[a],
                strip.algoDescs[a]
            });
        }
    }

    // Bottom Master Panel Area
    auto masterRect = juce::Rectangle<int> (16, 448, getWidth() - 32, 90);
    g.setColour (juce::Colour (0xFF181A22));
    g.fillRoundedRectangle (masterRect.toFloat(), 10.0f);
    g.setColour (juce::Colour (0xFF2F3240));
    g.drawRoundedRectangle (masterRect.toFloat(), 10.0f, 1.2f);

    // VU Meters
    float inPeak  = audioProcessor.getInputLevel();
    float outPeak = audioProcessor.getOutputLevel();

    auto meterRect = juce::Rectangle<int> (getWidth() - 170, 458, 140, 70);
    g.setColour (juce::Colour (0xFF121319));
    g.fillRoundedRectangle (meterRect.toFloat(), 6.0f);
    g.setColour (juce::Colour (0xFF2A2C38));
    g.drawRoundedRectangle (meterRect.toFloat(), 6.0f, 1.0f);

    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.setColour (juce::Colour (0xFF9CA3AF));
    g.drawText ("INPUT",  meterRect.getX() + 6, meterRect.getY() + 6,  40, 14, juce::Justification::left);
    g.drawText ("OUTPUT", meterRect.getX() + 6, meterRect.getY() + 38, 40, 14, juce::Justification::left);

    auto drawMeterBar = [&g] (juce::Rectangle<int> barBounds, float level, juce::Colour col)
    {
        g.setColour (juce::Colour (0xFF1E202A));
        g.fillRoundedRectangle (barBounds.toFloat(), 3.0f);
        float fillW = barBounds.getWidth() * juce::jlimit (0.0f, 1.0f, level);
        if (fillW > 1.0f)
        {
            auto fillBounds = barBounds.toFloat().withWidth (fillW);
            g.setColour (col);
            g.fillRoundedRectangle (fillBounds, 3.0f);
        }
    };

    drawMeterBar (juce::Rectangle<int> (meterRect.getX() + 52, meterRect.getY() + 10, 80, 8), inPeak,  juce::Colour (0xFFF59E0B));
    drawMeterBar (juce::Rectangle<int> (meterRect.getX() + 52, meterRect.getY() + 42, 80, 8), outPeak, juce::Colour (0xFF10B981));

    // Dynamic Hover Banner at the bottom
    auto bannerRect = juce::Rectangle<int> (16, 544, getWidth() - 32, 28);
    g.setColour (juce::Colour (0xFF101116));
    g.fillRoundedRectangle (bannerRect.toFloat(), 6.0f);
    g.setColour (juce::Colour (0xFF242633));
    g.drawRoundedRectangle (bannerRect.toFloat(), 6.0f, 1.0f);

    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (juce::Colour (0xFFF59E0B));
    g.drawText (currentHoverTitle + ":", bannerRect.getX() + 10, bannerRect.getY(), 230, bannerRect.getHeight(), juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (11.0f));
    g.setColour (juce::Colour (0xFFE5E7EB));
    g.drawText (currentHoverText, bannerRect.getX() + 245, bannerRect.getY(), bannerRect.getWidth() - 255, bannerRect.getHeight(), juce::Justification::centredLeft, true);
}

//==============================================================================
void ChromaCosmAudioProcessorEditor::resized()
{
    // Top Bar
    presetBox.setBounds (270, 14, 100, 32);

    chromaOrderBox.setBounds (734, 14, 210, 32);

    // We can position the remaining controls cleanly
    openGuideBtn.setBounds (getWidth() - 100, 458, 86, 28);
    infoTooltipsBtn.setBounds (getWidth() - 210, 458, 100, 28);
    chromaCalibBox.setBounds (getWidth() - 210, 494, 196, 28);

    // Sliders & Buttons in the 4 strips
    const int startX = 16;
    const int startY = 62;
    const int stripW = 222;
    const int gapX   = 10;

    // Strip A: Character
    int xA = startX + 0 * (stripW + gapX);
    charTiltKnob.setBounds (xA + 14, startY + 200, 88, 88);
    charAmtKnob.setBounds  (xA + 120, startY + 200, 88, 88);
    charVolKnob.setBounds  (xA + 66, startY + 298, 90, 72);
    charCycleBtn.setBounds (xA + 14, startY + 298, 48, 26);
    charCycleBtn.setVisible (false); // Integrated in header and clickable pills

    // Strip B: Movement
    int xB = startX + 1 * (stripW + gapX);
    moveRateKnob.setBounds  (xB + 14, startY + 200, 88, 88);
    moveAmtKnob.setBounds   (xB + 120, startY + 200, 88, 88);
    moveDriftKnob.setBounds (xB + 14, startY + 298, 90, 72);
    moveVolKnob.setBounds   (xB + 118, startY + 298, 90, 72);
    moveCycleBtn.setVisible (false);

    // Strip C: Diffusion
    int xC = startX + 2 * (stripW + gapX);
    diffTimeKnob.setBounds  (xC + 14, startY + 200, 88, 88);
    diffAmtKnob.setBounds   (xC + 120, startY + 200, 88, 88);
    diffDriftKnob.setBounds (xC + 14, startY + 298, 90, 72);
    diffVolKnob.setBounds   (xC + 118, startY + 298, 90, 72);
    diffCycleBtn.setVisible (false);

    // Strip D: Texture
    int xD = startX + 3 * (stripW + gapX);
    textTiltKnob.setBounds (xD + 14, startY + 200, 88, 88);
    textAmtKnob.setBounds  (xD + 120, startY + 200, 88, 88);
    textVolKnob.setBounds  (xD + 66, startY + 298, 90, 72);
    textCycleBtn.setVisible (false);

    // Master Controls (Bottom Left to Middle)
    chromaMixKnob.setBounds   (32, 452, 80, 80);
    globalTiltKnob.setBounds  (124, 452, 80, 80);
    chromaLevelKnob.setBounds (216, 452, 80, 80);

    // Boutique Footswitches
    chromaCaptureBtn.setBounds (330, 464, 110, 48);
    tapTempoBtn.setBounds      (450, 464, 88, 48);
    chromaBypassBtn.setBounds  (548, 464, 100, 48);

    // Overlay
    guideOverlay.setBounds (getLocalBounds());
}
