#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::Font uiFont (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }

    const juce::Colour kRed    (0xFFE04838);
    const juce::Colour kYellow (0xFFECA820);
    const juce::Colour kGreen  (0xFF28A45E);
    const juce::Colour kBlue   (0xFF2D82C6);

    //==========================================================================
    // Hover descriptions: Chroma Console modes  [module][mode]
    //==========================================================================
    const char* chromaModeDescs[4][6] =
    {
        { "",
          "Saturazione valvolare asimmetrica, calda e dinamica. AMOUNT = quantita' di drive, TILT = colore (scuro <-> brillante).",
          "Preamp da console: compressione ottica morbida + armoniche e brillantezza. Rende il suono piu' 'finito' e presente.",
          "Fuzz aggressivo multi-stadio con leggero gate 'sputter'. AMOUNT al massimo = muro di fuzz.",
          "Distorsione con filtro risonante che segue l'inviluppo e 'urla'. TILT sposta la frequenza del picco risonante.",
          "Volume swell automatico (effetto violino). TILT = sensibilita' di innesco, AMOUNT = lentezza dell'attacco." },
        { "",
          "Raddoppio stereo tipo ADT a nastro: allarga e ispessisce. RATE = distanza del raddoppio, AMOUNT = quantita'.",
          "Vibrato di pitch tipo nastro/BBD. RATE = velocita', AMOUNT = profondita', DRIFT = instabilita'.",
          "Phaser ottico vintage a 6 stadi con feedback. RATE = velocita' dello sweep, AMOUNT = intensita'.",
          "Tremolo stereo valvolare. RATE = velocita', AMOUNT = profondita' (piu' alto = piu' stereo).",
          "Pitch shifter: RATE sceglie l'intervallo (-12, -7, -5, detune, +5, +7, +12 semitoni), AMOUNT = quantita' di segnale trasposto." },
        { "",
          "Delay multi-tap 'a cascata' con ripetizioni incrociate L/R. TIME = tempo, AMOUNT = quantita' e feedback.",
          "Eco a nastro (reel-to-reel) con saturazione, perdita di alti e wow/flutter (DRIFT). TIME = tempo di eco.",
          "Riverbero ambient modulato con pre-delay. TIME = grandezza della stanza, DRIFT = piu' luminoso/scuro.",
          "Delay granulare ritmico con frammenti trasposti di ottava: crea collage sonori. TIME = lunghezza delle fette.",
          "Eco al contrario a doppia finestra (reverse delay). TIME = lunghezza del frammento invertito." },
        { "",
          "Filtro morphing: TILT a sinistra = passa-basso, al centro = aperto, a destra = passa-alto. AMOUNT = intensita' e risonanza.",
          "Compressore VCA/nastro 'pompato'. AMOUNT = quantita' di compressione, TILT = velocita' di rilascio.",
          "Registratore a cassetta 4 piste: flutter, saturazione, perdita di alti e fruscio. TILT = usura/brillantezza del nastro.",
          "Circuito 'rotto': bitcrush, riduzione del sample-rate e micro-dropout. TILT = finezza del glitch.",
          "Interferenza radio: ring-modulator, statica e scariche. TILT = sintonia della portante." }
    };

    const char* chromaModeNames[4][6] =
    {
        { "OFF", "DRIVE",   "SWEETEN", "FUZZ",     "HOWL",    "SWELL" },
        { "OFF", "DOUBLER", "VIBRATO", "PHASER",   "TREMOLO", "PITCH" },
        { "OFF", "CASCADE", "REELS",   "SPACE",    "COLLAGE", "REVERSE" },
        { "OFF", "FILTER",  "SQUASH",  "CASSETTE", "BROKEN",  "INTERFERENCE" }
    };

    //==========================================================================
    // Hover descriptions: Microcosm effects + variations
    //==========================================================================
    const char* microFxNames[11] =
    {
        "MOSAIC", "SEQ", "GLIDE", "HAZE", "TUNNEL", "STRUM",
        "BLOCKS", "INTERRUPT", "ARP", "PATTERN", "WARP"
    };

    const char* microFxDescs[11] =
    {
        "MICRO LOOP - Micro-loop sovrapposti a velocita' diverse (0.5x ottava sotto, 1x, 2x ottava sopra): un mosaico scintillante.",
        "MICRO LOOP - Il suono viene tagliato in fette e riorganizzato in una sequenza ritmica a 8 step sincronizzata al tempo.",
        "MICRO LOOP - Loop brevi sovrapposti che scivolano di intonazione (glissando) mentre sfumano.",
        "GRANULES - Nuvola densa e vellutata di grani: trasforma qualsiasi suono in un pad ambient immediato.",
        "GRANULES - Cicli brevissimi accordati che generano droni ipnotici e risonanti.",
        "GRANULES - Ad ogni attacco di nota parte una cascata di grani 'ad arpa' (unisono, quinta, ottave).",
        "GLITCH - Blocchi ritmici quantizzati che si ripetono: stutter musicale tipo CD inceppato.",
        "GLITCH - Burst trasposti che interrompono e abbassano (ducking) il segnale pulito.",
        "GLITCH - Frammenti di cio' che suoni arpeggiati automaticamente su un accordo.",
        "MULTI-DELAY - Rete di delay multi-tap stereo con schemi ritmici complessi.",
        "MULTI-DELAY - Multi-tap con tempi e pitch modulati e 'piegati': echi fluttuanti e sognanti."
    };

    const char* microVarDescs[11] =
    {
        "A bilanciato, B brillante (ottave e quinte alte), C profondo (sub-ottave), D intreccio di quinte e sub.",
        "A, B, C, D: quattro diverse sequenze di intonazione a 8 step.",
        "A 1x -> quinta su, B ottava giu' -> 1x, C 1x -> ottava giu', D ottava su -> 1x.",
        "A quasi intonato, B detune ampio, C con sub-ottave, D con ottave alte.",
        "A-D: risonanze accordate su La1, Mi2, La2, Re3 (B e D suonano un'ottava sotto).",
        "A-D: ordine diverso degli intervalli dell'arpa (unisono, quinta, ottava, sub-ottava).",
        "A blocchi corti, B blocchi lunghi, C e D aggiungono salti d'ottava.",
        "A-D: intervalli diversi dei burst; D aggiunge un glissando discendente.",
        "A accordo Maggiore, B minore 7, C quinte e ottave, D sus/9.",
        "A ritmo dritto, B terzine, C sezione aurea (code naturali), D puntato.",
        "A dritto, B terzine + glide verso l'alto, C sezione aurea, D puntato + glide."
    };

    //==========================================================================
    // Built-in guide pages ("# " = header, "@COL2" = switch to right column)
    //==========================================================================
    const char* guidePages[4] =
    {
        // 1. PANORAMICA & ROUTING
        "# COS'E' CHROMACOSM\n"
        "Un unico plugin che unisce due processori ispirati ai pedali\n"
        "Hologram Chroma Console (a sinistra) e Microcosm (a destra).\n"
        "\n"
        "# BARRA SUPERIORE\n"
        "PRESET e frecce < > : 12 preset di fabbrica pronti all'uso.\n"
        "ROUTING : l'ordine in cui lavorano i due pedali:\n"
        "- CHROMA -> MICROCOSM: il suono colorato entra nelle nuvole granulari.\n"
        "- MICROCOSM -> CHROMA: i grani passano in nastro, fuzz e filtri.\n"
        "- PARALLEL BLEND: i due pedali lavorano in parallelo e si sommano.\n"
        "INFO MOUSE : attiva/disattiva le descrizioni al passaggio del mouse.\n"
        "? GUIDA : apre questa guida.\n"
        "BPM e TAP : il tempo arriva dalla DAW; premi TAP a ritmo per forzarlo.\n"
        "IN / OUT : meter di ingresso e uscita. MASTER: volume finale.\n"
        "@COL2\n"
        "# FLUSSO DEL SEGNALE\n"
        "Ingresso -> Calibrazione -> Chroma Console (4 moduli nell'ordine scelto)\n"
        "-> Microcosm (grani -> filtro -> Space) -> Looper -> Master.\n"
        "\n"
        "# PRIMI PASSI (3 MOSSE)\n"
        "1. Scegli un preset dal menu in alto.\n"
        "2. Sul Chroma clicca un effetto nella finestrella di ogni colonna.\n"
        "3. Sul Microcosm clicca uno degli 11 effetti e una VAR A/B/C/D.\n"
        "Poi usa i due MIX per dosare il suono originale.\n"
        "\n"
        "# SICUREZZA AUDIO\n"
        "Filtri stabili, protezione anti-NaN e soft-clipper finale:\n"
        "puoi portare qualsiasi pomello al massimo senza blocchi o silenzi.\n",

        // 2. CHROMA CONSOLE
        "# CALIBRAZIONE E ORDINE\n"
        "CAL LOW/MED/HIGH/SYNTH: guadagno d'ingresso (chitarra -> synth/linea).\n"
        "Menu ordine: la sequenza in cui lavorano i 4 moduli colorati.\n"
        "\n"
        "# CHARACTER (ROSSO) - TILT/SENS, AMOUNT, EFFECT VOL\n"
        "DRIVE: saturazione valvolare. SWEETEN: preamp e compressione.\n"
        "FUZZ: fuzz aggressivo. HOWL: distorsione risonante che 'urla'.\n"
        "SWELL: volume automatico (TILT = sensibilita', AMOUNT = attacco).\n"
        "TILT: a sinistra scuro, al centro piatto, a destra brillante.\n"
        "\n"
        "# MOVEMENT (GIALLO) - RATE, AMOUNT, DRIFT\n"
        "DOUBLER: raddoppio stereo. VIBRATO: oscillazione di pitch.\n"
        "PHASER: 6 stadi. TREMOLO: volume pulsante stereo.\n"
        "PITCH: RATE sceglie l'intervallo (-12, -7, -5, detune, +5, +7, +12).\n"
        "DRIFT: instabilita' analogica casuale.\n"
        "@COL2\n"
        "# DIFFUSION (VERDE) - TIME, AMOUNT, DRIFT\n"
        "CASCADE: delay multi-tap. REELS: eco a nastro con wow/flutter.\n"
        "SPACE: riverbero ambient. COLLAGE: delay granulare con ottave.\n"
        "REVERSE: eco al contrario. AMOUNT = quantita' e feedback.\n"
        "\n"
        "# TEXTURE (BLU) - TILT/FREQ, AMOUNT, EFFECT VOL\n"
        "FILTER: sinistra passa-basso, centro aperto, destra passa-alto.\n"
        "SQUASH: compressione 'pompata'. CASSETTE: nastro usurato e fruscio.\n"
        "BROKEN: bitcrush e micro-dropout. INTERFERENCE: radio e statica.\n"
        "\n"
        "# COMANDI DEI MODULI\n"
        "Clicca un nome nella finestrella per attivarlo; BYPASS/OFF lo esclude.\n"
        "Tasti A/B/C/D: scorrono ciclicamente gli effetti del modulo.\n"
        "CHROMA MIX: dry/wet. OUTPUT LVL: volume d'uscita del Chroma.\n"
        "CAPTURE: congela un frammento in loop. BYPASS: esclude il Chroma.\n",

        // 3. MICROCOSM
        "# GLI 11 EFFETTI\n"
        "MICRO LOOP: MOSAIC (loop a 0.5x/1x/2x), SEQ (sequenza a 8 step),\n"
        "GLIDE (loop con glissando).\n"
        "GRANULES: HAZE (nuvola densa), TUNNEL (droni risonanti),\n"
        "STRUM (cascata ad arpa sugli attacchi delle note).\n"
        "GLITCH: BLOCKS (stutter ritmici), INTERRUPT (burst che abbassano il dry),\n"
        "ARP (arpeggi: A Maggiore, B min7, C quinte, D sus9).\n"
        "MULTI-DELAY: PATTERN (A dritto, B terzine, C aurea, D puntato),\n"
        "WARP (multi-tap modulato e 'piegato').\n"
        "\n"
        "VAR A/B/C/D: 4 ricette per ogni effetto = 44 algoritmi.\n"
        "Passa il mouse su una VAR per sapere cosa cambia nell'effetto attivo.\n"
        "@COL2\n"
        "# POMELLI\n"
        "ACTIVITY: densita' dei grani. REPEATS: feedback / durata.\n"
        "SHAPE: inviluppo dei grani (percussivo -> morbido -> pad).\n"
        "FILTER: passa-basso <-> passa-alto. FILTER RES: risonanza.\n"
        "TIME e SUBDIV: durata ritmica; con SYNC segue il BPM.\n"
        "SPACE: riverbero. SPACE I-IV: Room, Hall, Cathedral, Shimmer.\n"
        "MOD RATE / MOD DEPTH: modulazione di pitch (chorus) sui grani.\n"
        "MIX: dry/wet del Microcosm. REV: grani al contrario.\n"
        "HOLD SAMPLER: congela il buffer e ci suoni sopra all'infinito.\n"
        "\n"
        "# VISUALIZZATORE\n"
        "La linea e' la forma d'onda in uscita; i cerchi sono i grani attivi:\n"
        "giallo = piu' acuto, rosso = piu' grave, verde acqua = originale.\n",

        // 4. LOOPER & CONSIGLI
        "# PHRASE LOOPER (FINO A 40 SECONDI STEREO)\n"
        "REC/PLAY: 1o clic registra, 2o suona, 3o sovraincide (overdub).\n"
        "STOP: ferma. CLEAR: cancella il loop. LOOP LEVEL: volume del loop.\n"
        "SPD 0.5x/1x/2x: velocita' (0.5x = ottava sotto). REV LOOP: al contrario.\n"
        "PRE-FX attivo: registra il segnale pulito, che poi passa negli effetti.\n"
        "PRE-FX spento: registra il suono gia' processato.\n"
        "\n"
        "# DAW E AUTOMAZIONE\n"
        "Tutti i parametri sono automatizzabili in Ableton o in qualsiasi DAW.\n"
        "Il tempo segue il BPM della DAW (TAP lo sovrascrive).\n"
        "Lo stato di INFO MOUSE viene salvato con il progetto.\n"
        "@COL2\n"
        "# RICETTE VELOCI\n"
        "Pad ambient: SWEETEN + REELS + CASSETTE -> HAZE B, SPACE III.\n"
        "Shoegaze: ordine DIFF -> CHAR, SPACE + FUZZ -> TUNNEL, SPACE IV.\n"
        "Lo-fi beat: ROUTING Microcosm -> Chroma, SEQ D -> CASSETTE + SQUASH.\n"
        "Glitch: BLOCKS C + BROKEN + INTERFERENCE, MIX al 55%.\n"
        "\n"
        "# CONSIGLI\n"
        "- Suono troppo denso? Abbassa REPEATS o ACTIVITY.\n"
        "- Usa CAL SYNTH per synth e drum machine a livello linea.\n"
        "- Con INFO MOUSE attivo leggi l'aiuto rapido nella barra in basso.\n"
    };
}

