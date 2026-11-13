#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

//==============================================================================
// Custom Boutique Hardware LookAndFeel for Chroma Console
//==============================================================================
class ChromaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ChromaLookAndFeel()
    {
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xFF22232A));
        setColour (juce::ComboBox::textColourId,       juce::Colour (0xFFF4F1EA));
        setColour (juce::ComboBox::outlineColourId,    juce::Colour (0xFF3F414D));
        setColour (juce::PopupMenu::backgroundColourId,juce::Colour (0xFF1A1B22));
        setColour (juce::PopupMenu::textColourId,      juce::Colour (0xFFF4F1EA));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xFFE24A3B));
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xF514151C));
        setColour (juce::TooltipWindow::textColourId,       juce::Colour (0xFFF5F2EB));
        setColour (juce::TooltipWindow::outlineColourId,    juce::Colour (0xFFF59E0B));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, const float rotaryStartAngle,
                           const float rotaryEndAngle, juce::Slider& slider) override
    {
        const float radius = static_cast<float> (juce::jmin (width, height)) * 0.42f;
        const float centreX = static_cast<float> (x) + static_cast<float> (width) * 0.5f;
        const float centreY = static_cast<float> (y) + static_cast<float> (height) * 0.5f;
        const float rx = centreX - radius;
        const float ry = centreY - radius;
        const float rw = radius * 2.0f;
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        const juce::Colour accent = slider.findColour (juce::Slider::rotarySliderFillColourId);

        // Scale tick marks
        g.setColour (juce::Colour (0x44000000));
        for (int i = 0; i <= 10; ++i)
        {
            float a = rotaryStartAngle + (static_cast<float> (i) / 10.0f) * (rotaryEndAngle - rotaryStartAngle);
            float r1 = radius * 1.08f;
            float r2 = radius * 1.18f;
            g.drawLine (centreX + std::sin (a) * r1, centreY - std::cos (a) * r1,
                        centreX + std::sin (a) * r2, centreY - std::cos (a) * r2, 1.2f);
        }

        // Active Value Arc
        juce::Path valueArc;
        valueArc.addCentredArc (centreX, centreY, radius * 1.12f, radius * 1.12f, 0.0f,
                                rotaryStartAngle, angle, true);
        g.setColour (accent);
        g.strokePath (valueArc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Soft drop shadow
        g.setColour (juce::Colour (0x66000000));
        g.fillEllipse (rx + 1.5f, ry + 3.0f, rw, rw);

        // Dark anodized metal body
        juce::ColourGradient bodyGrad (juce::Colour (0xFF3A3B44), centreX - radius * 0.5f, centreY - radius * 0.5f,
                                       juce::Colour (0xFF181920), centreX + radius * 0.5f, centreY + radius * 0.5f, true);
        g.setGradientFill (bodyGrad);
        g.fillEllipse (rx, ry, rw, rw);

        // Machined inner cap
        const float capR = radius * 0.72f;
        juce::ColourGradient capGrad (juce::Colour (0xFF4E505E), centreX, centreY - capR,
                                      juce::Colour (0xFF22232B), centreX, centreY + capR, false);
        g.setGradientFill (capGrad);
        g.fillEllipse (centreX - capR, centreY - capR, capR * 2.0f, capR * 2.0f);

        g.setColour (juce::Colour (0xFF101014));
        g.drawEllipse (rx, ry, rw, rw, 1.4f);

        // High-contrast cream indicator line
        juce::Path p;
        const float pointerLength = radius * 0.78f;
        const float pointerThickness = 2.6f;
        p.addRoundedRectangle (-pointerThickness * 0.5f, -radius * 0.92f, pointerThickness, pointerLength, 1.2f);
        p.applyTransform (juce::AffineTransform::rotation (angle).translated (centreX, centreY));

        g.setColour (juce::Colour (0xFFFBF8F0));
        g.fillPath (p);
    }
};

