#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

//==============================================================================
// Custom Hardware LookAndFeel inspired by Hologram Electronics Pedals
//==============================================================================
class HologramLookAndFeel : public juce::LookAndFeel_V4
{
public:
    HologramLookAndFeel()
    {
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xFF232329));
        setColour (juce::ComboBox::textColourId,       juce::Colour (0xFFF4F1EA));
        setColour (juce::ComboBox::outlineColourId,    juce::Colour (0xFF454552));
        setColour (juce::PopupMenu::backgroundColourId,juce::Colour (0xFF1E1E24));
        setColour (juce::PopupMenu::textColourId,      juce::Colour (0xFFF4F1EA));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xFFD94336));
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xF2191A22));
        setColour (juce::TooltipWindow::textColourId,       juce::Colour (0xFFF5F2EB));
        setColour (juce::TooltipWindow::outlineColourId,    juce::Colour (0xFFECA820));
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

        // Outer scale ticks
        g.setColour (juce::Colour (0x55000000));
        for (int i = 0; i <= 10; ++i)
        {
            float a = rotaryStartAngle + (static_cast<float> (i) / 10.0f) * (rotaryEndAngle - rotaryStartAngle);
            float r1 = radius * 1.08f;
            float r2 = radius * 1.18f;
            g.drawLine (centreX + std::sin (a) * r1, centreY - std::cos (a) * r1,
                        centreX + std::sin (a) * r2, centreY - std::cos (a) * r2, 1.3f);
        }

        // Value arc
        juce::Path valueArc;
        valueArc.addCentredArc (centreX, centreY, radius * 1.12f, radius * 1.12f, 0.0f,
                                rotaryStartAngle, angle, true);
        g.setColour (accent);
        g.strokePath (valueArc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Drop shadow
        g.setColour (juce::Colour (0x66000000));
        g.fillEllipse (rx + 1.5f, ry + 3.0f, rw, rw);

        // Fluted dark Bakelite / anodized aluminum knob body
        juce::ColourGradient bodyGrad (juce::Colour (0xFF383840), centreX - radius * 0.5f, centreY - radius * 0.5f,
                                       juce::Colour (0xFF18181C), centreX + radius * 0.5f, centreY + radius * 0.5f, true);
        g.setGradientFill (bodyGrad);
        g.fillEllipse (rx, ry, rw, rw);

        // Metallic inner cap
        const float capR = radius * 0.74f;
        juce::ColourGradient capGrad (juce::Colour (0xFF4B4B55), centreX, centreY - capR,
                                      juce::Colour (0xFF26262C), centreX, centreY + capR, false);
        g.setGradientFill (capGrad);
        g.fillEllipse (centreX - capR, centreY - capR, capR * 2.0f, capR * 2.0f);

        g.setColour (juce::Colour (0xFF111115));
        g.drawEllipse (rx, ry, rw, rw, 1.5f);

        // High-contrast cream pointer line
        juce::Path p;
        const float pointerLength = radius * 0.78f;
        const float pointerThickness = 2.8f;
        p.addRoundedRectangle (-pointerThickness * 0.5f, -radius * 0.92f, pointerThickness, pointerLength, 1.2f);
        p.applyTransform (juce::AffineTransform::rotation (angle).translated (centreX, centreY));

        g.setColour (juce::Colour (0xFFF7F4EB));
        g.fillPath (p);
    }
};

//==============================================================================
// Built-in Interactive Manual / Guide Overlay Component
//==============================================================================
class GuideOverlayComponent : public juce::Component
{
public:
    GuideOverlayComponent()
    {
        for (int i = 0; i < 4; ++i)
        {
            tabBtns[i].setButtonText (tabNames[i]);
            tabBtns[i].onClick = [this, i]
            {
                activeTab = i;
                updateTabColors();
                repaint();
            };
            addAndMakeVisible (tabBtns[i]);
        }

        closeBtn.setButtonText ("CHIUDI GUIDA [X]");
        closeBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFFD94336));
        closeBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        closeBtn.onClick = [this] { setVisible (false); };
        addAndMakeVisible (closeBtn);

        updateTabColors();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (60, 42);
        int tabY = area.getY() + 54;
        int tabW = (area.getWidth() - 48) / 4;
        for (int i = 0; i < 4; ++i)
            tabBtns[i].setBounds (area.getX() + 16 + i * (tabW + 5), tabY, tabW, 32);

        closeBtn.setBounds (area.getRight() - 156, area.getY() + 12, 140, 32);
    }

    void paint (juce::Graphics& g) override;