//==============================================================================
// GUIDE OVERLAY PAINT
//==============================================================================
void GuideOverlayComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xE00B0B0F));

    auto area = getLocalBounds().reduced (60, 42).toFloat();
    g.setColour (juce::Colour (0xFF1A1B22));
    g.fillRoundedRectangle (area, 16.0f);
    g.setColour (kYellow.withAlpha (0.6f));
    g.drawRoundedRectangle (area, 16.0f, 1.5f);

    const juce::Colour stripes[4] = { kRed, kYellow, kGreen, kBlue };
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (stripes[i]);
        g.fillRect (area.getX() + 18.0f + i * 7.0f, area.getY() + 14.0f, 5.0f, 30.0f);
    }

    g.setColour (juce::Colour (0xFFF5F2EB));
    g.setFont (uiFont (21.0f, true));
    g.drawText ("GUIDA CHROMACOSM", static_cast<int> (area.getX()) + 56, static_cast<int> (area.getY()) + 12,
                420, 34, juce::Justification::centredLeft);

    const int contentTop = static_cast<int> (area.getY()) + 104;
    const int colGap = 32;
    const int colW = (static_cast<int> (area.getWidth()) - 48 - colGap) / 2;
    const int colX[2] = { static_cast<int> (area.getX()) + 24, static_cast<int> (area.getX()) + 24 + colW + colGap };

    const juce::Colour accent = stripes[juce::jlimit (0, 3, activeTab)];

    g.setColour (juce::Colour (0xFF30313C));
    g.drawVerticalLine (colX[1] - colGap / 2, static_cast<float> (contentTop), area.getBottom() - 24.0f);

    auto lines = juce::StringArray::fromLines (guidePages[juce::jlimit (0, 3, activeTab)]);
    int col = 0;
    int y = contentTop;

    for (const auto& line : lines)
    {
        if (line == "@COL2")
        {
            col = 1;
            y = contentTop;
            continue;
        }

        if (line.startsWith ("# "))
        {
            y += 4;
            g.setColour (accent);
            g.setFont (uiFont (14.5f, true));
            g.drawText (line.substring (2), colX[col], y, colW, 20, juce::Justification::centredLeft);
            y += 24;
        }
        else if (line.trim().isEmpty())
        {
            y += 10;
        }
        else
        {
            g.setColour (juce::Colour (0xFFD8D6CF));
            g.setFont (uiFont (13.0f));
            g.drawFittedText (line, colX[col], y, colW, 18, juce::Justification::centredLeft, 1, 0.8f);
            y += 19;
        }
    }
}