//==============================================================================
// Interactive User Manual Overlay
//==============================================================================
class ChromaGuideOverlayComponent : public juce::Component
{
public:
    ChromaGuideOverlayComponent()
    {
        for (int i = 0; i < 3; ++i)
        {
            tabBtns[i].setButtonText (tabNames[i]);
            tabBtns[i].onClick = [this, i] { activeTab = i; repaint(); };
            addAndMakeVisible (tabBtns[i]);
        }

        closeBtn.setButtonText ("CHIUDI (X)");
        closeBtn.onClick = [this] { setVisible (false); };
        addAndMakeVisible (closeBtn);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xF20F1016));

        auto bounds = getLocalBounds().reduced (24);
        g.setColour (juce::Colour (0xFF1C1D26));
        g.fillRoundedRectangle (bounds.toFloat(), 12.0f);
        g.setColour (juce::Colour (0xFF3F414D));
        g.drawRoundedRectangle (bounds.toFloat(), 12.0f, 1.5f);

        auto contentArea = bounds.reduced (20).withTrimmedTop (50);

        g.setColour (juce::Colour (0xFFF4F1EA));
        g.setFont (juce::FontOptions (14.0f));

        if (activeTab == 0)
        {
            juce::String text =
                "=== BENVENUTO IN CHROMA CONSOLE (DaMA Audio) ===\n\n"
                "Chroma Console e un processore di colorazione analogica, saturazione, modulazione e spazialita da studio.\n"
                "Ispirato al celebre pedale boutique, unisce 4 motori di colorazione con riordinamento flessibile:\n\n"
                "- [A] CHARACTER: Distorsione, saturazione valvolare, EQ dinamico Tilt, e Swell d'inviluppo.\n"
                "- [B] MOVEMENT: Flanger/Doubler, Vibrato a nastro con flutter organico, Phaser analogico e Tremolo.\n"
                "- [C] DIFFUSION: Delay a cascata, emulazione bobine a nastro (Reels), riverbero Space e Reverse tape.\n"
                "- [D] TEXTURE: Filtro analogico risonante, compressore ottico Squash, cassetta Lo-Fi e Broken radio.\n\n"
                "FILOSOFIA DI DESIGN:\n"
                "Ogni algoritmo e calibrato per dare immediatamente calore, presenza e musicalita a qualsiasi sorgente:\n"
                "chitarre elettriche e acustiche, sintetizzatori, tastiere vintage, bassi, batterie e intere voci di mix.";
            g.drawFittedText (text, contentArea, juce::Justification::topLeft, 18);
        }
        else if (activeTab == 1)
        {
            juce::String text =
                "=== I 20 ALGORITMI DEI 4 MODULI ===\n\n"
                "[A] CHARACTER:\n"
                "  1. DRIVE: Overdrive analogico cremoso con risposta dinamica al plettrato.\n"
                "  2. SWEETEN: Preamp valvolare da studio con compressione armonica delicata.\n"
                "  3. FUZZ: Silicio vintage ad alto guadagno con sustain infinito e ricchezza di armoniche.\n"
                "  4. HOWL: Risoluzione preamplificata con filtro risonante aggressivo e carattere scuro.\n"
                "  5. SWELL: Inviluppo automatico 'slow gear' che elimina l'attacco per creare archi e pad.\n\n"
                "[B] MOVEMENT:\n"
                "  1. DOUBLER: Chorus e flanger a nastro stereo con micro-spostamento temporale.\n"
                "  2. VIBRATO: Oscillazione d'intonazione organica con wow & flutter da vero registratore a nastro.\n"
                "  3. PHASER: 6 stadi all-pass caldi con feedback analogico e movimento circolare.\n"
                "  4. TREMOLO: Modulazione d'ampiezza ritmica con curva morbida o a onda quadra.\n"
                "  5. PITCH: Doppiatore con trasposizione d'ottava e modulazione fluttuante shimmer.\n\n"
                "[C] DIFFUSION:\n"
                "  1. CASCADE: Delay ritmico stereo a stadi con diffusion cross-coupled.\n"
                "  2. REELS: Delay a bobine aperte (Tape Reel-to-Reel) con saturazione progressiva delle ripetizioni.\n"
                "  3. SPACE: Riverbero plate/cavern tridimensionale, profondo e vellutato.\n"
                "  4. COLLAGE: Delay granulare asimmetrico che riproduce frammenti fluttuanti nello spazio stereo.\n"
                "  5. REVERSE: Eco inverso a nastro che cattura e capovolge le code di ritardo in tempo reale.\n\n"
                "[D] TEXTURE:\n"
                "  1. FILTER: Filtro State-Variable Cytomic TPT con risonanza musicale anti-blowup.\n"
                "  2. SQUASH: Compressore ottico da studio stile LA-2A con rilascio caldo e punch immediato.\n"
                "  3. CASSETTE: Emulazione nastro a cassetta Type-I con roll-off acuti, rumore di fondo e compressione.\n"
                "  4. BROKEN: Riduzione di risoluzione analogica, drop-out e imperfezioni da cavo guasto/altoparlante liso.\n"
                "  5. INTERFERENCE: Radiofrequenze AM, ronzio a induzione magnetica e modulazione ad anello dolce.";
            g.drawFittedText (text, contentArea, juce::Justification::topLeft, 28);
        }
        else
        {
            juce::String text =
                "=== ROUTING, CAPTURE & CONSIGLI DA STUDIO ===\n\n"
                "1. SELETTORE ROUTING ORDER:\n"
                "   Cambiare l'ordine dei 4 moduli trasforma radicalmente il suono!\n"
                "   - Standard (Char > Move > Diff > Text): perfetto per suoni puliti e rifiniti da mix.\n"
                "   - Diff > Char (Reverb into Drive): crea muri sonori epici in stile Shoegaze e Post-Rock.\n"
                "   - Text > Char (Pre-Filter/Squash): scolpisce il segnale prima della saturazione per massima chiarezza.\n"
                "   - Move > Diff > Char: modulazioni vintage in stile Hendrix/Pink Floyd prima dell'amplificatore.\n\n"
                "2. FOOTSWITCH CAPTURE (Sustainer Looper):\n"
                "   Premendo CAPTURE, il pedale congela e trattiene in un micro-loop morbido l'ultimo frammento audio,\n"
                "   trasformandolo in un drone continuo o pad su cui puoi suonare liberamente sopra senza consumare memoria.\n\n"
                "3. CONTROLLO DRIFT ORGANICO:\n"
                "   La manopola DRIFT nei moduli Movement e Diffusion inietta un algoritmo proprietario di rumore Pink/Perlin\n"
                "   multilivello che simula le imperfezioni fisiche dei motori a nastro vintage (wow, flutter e instabilita termica).\n\n"
                "4. TAP TEMPO & CALIBRATION:\n"
                "   Il tasto TAP permette di impostare il tempo a mano oppure si sincronizza automaticamente alla DAW.\n"
                "   Il selettore CALIB (Lo-Gain, Line, Hot/Inst, High Headroom) adatta il preamplificatore a chitarre passive,\n"
                "   chitarre attive, sintetizzatori di linea o tracce master ad alto volume.";
            g.drawFittedText (text, contentArea, juce::Justification::topLeft, 24);
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (24);
        auto topBar = bounds.removeFromTop (44).reduced (10, 4);

        int tabW = 190;
        for (int i = 0; i < 3; ++i)
            tabBtns[i].setBounds (topBar.removeFromLeft (tabW).reduced (3, 0));

        closeBtn.setBounds (topBar.removeFromRight (110).reduced (3, 0));
    }