private:
    void updateTabColors()
    {
        const juce::Colour cols[4] = {
            juce::Colour (0xFFE04838), juce::Colour (0xFFECA820),
            juce::Colour (0xFF28A45E), juce::Colour (0xFF2D82C6)
        };
        for (int i = 0; i < 4; ++i)
        {
            tabBtns[i].setColour (juce::TextButton::buttonColourId,
                                  (activeTab == i) ? cols[i] : juce::Colour (0xFF2B2C36));
            tabBtns[i].setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        }
    }

    const char* tabNames[4] = {
        "1. PANORAMICA & ROUTING",
        "2. CHROMA CONSOLE (SX)",
        "3. MICROCOSM (DX)",
        "4. LOOPER & CONSIGLI"
    };
    int activeTab = 0;
    juce::TextButton tabBtns[4];
    juce::TextButton closeBtn;
};

//==============================================================================
// Main Dual-Pedal Editor
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
    HologramLookAndFeel lookAndFeel;
    std::unique_ptr<juce::TooltipWindow> tooltipWindow;

    // Attachments container
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;

    // Top Bar Controls
    juce::ComboBox presetBox;
    juce::TextButton prevPresetBtn { "<" }, nextPresetBtn { ">" };
    juce::ComboBox comboRoutingBox;
    juce::ToggleButton infoTooltipsBtn { "INFO MOUSE" };
    juce::TextButton openGuideBtn { "? GUIDA" };
    juce::TextButton tapTempoBtn { "TAP" };
    juce::Slider masterOutKnob;

    //==========================================================================
    // CHROMA CONSOLE CONTROLS (Left Pedal)
    //==========================================================================
    juce::ComboBox chromaOrderBox;
    juce::ComboBox chromaCalibBox;

    // Strip A: Character (Red)
    juce::Slider charTiltKnob, charAmtKnob, charVolKnob;
    juce::TextButton charCycleBtn { "A" };

    // Strip B: Movement (Yellow)
    juce::Slider moveRateKnob, moveAmtKnob, moveDriftKnob;
    juce::TextButton moveCycleBtn { "B" };

    // Strip C: Diffusion (Green)
    juce::Slider diffTimeKnob, diffAmtKnob, diffDriftKnob;
    juce::TextButton diffCycleBtn { "C" };

    // Strip D: Texture (Blue)
    juce::Slider textTiltKnob, textAmtKnob, textVolKnob;
    juce::TextButton textCycleBtn { "D" };

    // Chroma Master
    juce::Slider chromaMixKnob, chromaLevelKnob;
    juce::ToggleButton chromaCaptureBtn { "CAPTURE" };
    juce::ToggleButton chromaBypassBtn { "BYPASS" };

    // Clickable & hoverable hit-boxes for Chroma mode labels & Microcosm effect pills
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
    juce::String currentHoverTitle = "SUGGERIMENTO RAPIDO";
    juce::String currentHoverText  = "Passa il mouse su qualsiasi pomello, tasto o effetto per leggerne la descrizione qui, oppure clicca su '? GUIDA' in alto.";

    //==========================================================================
    // MICROCOSM CONTROLS (Right Pedal)
    //==========================================================================
    juce::Slider microActivityKnob, microShapeKnob, microFilterKnob, microMixKnob;
    juce::Slider microTimeKnob, microRepeatsKnob, microSpaceKnob, loopLevelKnob;
    juce::Slider microResKnob, microPitchRateKnob, microPitchDepthKnob;

    juce::ComboBox microSubdivBox;
    juce::ComboBox microSpaceModeBox;
    juce::ToggleButton microSyncBtn { "SYNC" };
    juce::ToggleButton microRevBtn  { "REV" };
    juce::ToggleButton microHoldBtn { "HOLD SAMPLER" };
    juce::ToggleButton microFxOffBtn { "LOOPER ONLY" };
    juce::ToggleButton microBypassBtn { "BYPASS" };

    // Phrase Looper Controls
    juce::TextButton looperRecPlayBtn { "REC / PLAY" };
    juce::TextButton looperStopBtn    { "STOP" };
    juce::TextButton looperClearBtn   { "CLEAR" };
    juce::ComboBox   looperSpeedBox;
    juce::ToggleButton looperRevBtn   { "REV LOOP" };
    juce::ToggleButton looperPreBtn   { "PRE-FX" };

    GuideOverlayComponent guideOverlay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChromaCosmAudioProcessorEditor)
};