//==============================================================================
// EDITOR
//==============================================================================
ChromaCosmAudioProcessorEditor::ChromaCosmAudioProcessorEditor (ChromaCosmAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);
    tooltipWindow = std::make_unique<juce::TooltipWindow> (this, 650);

    tooltipsEnabled = static_cast<bool> (audioProcessor.apvts.state.getProperty ("showTooltips", true));

    //==========================================================================
    // TOP HEADER BAR
    //==========================================================================
    const auto& presets = audioProcessor.getPresets();
    for (int i = 0; i < static_cast<int> (presets.size()); ++i)
        presetBox.addItem (presets[i].name, i + 1);
    presetBox.setSelectedId (audioProcessor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0)
            audioProcessor.loadPreset (idx);
    };
    addAndMakeVisible (presetBox);
    registerControlDescription (presetBox, "PRESET",
        "12 preset di fabbrica che combinano Chroma Console e Microcosm. Caricarne uno sovrascrive le impostazioni attuali.");

    prevPresetBtn.onClick = [this]
    {
        int count = presetBox.getNumItems();
        int cur = presetBox.getSelectedId();
        presetBox.setSelectedId (cur > 1 ? cur - 1 : count, juce::sendNotification);
    };
    nextPresetBtn.onClick = [this]
    {
        int count = presetBox.getNumItems();
        int cur = presetBox.getSelectedId();
        presetBox.setSelectedId (cur < count ? cur + 1 : 1, juce::sendNotification);
    };
    addAndMakeVisible (prevPresetBtn);
    addAndMakeVisible (nextPresetBtn);
    registerControlDescription (prevPresetBtn, "PRESET PRECEDENTE", "Carica il preset precedente della lista.");
    registerControlDescription (nextPresetBtn, "PRESET SUCCESSIVO", "Carica il preset successivo della lista.");

    comboRoutingBox.addItemList ({ "CHROMA -> MICROCOSM", "MICROCOSM -> CHROMA", "PARALLEL BLEND" }, 1);
    addAndMakeVisible (comboRoutingBox);
    comboAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, "comboRouting", comboRoutingBox));
    registerControlDescription (comboRoutingBox, "COMBO ROUTING",
        "Ordine dei due pedali. Chroma -> Microcosm: il suono colorato entra nei grani. Microcosm -> Chroma: i grani passano in nastro/fuzz/filtri. Parallel: lavorano in parallelo.");

    infoTooltipsBtn.setToggleState (tooltipsEnabled, juce::dontSendNotification);
    infoTooltipsBtn.onClick = [this]
    {
        tooltipsEnabled = infoTooltipsBtn.getToggleState();
        audioProcessor.apvts.state.setProperty ("showTooltips", tooltipsEnabled, nullptr);
        applyTooltipState();
        repaint();
    };
    addAndMakeVisible (infoTooltipsBtn);
    registerControlDescription (infoTooltipsBtn, "INFO MOUSE",
        "Attiva o disattiva le descrizioni che compaiono passando il mouse sui comandi (e nella barra in basso). L'impostazione viene salvata con il progetto.");

    openGuideBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2D82C6));
    openGuideBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    openGuideBtn.onClick = [this]
    {
        guideOverlay.setVisible (true);
        guideOverlay.toFront (true);
    };
    addAndMakeVisible (openGuideBtn);
    registerControlDescription (openGuideBtn, "GUIDA", "Apre la guida integrata del plugin, divisa in 4 sezioni.");

    tapTempoBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2B2B33));
    tapTempoBtn.onClick = [this] { audioProcessor.triggerTapTempo(); };
    addAndMakeVisible (tapTempoBtn);
    registerControlDescription (tapTempoBtn, "TAP TEMPO",
        "Premi a ritmo (almeno 2 volte) per impostare il tempo a mano. Sovrascrive il BPM della DAW.");

    setupKnob (masterOutKnob, "masterOutput", kYellow,
        "Volume finale di tutto il plugin, dopo entrambi i pedali e il limitatore di sicurezza.");

    //==========================================================================
    // CHROMA CONSOLE CONTROLS (LEFT PEDAL)
    //==========================================================================
    chromaOrderBox.addItemList ({
        "CHAR -> MOVE -> DIFF -> TEXT",
        "TEXT -> CHAR -> MOVE -> DIFF",
        "DIFF -> CHAR -> MOVE -> TEXT",
        "MOVE -> DIFF -> CHAR -> TEXT",
        "CHAR -> DIFF -> MOVE -> TEXT",
        "TEXT -> DIFF -> MOVE -> CHAR"
    }, 1);
    addAndMakeVisible (chromaOrderBox);
    comboAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, "chromaOrder", chromaOrderBox));
    registerControlDescription (chromaOrderBox, "ORDINE DEI MODULI",
        "Sceglie in che sequenza lavorano i 4 moduli. Esempio: DIFF -> CHAR manda riverbero/delay DENTRO la distorsione (stile shoegaze).");

    chromaCalibBox.addItemList ({ "CAL: LOW", "CAL: MED", "CAL: HIGH", "CAL: SYNTH" }, 1);
    addAndMakeVisible (chromaCalibBox);
    comboAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, "chromaCalib", chromaCalibBox));
    registerControlDescription (chromaCalibBox, "CALIBRAZIONE INGRESSO",
        "Adatta il guadagno d'ingresso alla sorgente: LOW per segnali forti, MED standard, HIGH per chitarre deboli, SYNTH per synth/drum machine.");

    setupKnob (charTiltKnob, "charTilt", kRed,
        "TILT EQ attorno a ~950 Hz: a sinistra scuro e caldo, al centro piatto, a destra brillante. Su HOWL sposta il picco risonante, su SWELL e' la sensibilita'.");
    setupKnob (charAmtKnob, "charAmount", kRed,
        "Intensita' dell'effetto Character: quantita' di drive/fuzz/compressione. Su SWELL regola la lentezza dell'attacco.");
    setupKnob (charVolKnob, "charVol", kRed,
        "EFFECT VOL: volume in uscita dal modulo Character. Ore 3 circa (75%) = guadagno unitario. Utile per compensare la distorsione.");

    setupKnob (moveRateKnob, "moveRate", kYellow,
        "RATE: velocita' della modulazione. Su DOUBLER regola la distanza del raddoppio, su PITCH sceglie l'intervallo (-12 ... +12 semitoni).");
    setupKnob (moveAmtKnob, "moveAmount", kYellow,
        "Profondita'/quantita' della modulazione (vibrato, phaser, tremolo) o del segnale trasposto su PITCH.");
    setupKnob (moveDriftKnob, "moveDrift", kYellow,
        "DRIFT: aggiunge instabilita' analogica casuale alla modulazione, come un nastro o un circuito vintage che 'respira'.");

    setupKnob (diffTimeKnob, "diffTime", kGreen,
        "TIME: tempo di ritardo dei delay o grandezza della stanza su SPACE.");
    setupKnob (diffAmtKnob, "diffAmount", kGreen,
        "Quantita' dell'effetto Diffusion e numero di ripetizioni (feedback). Al massimo le code diventano molto lunghe.");
    setupKnob (diffDriftKnob, "diffDrift", kGreen,
        "DRIFT: wow & flutter del nastro e variazioni casuali del tempo di ritardo. Su SPACE rende il riverbero piu' luminoso.");

    setupKnob (textTiltKnob, "textTilt", kBlue,
        "TILT/FREQ: su FILTER sinistra = passa-basso, centro = aperto, destra = passa-alto. Su SQUASH = rilascio, CASSETTE = usura, BROKEN = clock, INTERFERENCE = sintonia.");
    setupKnob (textAmtKnob, "textAmount", kBlue,
        "Intensita' della texture lo-fi. Su FILTER aumenta anche la risonanza.");
    setupKnob (textVolKnob, "textVol", kBlue,
        "EFFECT VOL: volume in uscita dal modulo Texture (75% = guadagno unitario).");

    auto setupCycleButton = [this] (juce::TextButton& btn, const juce::String& paramID, juce::Colour col, const juce::String& moduleName)
    {
        btn.setColour (juce::TextButton::buttonColourId, col.darker (0.25f));
        btn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        btn.onClick = [this, paramID]
        {
            if (auto* prm = dynamic_cast<juce::AudioParameterChoice*> (audioProcessor.apvts.getParameter (paramID)))
            {
                int nextIdx = (prm->getIndex() + 1) % prm->choices.size();
                prm->setValueNotifyingHost (prm->convertTo0to1 (static_cast<float> (nextIdx)));
                repaint();
            }
        };
        addAndMakeVisible (btn);
        registerControlDescription (btn, "TASTO " + btn.getButtonText() + " - " + moduleName,
            "Scorre ciclicamente gli effetti del modulo " + moduleName + " (come il tasto del pedale originale). Puoi anche cliccare direttamente sui nomi nella finestrella.");
    };
    setupCycleButton (charCycleBtn, "charType", kRed,    "CHARACTER");
    setupCycleButton (moveCycleBtn, "moveType", kYellow, "MOVEMENT");
    setupCycleButton (diffCycleBtn, "diffType", kGreen,  "DIFFUSION");
    setupCycleButton (textCycleBtn, "textType", kBlue,   "TEXTURE");

    setupKnob (chromaMixKnob, "chromaMix", juce::Colour (0xFF25252B),
        "Bilanciamento dry/wet di tutto il Chroma Console: a sinistra suono originale, a destra solo effetto.");
    setupKnob (chromaLevelKnob, "chromaLevel", juce::Colour (0xFF25252B),
        "Volume d'uscita del Chroma Console (75% = guadagno unitario).");

    addAndMakeVisible (chromaCaptureBtn);
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, "chromaCapture", chromaCaptureBtn));
    registerControlDescription (chromaCaptureBtn, "CAPTURE",
        "Cattura l'ultimo frammento audio (mezzo battito) e lo ripete in loop sfumato, mentre continui a suonare sopra.");

    addAndMakeVisible (chromaBypassBtn);
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, "chromaBypass", chromaBypassBtn));
    registerControlDescription (chromaBypassBtn, "BYPASS CHROMA", "Esclude completamente il Chroma Console.");

    //==========================================================================
    // MICROCOSM CONTROLS (RIGHT PEDAL)
    //==========================================================================
    const juce::Colour colMicro (0xFF3E8E9E);
    const juce::Colour colCoral (0xFFE26D5C);

    setupKnob (microActivityKnob, "microActivity", colCoral,
        "ACTIVITY: densita' e frequenza con cui nascono grani e micro-loop. A zero l'effetto si ferma, al massimo diventa una nuvola fittissima.");
    setupKnob (microShapeKnob, "microShape", colMicro,
        "SHAPE: inviluppo dei grani (sinistra percussivo, centro morbido, destra pad lento) e leggero movimento del filtro.");
    setupKnob (microFilterKnob, "microFilter", colMicro,
        "FILTER: filtro sui grani. Da sinistra a circa 2/3 passa-basso (scurisce), oltre passa-alto (alleggerisce).");
    setupKnob (microMixKnob, "microMix", colCoral,
        "MIX: bilanciamento tra suono che entra (dry) e grani/delay del Microcosm (wet).");
    setupKnob (microTimeKnob, "microTime", colMicro,
        "TIME: durata di base di loop e delay quando SYNC e' spento. Con SYNC acceso il tempo segue il BPM e SUBDIV.");
    setupKnob (microRepeatsKnob, "microRepeats", colCoral,
        "REPEATS: quanto i grani vengono rimandati nel buffer (feedback). Piu' alto = code piu' lunghe ed evolutive.");
    setupKnob (microSpaceKnob, "microSpace", colMicro,
        "SPACE: quantita' del riverbero integrato. Il tipo si sceglie con il menu SPACE I-IV.");
    setupKnob (loopLevelKnob, "loopLevel", juce::Colour (0xFF439A68),
        "LOOP LEVEL: volume di riproduzione del Phrase Looper.");
    setupKnob (microResKnob, "microFilterRes", colMicro,
        "FILTER RES: risonanza del filtro del Microcosm (picco attorno alla frequenza di FILTER).");
    setupKnob (microPitchRateKnob, "microPitchRate", juce::Colour (0xFFD89A2B),
        "MOD RATE: velocita' della modulazione di pitch applicata ai grani.");
    setupKnob (microPitchDepthKnob, "microPitchDepth", juce::Colour (0xFFD89A2B),
        "MOD DEPTH: profondita' della modulazione di pitch (da leggero chorus a vibrato marcato).");

    microSubdivBox.addItemList ({ "1/4", "1/2", "1x", "2x", "4x", "TRIP" }, 1);
    addAndMakeVisible (microSubdivBox);
    comboAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, "microSubdiv", microSubdivBox));
    registerControlDescription (microSubdivBox, "SUBDIV",
        "Moltiplicatore ritmico del tempo dei grani: 1/4 e 1/2 piu' veloci, 2x e 4x piu' lenti, TRIP = terzine.");

    microSpaceModeBox.addItemList ({ "SPACE I: ROOM", "SPACE II: HALL", "SPACE III: CATH", "SPACE IV: SHIMMER" }, 1);
    addAndMakeVisible (microSpaceModeBox);
    comboAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, "microSpaceMode", microSpaceModeBox));
    registerControlDescription (microSpaceModeBox, "SPACE MODE",
        "Tipo di riverbero: I stanza, II hall, III cattedrale, IV shimmer (coda scintillante e infinita).");

    addAndMakeVisible (microSyncBtn);
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, "microSync", microSyncBtn));
    registerControlDescription (microSyncBtn, "SYNC", "Acceso: il tempo dei grani segue il BPM (DAW o TAP). Spento: usa il pomello TIME.");

    addAndMakeVisible (microRevBtn);
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, "microReverse", microRevBtn));
    registerControlDescription (microRevBtn, "REVERSE", "Riproduce tutti i grani e i micro-loop al contrario.");

    addAndMakeVisible (microHoldBtn);
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, "microHold", microHoldBtn));
    registerControlDescription (microHoldBtn, "HOLD SAMPLER",
        "Congela il buffer dei grani: l'effetto continua a suonare all'infinito quello che hai appena suonato, e tu puoi suonarci sopra.");

    addAndMakeVisible (microFxOffBtn);
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, "microFxOff", microFxOffBtn));
    registerControlDescription (microFxOffBtn, "LOOPER ONLY",
        "Disattiva la generazione dei grani (come nel video tutorial ufficiale Hologram): puoi usare il Microcosm come un looper pulito con Space Reverb, chorus e filtro analogico.");

    addAndMakeVisible (microBypassBtn);
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, "microBypass", microBypassBtn));
    registerControlDescription (microBypassBtn, "BYPASS MICROCOSM", "Esclude il Microcosm (il Looper continua a funzionare).");

    // Phrase Looper Transport
    looperRecPlayBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFFC83E3B));
    looperRecPlayBtn.onClick = [this] { audioProcessor.getMicrocosmEngine().triggerLooperRecordOrPlay(); };
    addAndMakeVisible (looperRecPlayBtn);
    registerControlDescription (looperRecPlayBtn, "REC / PLAY / DUB",
        "1o clic: registra. 2o clic: chiude il loop e lo suona. Clic successivi: alterna sovraincisione (overdub) e riproduzione.");

    looperStopBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF33333D));
    looperStopBtn.onClick = [this] { audioProcessor.getMicrocosmEngine().triggerLooperStop(); };
    addAndMakeVisible (looperStopBtn);
    registerControlDescription (looperStopBtn, "STOP", "Ferma il looper senza cancellarlo. REC/PLAY lo fa ripartire.");

    looperClearBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF33333D));
    looperClearBtn.onClick = [this] { audioProcessor.getMicrocosmEngine().triggerLooperClear(); };
    addAndMakeVisible (looperClearBtn);
    registerControlDescription (looperClearBtn, "CLEAR", "Cancella il loop registrato.");

    looperSpeedBox.addItemList ({ "SPD: 0.5x", "SPD: 1.0x", "SPD: 2.0x" }, 1);
    addAndMakeVisible (looperSpeedBox);
    comboAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, "loopSpeed", looperSpeedBox));
    registerControlDescription (looperSpeedBox, "VELOCITA' LOOP", "0.5x = meta' velocita' (ottava sotto), 2x = doppia velocita' (ottava sopra).");

    addAndMakeVisible (looperRevBtn);
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, "loopReverse", looperRevBtn));
    registerControlDescription (looperRevBtn, "REV LOOP", "Riproduce il loop al contrario.");

    addAndMakeVisible (looperPreBtn);
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, "loopPreFX", looperPreBtn));
    registerControlDescription (looperPreBtn, "PRE-FX",
        "Acceso: il looper registra il suono pulito, che poi passa negli effetti del Microcosm. Spento: registra il suono gia' processato.");

    // Built-in guide overlay (hidden until '? GUIDA' is pressed)
    addChildComponent (guideOverlay);

    applyTooltipState();

    setSize (1280, 812);
    startTimerHz (30);
}