private:
    const juce::String tabNames[3] = {
        "1. PANORAMICA",
        "2. I 20 ALGORITMI",
        "3. ROUTING & CAPTURE"
    };
    int activeTab = 0;
    juce::TextButton tabBtns[3];
    juce::TextButton closeBtn;
};

//==============================================================================
// Main Chroma Console Plugin Editor
//==============================================================================
class ChromaCosmAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit ChromaCosmAudioProcessorEditor (ChromaCosmAudioProcessor&);
    ~ChromaCosmAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseMove (const juce::MouseEvent& event) override;

private:
    void timerCallback() override;
    void setupKnob (juce::Slider& s, const juce::String& paramID, juce::Colour accentColour, const juce::String& desc);
    void registerControlDescription (juce::Component& comp, const juce::String& title, const juce::String& desc);
    void applyTooltipState();
    void updateHoverBannerFromPoint (juce::Point<int> pt);

    ChromaCosmAudioProcessor& audioProcessor;
    ChromaLookAndFeel lookAndFeel;
    std::unique_ptr<juce::TooltipWindow> tooltipWindow;

    // Attachments
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;

    // Top Bar Controls
    juce::ComboBox presetBox;
    juce::TextButton prevPresetBtn { "<" }, nextPresetBtn { ">" };
    juce::ComboBox chromaOrderBox;
    juce::ComboBox chromaCalibBox;
    juce::ToggleButton infoTooltipsBtn { "INFO MOUSE" };
    juce::TextButton openGuideBtn { "? GUIDA" };
    juce::TextButton tapTempoBtn { "TAP" };

    // Strip A: CHARACTER (Red)
    juce::Slider charTiltKnob, charAmtKnob, charVolKnob;
    juce::TextButton charCycleBtn { "A: CHAR" };

    // Strip B: MOVEMENT (Yellow)
    juce::Slider moveRateKnob, moveAmtKnob, moveDriftKnob, moveVolKnob;
    juce::TextButton moveCycleBtn { "B: MOVE" };

    // Strip C: DIFFUSION (Green)
    juce::Slider diffTimeKnob, diffAmtKnob, diffDriftKnob, diffVolKnob;
    juce::TextButton diffCycleBtn { "C: DIFF" };

    // Strip D: TEXTURE (Blue)
    juce::Slider textTiltKnob, textAmtKnob, textVolKnob;
    juce::TextButton textCycleBtn { "D: TEXT" };

    // Master Controls
    juce::Slider chromaMixKnob, globalTiltKnob, chromaLevelKnob;
    juce::ToggleButton chromaCaptureBtn { "CAPTURE" };
    juce::ToggleButton chromaBypassBtn  { "BYPASS" };

    // Interactive clickable boxes for algorithm choices
    struct ModeHitBox
    {
        juce::Rectangle<int> bounds;
        juce::String paramID;
        int choiceIndex;
        juce::String hoverTitle;
        juce::String hoverDesc;
    };
    std::vector<ModeHitBox> interactiveBoxes;

    struct CompHoverInfo
    {
        juce::Component* comp = nullptr;
        juce::String title;
        juce::String desc;
    };
    std::vector<CompHoverInfo> compDescriptions;

    bool tooltipsEnabled = true;
    juce::String currentHoverTitle = "SUGGERIMENTO RAPIDO CHROMA CONSOLE";
    juce::String currentHoverText  = "Passa il mouse su qualsiasi controllo o clicca direttamente sui nomi degli algoritmi per attivarli. Clicca su '? GUIDA' per il manuale completo.";

    ChromaGuideOverlayComponent guideOverlay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChromaCosmAudioProcessorEditor)
};