ChromaCosmAudioProcessorEditor::~ChromaCosmAudioProcessorEditor()
{
    stopTimer();
    tooltipWindow.reset();
    setLookAndFeel (nullptr);
}

void ChromaCosmAudioProcessorEditor::setupKnob (juce::Slider& s, const juce::String& paramID,
                                                juce::Colour accentColour, const juce::String& desc)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setColour (juce::Slider::rotarySliderFillColourId, accentColour);
    s.setPopupDisplayEnabled (true, true, this);
    s.setDoubleClickReturnValue (true, static_cast<double> (
        audioProcessor.apvts.getParameterRange (paramID).convertFrom0to1 (
            audioProcessor.apvts.getParameter (paramID)->getDefaultValue())));
    addAndMakeVisible (s);
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.apvts, paramID, s));

    juce::String title = paramID;
    if (auto* prm = audioProcessor.apvts.getParameter (paramID))
        title = prm->getName (64).toUpperCase();

    registerControlDescription (s, title, desc + "  (Doppio clic = valore di default)");
}

void ChromaCosmAudioProcessorEditor::registerControlDescription (juce::Component& comp, const juce::String& title,
                                                                 const juce::String& desc)
{
    compDescriptions.push_back ({ &comp, title, desc });
}

void ChromaCosmAudioProcessorEditor::applyTooltipState()
{
    for (auto& info : compDescriptions)
    {
        if (auto* client = dynamic_cast<juce::SettableTooltipClient*> (info.comp))
            client->setTooltip (tooltipsEnabled ? (info.title + "\n" + info.desc) : juce::String());
    }

    if (! tooltipsEnabled)
    {
        currentHoverTitle = "DESCRIZIONI DISATTIVATE";
        currentHoverText  = "Attiva 'INFO MOUSE' in alto per vedere la descrizione di ogni comando al passaggio del mouse, oppure apri '? GUIDA'.";
    }
    else
    {
        currentHoverTitle = "SUGGERIMENTO RAPIDO";
        currentHoverText  = "Passa il mouse su qualsiasi pomello, tasto o effetto per leggerne la descrizione qui, oppure clicca su '? GUIDA' in alto.";
    }
}

void ChromaCosmAudioProcessorEditor::updateHoverBannerFromPoint (juce::Point<int> pt)
{
    if (! getLocalBounds().contains (pt))
        return;

    for (const auto& info : compDescriptions)
    {
        if (info.comp != nullptr && info.comp->isVisible() && info.comp->getBounds().contains (pt))
        {
            currentHoverTitle = info.title;
            currentHoverText  = info.desc;
            return;
        }
    }

    for (const auto& box : interactiveBoxes)
    {
        if (box.hoverTitle.isNotEmpty() && box.bounds.contains (pt))
        {
            currentHoverTitle = box.hoverTitle;
            currentHoverText  = box.hoverDesc;
            return;
        }
    }
}

void ChromaCosmAudioProcessorEditor::timerCallback()
{
    const int lst = audioProcessor.getMicrocosmEngine().getLooperState();
    if (lst == ChromaCosm::MicrocosmEngine::Looper_Recording)
        looperRecPlayBtn.setButtonText ("REC (CLICK->PLAY)");
    else if (lst == ChromaCosm::MicrocosmEngine::Looper_Playing)
        looperRecPlayBtn.setButtonText ("PLAY (CLICK->DUB)");
    else if (lst == ChromaCosm::MicrocosmEngine::Looper_Overdubbing)
        looperRecPlayBtn.setButtonText ("OVERDUBBING");
    else
        looperRecPlayBtn.setButtonText ("REC / PLAY");

    if (tooltipsEnabled && ! guideOverlay.isVisible())
        updateHoverBannerFromPoint (getMouseXYRelative());

    repaint();
}

void ChromaCosmAudioProcessorEditor::mouseMove (const juce::MouseEvent& event)
{
    if (tooltipsEnabled && ! guideOverlay.isVisible())
        updateHoverBannerFromPoint (event.getEventRelativeTo (this).getPosition());
}

void ChromaCosmAudioProcessorEditor::mouseDown (const juce::MouseEvent& event)
{
    const auto pt = event.getPosition();
    for (const auto& box : interactiveBoxes)
    {
        if (box.paramID.isEmpty() || ! box.bounds.contains (pt))
            continue;

        if (auto* prm = dynamic_cast<juce::AudioParameterChoice*> (audioProcessor.apvts.getParameter (box.paramID)))
        {
            prm->setValueNotifyingHost (prm->convertTo0to1 (static_cast<float> (box.choiceIndex)));
            repaint();
            return;
        }
    }
}

void ChromaCosmAudioProcessorEditor::paint (juce::Graphics& g)
{
    interactiveBoxes.clear();

    g.fillAll (juce::Colour (0xFF141418));

    const juce::Colour stripeCols[4] = { kRed, kYellow, kGreen, kBlue };

    //==========================================================================
    // TOP HEADER BAR (0..54)
    //==========================================================================
    g.setColour (juce::Colour (0xFF1F1F26));
    g.fillRect (0, 0, getWidth(), 54);

    for (int i = 0; i < 4; ++i)
    {
        g.setColour (stripeCols[i]);
        g.fillRect (16 + i * 7, 10, 5, 34);
    }

    g.setColour (juce::Colour (0xFFF5F2EB));
    g.setFont (uiFont (18.0f, true));
    g.drawText ("DaMA // CHROMACOSM", 52, 8, 220, 22, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xFFA0A0B0));
    g.setFont (uiFont (10.5f));
    g.drawText ("COLOR CONSOLE + COSMIC GRANULAR", 52, 29, 220, 16, juce::Justification::centredLeft);

    // BPM readout
    const double bpm = audioProcessor.getEffectiveBpm();
    juce::Rectangle<int> bpmRect (962, 11, 74, 32);
    g.setColour (juce::Colour (0xFF2B2B35));
    g.fillRoundedRectangle (bpmRect.toFloat(), 5.0f);
    g.setColour (kYellow);
    g.setFont (uiFont (13.0f, true));
    g.drawText (juce::String (bpm, 1) + " BPM", bpmRect, juce::Justification::centred);
    interactiveBoxes.push_back ({ bpmRect, {}, -1, "TEMPO (BPM)",
        "Tempo usato per sincronizzare grani, loop e capture. Arriva dalla DAW, oppure dal tasto TAP." });

    // Input / Output VU Bars
    const float inLvl  = juce::jlimit (0.0f, 1.0f, audioProcessor.getInputLevel());
    const float outLvl = juce::jlimit (0.0f, 1.0f, audioProcessor.getOutputLevel());

    g.setColour (juce::Colour (0xFFAAAAB8));
    g.setFont (uiFont (10.5f, true));
    g.drawText ("IN",  1095, 11, 22, 14, juce::Justification::centredRight);
    g.drawText ("OUT", 1095, 27, 22, 14, juce::Justification::centredRight);

    g.setColour (juce::Colour (0xFF2A2A32));
    g.fillRoundedRectangle (1122.0f, 14.0f, 82.0f, 8.0f, 3.0f);
    g.fillRoundedRectangle (1122.0f, 30.0f, 82.0f, 8.0f, 3.0f);

    g.setColour (kGreen);
    g.fillRoundedRectangle (1122.0f, 14.0f, 82.0f * inLvl, 8.0f, 3.0f);
    g.setColour (kBlue);
    g.fillRoundedRectangle (1122.0f, 30.0f, 82.0f * outLvl, 8.0f, 3.0f);
    interactiveBoxes.push_back ({ { 1095, 8, 112, 36 }, {}, -1, "METER IN / OUT",
        "Livello di picco in ingresso (verde) e in uscita (blu)." });

    g.setColour (juce::Colour (0xFFCCCCD6));
    g.drawText ("MASTER", 1212, 40, 56, 12, juce::Justification::centred);

    //==========================================================================
    // LEFT PEDAL: CHROMA CONSOLE CHASSIS (x: 16..630, y: 64..746)
    //==========================================================================
    juce::Rectangle<float> chromaChassis (16.0f, 64.0f, 614.0f, 682.0f);
    g.setColour (juce::Colour (0xFF0A0A0D));
    g.fillRoundedRectangle (chromaChassis.translated (0.0f, 4.0f), 18.0f);

    juce::ColourGradient chromaGrad (juce::Colour (0xFFF4F0E6), 16.0f, 64.0f,
                                     juce::Colour (0xFFE3DDD0), 16.0f, 746.0f, false);
    g.setGradientFill (chromaGrad);
    g.fillRoundedRectangle (chromaChassis, 16.0f);
    g.setColour (juce::Colour (0xFFC8C0B0));
    g.drawRoundedRectangle (chromaChassis, 16.0f, 2.0f);

    g.setColour (juce::Colour (0xFF222226));
    g.setFont (uiFont (21.0f, true));
    g.drawText ("CHROMA ENGINE", 34, 74, 240, 26, juce::Justification::centredLeft);
    g.setFont (uiFont (11.0f, true));
    g.setColour (juce::Colour (0xFF666258));
    g.drawText ("MULTI-EFFECTOR // SIGNAL CHAIN:", 34, 98, 220, 16, juce::Justification::centredLeft);

    const int orderPreset = static_cast<int> (audioProcessor.apvts.getRawParameterValue ("chromaOrder")->load());
    const auto orderArr = ChromaCosm::ChromaEngine::getRoutingOrder (orderPreset);
    const char* modShort[4] = { "CHAR", "MOVE", "DIFF", "TEXT" };
    for (int s = 0; s < 4; ++s)
    {
        const int m = orderArr[s];
        const float bx = 34.0f + s * 54.0f;
        g.setColour (stripeCols[m]);
        g.fillRoundedRectangle (bx, 116.0f, 42.0f, 18.0f, 4.0f);
        g.setColour (juce::Colours::white);
        g.setFont (uiFont (10.0f, true));
        g.drawText (modShort[m], static_cast<int> (bx), 116, 42, 18, juce::Justification::centred);
        if (s < 3)
        {
            g.setColour (juce::Colour (0xFF44444A));
            g.drawText (">", static_cast<int> (bx + 42), 116, 12, 18, juce::Justification::centred);
        }
    }
    interactiveBoxes.push_back ({ { 34, 114, 210, 22 }, {}, -1, "CATENA DEL SEGNALE",
        "Mostra l'ordine attuale dei 4 moduli colorati. Si cambia con il menu ORDINE accanto al titolo." });

    juce::Rectangle<float> chromaBezel (30.0f, 142.0f, 586.0f, 470.0f);
    g.setColour (juce::Colour (0xFF19191E));
    g.fillRoundedRectangle (chromaBezel, 20.0f);

    const char* stripTitles[4]   = { "CHARACTER", "MOVEMENT", "DIFFUSION", "TEXTURE" };
    const char* topKnobLabels[4] = { "TILT / SENS", "RATE", "TIME", "TILT / FREQ" };
    const char* secKnobLabels[4] = { "EFFECT VOL", "DRIFT", "DRIFT", "EFFECT VOL" };
    const char* stripDescs[4] =
    {
        "Modulo ROSSO: definisce il carattere del suono, dal calore leggero alla distruzione (saturazioni, fuzz, swell).",
        "Modulo GIALLO: aggiunge movimento (raddoppio, vibrato, phaser, tremolo, pitch).",
        "Modulo VERDE: spazio e ripetizioni (delay, nastro, riverbero, collage, reverse).",
        "Modulo BLU: textures lo-fi e filtri (filtro, compressione, cassetta, circuito rotto, radio)."
    };
    const juce::String modeParamIDs[4] = { "charType", "moveType", "diffType", "textType" };

    for (int col = 0; col < 4; ++col)
    {
        const float sx = 42.0f + col * 142.0f;
        const float sy = 152.0f;
        const float sw = 136.0f;
        const float sh = 450.0f;

        g.setColour (stripeCols[col]);
        g.fillRoundedRectangle (sx, sy, sw, sh, 12.0f);

        g.setColour (juce::Colour (0x28000000));
        g.fillRoundedRectangle (sx + 4.0f, sy + 4.0f, sw - 8.0f, 24.0f, 8.0f);
        g.setColour (juce::Colours::white);
        g.setFont (uiFont (12.5f, true));
        g.drawText (stripTitles[col], static_cast<int> (sx), static_cast<int> (sy + 4), static_cast<int> (sw), 24, juce::Justification::centred);
        interactiveBoxes.push_back ({ { static_cast<int> (sx + 4), static_cast<int> (sy + 4), static_cast<int> (sw - 8), 24 },
                                      {}, -1, stripTitles[col], stripDescs[col] });

        g.setFont (uiFont (10.0f, true));
        g.setColour (juce::Colour (0xFFFDFBF7));
        g.drawText (topKnobLabels[col], static_cast<int> (sx), static_cast<int> (sy + 94),  static_cast<int> (sw), 13, juce::Justification::centred);
        g.drawText ("AMOUNT",           static_cast<int> (sx), static_cast<int> (sy + 172), static_cast<int> (sw), 13, juce::Justification::centred);
        g.drawText (secKnobLabels[col], static_cast<int> (sx), static_cast<int> (sy + 236), static_cast<int> (sw), 13, juce::Justification::centred);

        juce::Rectangle<int> listBox (static_cast<int> (sx + 8), static_cast<int> (sy + 254), static_cast<int> (sw - 16), 152);
        g.setColour (juce::Colour (0xFF18181D));
        g.fillRoundedRectangle (listBox.toFloat(), 8.0f);

        const int activeMode = static_cast<int> (audioProcessor.apvts.getRawParameterValue (modeParamIDs[col])->load());
        for (int m = 1; m <= 5; ++m)
        {
            const int rowY = listBox.getY() + 6 + (m - 1) * 24;
            juce::Rectangle<int> itemRect (listBox.getX() + 4, rowY, listBox.getWidth() - 8, 22);
            const bool isSelected = (activeMode == m);

            if (isSelected)
            {
                g.setColour (stripeCols[col]);
                g.fillRoundedRectangle (itemRect.toFloat(), 5.0f);
                g.setColour (juce::Colours::white);
            }
            else
            {
                g.setColour (juce::Colour (0xFFA5A5B2));
            }
            g.setFont (uiFont (11.5f, isSelected));
            g.drawText (chromaModeNames[col][m], itemRect, juce::Justification::centred);

            interactiveBoxes.push_back ({ itemRect, modeParamIDs[col], m,
                                          juce::String (stripTitles[col]) + " > " + chromaModeNames[col][m],
                                          chromaModeDescs[col][m] });
        }

        juce::Rectangle<int> offRect (listBox.getX() + 4, listBox.getBottom() - 22, listBox.getWidth() - 8, 18);
        const bool isOff = (activeMode == 0);
        g.setColour (isOff ? juce::Colour (0xFF555562) : juce::Colour (0xFF282830));
        g.fillRoundedRectangle (offRect.toFloat(), 4.0f);
        g.setColour (isOff ? juce::Colours::white : juce::Colour (0xFF777785));
        g.setFont (uiFont (9.5f, true));
        g.drawText ("BYPASS / OFF", offRect, juce::Justification::centred);
        interactiveBoxes.push_back ({ offRect, modeParamIDs[col], 0,
                                      juce::String (stripTitles[col]) + " > OFF",
                                      "Esclude solo questo modulo, lasciando attivi gli altri." });
    }

    g.setColour (juce::Colour (0xFF25252B));
    g.setFont (uiFont (11.0f, true));
    g.drawText ("CHROMA MIX", 40,  692, 90, 14, juce::Justification::centred);
    g.drawText ("OUTPUT LVL", 138, 692, 90, 14, juce::Justification::centred);

    //==========================================================================
    // RIGHT PEDAL: MICROCOSM CHASSIS (x: 644..1264, y: 64..746)
    //==========================================================================
    juce::Rectangle<float> microChassis (644.0f, 64.0f, 620.0f, 682.0f);
    g.setColour (juce::Colour (0xFF0A0A0D));
    g.fillRoundedRectangle (microChassis.translated (0.0f, 4.0f), 18.0f);

    juce::ColourGradient microGrad (juce::Colour (0xFFF4F3EF), 644.0f, 64.0f,
                                    juce::Colour (0xFFE2E0D8), 644.0f, 746.0f, false);
    g.setGradientFill (microGrad);
    g.fillRoundedRectangle (microChassis, 16.0f);
    g.setColour (juce::Colour (0xFFC5C2B8));
    g.drawRoundedRectangle (microChassis, 16.0f, 2.0f);

    {
        const juce::Colour waveCols[3] = { juce::Colour (0x33E88D72), juce::Colour (0x33F2C14E), juce::Colour (0x334EA8B8) };
        for (int w = 0; w < 3; ++w)
        {
            const float o = static_cast<float> (w) * 35.0f;
            juce::Path wave;
            wave.startNewSubPath (646.0f, 340.0f + o);
            wave.cubicTo (790.0f, 290.0f + o, 980.0f, 410.0f + o, 1262.0f, 330.0f + o);
            wave.lineTo (1262.0f, 365.0f + o);
            wave.cubicTo (980.0f, 445.0f + o, 790.0f, 325.0f + o, 646.0f, 375.0f + o);
            wave.closeSubPath();
            g.setColour (waveCols[w]);
            g.fillPath (wave);
        }
    }

    g.setColour (juce::Colour (0xFF222228));
    g.setFont (uiFont (21.0f, true));
    g.drawText ("COSMIC GRANULAR", 664, 74, 210, 26, juce::Justification::centredLeft);
    g.setFont (uiFont (11.0f, true));
    g.setColour (juce::Colour (0xFF66645E));
    g.drawText ("GRANULAR LOOPER & MULTIDELAY", 664, 98, 230, 16, juce::Justification::centredLeft);

    const int activeFX  = juce::jlimit (0, 10, static_cast<int> (audioProcessor.apvts.getRawParameterValue ("microEffect")->load()));
    const int activeVar = static_cast<int> (audioProcessor.apvts.getRawParameterValue ("microVar")->load());
    const char* varLabels[4] = { "VAR A", "VAR B", "VAR C", "VAR D" };
    for (int v = 0; v < 4; ++v)
    {
        juce::Rectangle<int> vRect (910 + v * 58, 78, 52, 26);
        const bool sel = (activeVar == v);
        g.setColour (sel ? juce::Colour (0xFFE05A47) : juce::Colour (0xFF2A2A32));
        g.fillRoundedRectangle (vRect.toFloat(), 6.0f);
        g.setColour (sel ? juce::Colours::white : juce::Colour (0xFFAAAAB5));
        g.setFont (uiFont (11.0f, true));
        g.drawText (varLabels[v], vRect, juce::Justification::centred);
        interactiveBoxes.push_back ({ vRect, "microVar", v,
                                      juce::String (varLabels[v]) + " (" + microFxNames[activeFX] + ")",
                                      juce::String ("Variazioni di ") + microFxNames[activeFX] + ": " + microVarDescs[activeFX] });
    }

    //==========================================================================
    // GRANULAR VISUALIZER
    //==========================================================================
    juce::Rectangle<float> visBox (662.0f, 116.0f, 584.0f, 88.0f);
    g.setColour (juce::Colour (0xFF17181E));
    g.fillRoundedRectangle (visBox, 10.0f);
    g.setColour (juce::Colour (0xFF343642));
    g.drawRoundedRectangle (visBox, 10.0f, 1.5f);
    interactiveBoxes.push_back ({ visBox.toNearestInt(), {}, -1, "VISUALIZZATORE GRANULARE",
        "Linea = forma d'onda in uscita. Cerchi = grani attivi (posizione nel buffer, pan, volume). Giallo = piu' acuto, rosso = piu' grave, verde acqua = intonazione originale." });

    std::array<float, 128> scope;
    audioProcessor.getScopeData (scope);
    juce::Path wavePath;
    for (int i = 0; i < 128; ++i)
    {
        const float wx = visBox.getX() + 8.0f + (static_cast<float> (i) / 127.0f) * (visBox.getWidth() - 16.0f);
        const float wy = visBox.getCentreY() - juce::jlimit (-1.0f, 1.0f, scope[i] * 1.6f) * 32.0f;
        if (i == 0) wavePath.startNewSubPath (wx, wy);
        else        wavePath.lineTo (wx, wy);
    }
    g.setColour (juce::Colour (0x664EA8B8));
    g.strokePath (wavePath, juce::PathStrokeType (1.8f));

    const auto grains = audioProcessor.getMicrocosmEngine().getGrainVisuals();
    for (int i = 0; i < ChromaCosm::MicrocosmEngine::kMaxGrains; ++i)
    {
        if (! grains[i].active || grains[i].env <= 0.01f)
            continue;

        const float gx = visBox.getX() + 16.0f + grains[i].posNorm * (visBox.getWidth() - 32.0f);
        const float gy = visBox.getY() + 14.0f + (1.0f - grains[i].pan) * (visBox.getHeight() - 28.0f);
        const float gr = 4.0f + grains[i].env * 14.0f;

        const juce::Colour gCol = (std::fabs (grains[i].pitch) > 1.3f) ? juce::Colour (0xFFF2B705)
                                : (std::fabs (grains[i].pitch) < 0.8f) ? kRed
                                : juce::Colour (0xFF45B8AC);
        g.setColour (gCol.withAlpha (juce::jlimit (0.15f, 0.88f, grains[i].env)));
        g.fillEllipse (gx - gr, gy - gr, gr * 2.0f, gr * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (grains[i].env * 0.7f));
        g.fillEllipse (gx - 2.5f, gy - 2.5f, 5.0f, 5.0f);
    }

    if (audioProcessor.apvts.getRawParameterValue ("microHold")->load() > 0.5f)
    {
        g.setColour (juce::Colour (0xFFE05A47));
        g.fillRoundedRectangle (visBox.getRight() - 108.0f, visBox.getY() + 8.0f, 98.0f, 20.0f, 5.0f);
        g.setColour (juce::Colours::white);
        g.setFont (uiFont (10.5f, true));
        g.drawText ("HOLD FROZEN", static_cast<int> (visBox.getRight() - 108), static_cast<int> (visBox.getY() + 8), 98, 20, juce::Justification::centred);
    }

    //==========================================================================
    // 11-EFFECT SELECTOR
    //==========================================================================
    const juce::Colour fxCatCols[11] = {
        juce::Colour (0xFFE05A47), juce::Colour (0xFFE05A47), juce::Colour (0xFFE05A47),
        juce::Colour (0xFFD99B26), juce::Colour (0xFFD99B26), juce::Colour (0xFFD99B26),
        juce::Colour (0xFF3E8E9E), juce::Colour (0xFF3E8E9E), juce::Colour (0xFF3E8E9E),
        juce::Colour (0xFF3B7A57), juce::Colour (0xFF3B7A57)
    };

    g.setFont (uiFont (9.5f, true));
    g.setColour (juce::Colour (0xFFC84B38));
    g.drawText ("MICRO LOOP",  662,  208, 156, 12, juce::Justification::centred);
    g.setColour (juce::Colour (0xFFB87F14));
    g.drawText ("GRANULES",    822,  208, 156, 12, juce::Justification::centred);
    g.setColour (juce::Colour (0xFF2B7382));
    g.drawText ("GLITCH",      982,  208, 156, 12, juce::Justification::centred);
    g.setColour (juce::Colour (0xFF2D6848));
    g.drawText ("MULTI-DELAY", 1142, 208, 104, 12, juce::Justification::centred);

    for (int f = 0; f < 11; ++f)
    {
        juce::Rectangle<int> fxRect (662 + f * 53, 222, 51, 28);
        const bool sel = (activeFX == f);

        g.setColour (sel ? fxCatCols[f] : juce::Colour (0xFF25262E));
        g.fillRoundedRectangle (fxRect.toFloat(), 6.0f);
        g.setColour (sel ? juce::Colours::white : juce::Colour (0xFFC8C8D2));
        g.setFont (uiFont (9.5f, sel));
        g.drawText (microFxNames[f], fxRect, juce::Justification::centred);

        interactiveBoxes.push_back ({ fxRect, "microEffect", f, microFxNames[f],
                                      juce::String (microFxDescs[f]) + "  Variazioni: " + microVarDescs[f] });
    }

    //==========================================================================
    // MICROCOSM KNOB LABELS
    //==========================================================================
    g.setColour (juce::Colour (0xFF222228));
    g.setFont (uiFont (11.5f, true));
    g.drawText ("ACTIVITY", 674,  344, 110, 15, juce::Justification::centred);
    g.drawText ("SHAPE",    818,  344, 110, 15, juce::Justification::centred);
    g.drawText ("FILTER",   962,  344, 110, 15, juce::Justification::centred);
    g.drawText ("MIX",      1106, 344, 110, 15, juce::Justification::centred);

    g.drawText ("TIME",       674,  458, 110, 15, juce::Justification::centred);
    g.drawText ("REPEATS",    818,  458, 110, 15, juce::Justification::centred);
    g.drawText ("SPACE",      962,  458, 110, 15, juce::Justification::centred);
    g.drawText ("LOOP LEVEL", 1106, 458, 110, 15, juce::Justification::centred);

    g.setFont (uiFont (10.0f, true));
    g.setColour (juce::Colour (0xFF44444E));
    g.drawText ("FILTER RES", 930,  548, 80, 14, juce::Justification::centred);
    g.drawText ("MOD RATE",   1035, 548, 80, 14, juce::Justification::centred);
    g.drawText ("MOD DEPTH",  1140, 548, 80, 14, juce::Justification::centred);

    //==========================================================================
    // PHRASE LOOPER DECK
    //==========================================================================
    juce::Rectangle<float> looperDeck (662.0f, 574.0f, 584.0f, 86.0f);
    g.setColour (juce::Colour (0xFF1D1E24));
    g.fillRoundedRectangle (looperDeck, 10.0f);

    g.setColour (juce::Colour (0xFFECEAE2));
    g.setFont (uiFont (11.5f, true));
    g.drawText ("PHRASE LOOPER (40s STEREO)", 676, 580, 220, 16, juce::Justification::centredLeft);

    const float loopProg = audioProcessor.getMicrocosmEngine().getLooperProgress();
    const int loopSt = audioProcessor.getMicrocosmEngine().getLooperState();
    g.setColour (juce::Colour (0xFF32343F));
    g.fillRoundedRectangle (900.0f, 583.0f, 332.0f, 10.0f, 4.0f);

    const juce::Colour progCol = (loopSt == ChromaCosm::MicrocosmEngine::Looper_Recording)   ? kRed
                               : (loopSt == ChromaCosm::MicrocosmEngine::Looper_Overdubbing) ? kYellow
                               : kGreen;
    g.setColour (progCol);
    g.fillRoundedRectangle (900.0f, 583.0f, 332.0f * loopProg, 10.0f, 4.0f);
    interactiveBoxes.push_back ({ { 900, 578, 332, 20 }, {}, -1, "AVANZAMENTO LOOP",
        "Posizione di riproduzione del loop. Rosso = registrazione, giallo = overdub, verde = riproduzione." });

    //==========================================================================
    // HOVER HELP BANNER (bottom)
    //==========================================================================
    juce::Rectangle<float> banner (16.0f, 756.0f, 1248.0f, 48.0f);
    g.setColour (juce::Colour (0xFF1C1D24));
    g.fillRoundedRectangle (banner, 10.0f);
    g.setColour (tooltipsEnabled ? kYellow.withAlpha (0.55f) : juce::Colour (0xFF3A3B46));
    g.drawRoundedRectangle (banner, 10.0f, 1.2f);

    g.setColour (tooltipsEnabled ? kYellow : juce::Colour (0xFF8A8A96));
    g.setFont (uiFont (12.5f, true));
    g.drawFittedText (currentHoverTitle, 30, 760, 230, 40, juce::Justification::centredLeft, 2, 0.8f);

    g.setColour (juce::Colour (0xFFD8D6CF));
    g.setFont (uiFont (12.5f));
    g.drawFittedText (currentHoverText, 270, 760, 980, 40, juce::Justification::centredLeft, 2, 0.85f);
}

void ChromaCosmAudioProcessorEditor::resized()
{
    // Top Header Bar
    prevPresetBtn.setBounds   (280, 12, 28, 30);
    presetBox.setBounds       (312, 12, 230, 30);
    nextPresetBtn.setBounds   (546, 12, 28, 30);
    comboRoutingBox.setBounds (582, 12, 190, 30);
    infoTooltipsBtn.setBounds (780, 12, 104, 30);
    openGuideBtn.setBounds    (888, 12, 68, 30);
    tapTempoBtn.setBounds     (1040, 11, 48, 32);
    masterOutKnob.setBounds   (1218, 2, 44, 40);

    // Chroma Console
    chromaOrderBox.setBounds (275, 76, 210, 26);
    chromaCalibBox.setBounds (492, 76, 120, 26);

    juce::Slider* topKnobs[4] = { &charTiltKnob, &moveRateKnob,  &diffTimeKnob,  &textTiltKnob };
    juce::Slider* midKnobs[4] = { &charAmtKnob,  &moveAmtKnob,   &diffAmtKnob,   &textAmtKnob };
    juce::Slider* secKnobs[4] = { &charVolKnob,  &moveDriftKnob, &diffDriftKnob, &textVolKnob };
    juce::TextButton* cycBtns[4] = { &charCycleBtn, &moveCycleBtn, &diffCycleBtn, &textCycleBtn };

    for (int col = 0; col < 4; ++col)
    {
        const int sx = 42 + col * 142;
        const int sy = 152;
        const int sw = 136;

        topKnobs[col]->setBounds (sx + (sw - 64) / 2, sy + 30,  64, 64);
        midKnobs[col]->setBounds (sx + (sw - 64) / 2, sy + 108, 64, 64);
        secKnobs[col]->setBounds (sx + (sw - 46) / 2, sy + 188, 46, 46);
        cycBtns[col]->setBounds  (sx + 18,            sy + 414, sw - 36, 28);
    }

    chromaMixKnob.setBounds    (53,  624, 66, 66);
    chromaLevelKnob.setBounds  (151, 624, 66, 66);
    chromaCaptureBtn.setBounds (260, 642, 150, 38);
    chromaBypassBtn.setBounds  (430, 642, 150, 38);

    // Microcosm
    microActivityKnob.setBounds (690,  264, 78, 78);
    microShapeKnob.setBounds    (834,  264, 78, 78);
    microFilterKnob.setBounds   (978,  264, 78, 78);
    microMixKnob.setBounds      (1122, 264, 78, 78);

    microTimeKnob.setBounds     (690,  378, 78, 78);
    microRepeatsKnob.setBounds  (834,  378, 78, 78);
    microSpaceKnob.setBounds    (978,  378, 78, 78);
    loopLevelKnob.setBounds     (1122, 378, 78, 78);

    microSubdivBox.setBounds    (664, 492, 95, 28);
    microSyncBtn.setBounds      (664, 524, 95, 24);
    microSpaceModeBox.setBounds (772, 492, 148, 28);
    microRevBtn.setBounds       (772, 524, 148, 24);

    microResKnob.setBounds        (944,  492, 54, 54);
    microPitchRateKnob.setBounds  (1049, 492, 54, 54);
    microPitchDepthKnob.setBounds (1154, 492, 54, 54);

    looperRecPlayBtn.setBounds (676,  608, 138, 40);
    looperStopBtn.setBounds    (820,  608, 68,  40);
    looperClearBtn.setBounds   (894,  608, 68,  40);
    looperSpeedBox.setBounds   (972,  612, 96,  32);
    looperRevBtn.setBounds     (1078, 612, 82,  32);
    looperPreBtn.setBounds     (1164, 612, 72,  32);

    microHoldBtn.setBounds   (676, 678, 175, 44);
    microFxOffBtn.setBounds  (858, 678, 150, 44);
    microBypassBtn.setBounds (1016, 678, 210, 44);

    guideOverlay.setBounds (getLocalBounds());
}
