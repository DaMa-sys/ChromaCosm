# DaMA ChromaCosm — Architettura Software & DSP

Documento di riferimento per sviluppatori che descrive l'architettura interna di **ChromaCosm (Chroma Console Edition)**.

```mermaid
flowchart TD
    DAW[DAW Audio Input / Mic] --> SanitizeIn[Sanitize Audio & Level Meter]
    SanitizeIn --> Calib[Input Calibration Gain]
    
    subgraph RoutingMatrix [Routing Matrix: 8 / 16 Combinazioni Libere]
        Calib --> Slot1[Slot 1: Modulo A / B / C / D]
        Slot1 --> Slot2[Slot 2: Modulo A / B / C / D]
        Slot2 --> Slot3[Slot 3: Modulo A / B / C / D]
        Slot3 --> Slot4[Slot 4: Modulo A / B / C / D]
    end

    subgraph FourEngines [I 4 Motori DSP (ChromaEngine)]
        M_Char["[A] CHARACTER<br/>Drive, Sweeten, Fuzz, Howl, Swell"]
        M_Move["[B] MOVEMENT<br/>Doubler, Vibrato, Phaser, Tremolo, Pitch<br/>+ Pink/Perlin Drift"]
        M_Diff["[C] DIFFUSION<br/>Cascade, Reels Tape, Space Reverb, Collage, Reverse"]
        M_Text["[D] TEXTURE<br/>Cytomic TPT SVF, Optical Squash, Cassette, Broken, Interference"]
    end

    Slot4 --> CaptureLooper["Footswitch CAPTURE (Infinite Micro-Loop / Sustainer Drone)"]
    CaptureLooper --> GlobalTilt["Global Console Tilt EQ (Shelving +/- 6dB)"]
    GlobalTilt --> Limiter["Soft Console Transformer Saturation (tanh)"]
    Limiter --> WetDryMixer["Master Dry / Wet Blender"]
    WetDryMixer --> DAWOut[DAW Stereo Output / Bus]
```

---

## 1. Struttura dei File

```
ChromaCosm/
├── CMakeLists.txt                # Configurazione di build CMake con auto-fetch JUCE 7
├── README.md                     # Documentazione principale utente
├── DEV_GUIDE.md                  # Manuale completo di sviluppo ed estensione
├── ARCHITECTURE.md               # Questo documento
├── scripts/
│   ├── build_windows.bat         # Build script doppio-clic per Visual Studio 2022
│   ├── build_windows.ps1         # Script PowerShell configurabile (Debug/Release)
│   └── clean.bat                 # Pulizia della cartella di build
├── Source/
│   ├── ChromaEngine.h            # Motore DSP a 4 moduli, filtri TPT e saturazione
│   ├── PluginProcessor.h         # Gestione audio, parametri APVTS, BPM e preset
│   ├── PluginProcessor.cpp       # Implementazione del processore e pipeline audio
│   ├── PluginEditor.h            # Interfaccia grafica LookAndFeel personalizzata
│   └── PluginEditor.cpp          # Implementazione GUI, controlli e manuale integrato
└── docs/
    ├── index.html                # Showcase Web per GitHub Pages
    └── download/                 # Archivio pacchetti di distribuzione e Dev Kit
```

---

## 2. Principi di Thread-Safety e Performance Real-Time

Nel callback `processBlock(AudioBuffer<float>& buffer, MidiBuffer& midi)` sono applicate regole ferree:
1. **Nessuna allocazione dinamica di memoria**: `std::vector`, stringhe o allocazioni con `malloc`/`new` sono categoricamente vietate nel thread audio. Tutti i buffer di delay (`diffDelayL`, `moveDelayL`, `revBufL`, `captureBufL`) sono pre-allocati in `prepare()`.
2. **Protezione anti-denormali**: `juce::ScopedNoDenormals noDenormals;` è sempre attivo all'inizio di `processBlock`.
3. **Filtri unconditionally stable**: Non usiamo il classico filtro di Chamberlin a differenza finita (che diverge a valori estremi di frequenza o risonanza), ma l'algoritmo **Cytomic TPT State-Variable Filter**, matematicamente immune al blow-up.
4. **Sanificazione continua del segnale**: La funzione `sanitizeAudio(x)` protegge tutti gli anelli di feedback da `NaN`, `+/-Inf` o valori subnormali.
5. **Comunicazione Thread Audio &rarr; Thread UI**: Avviene esclusivamente tramite variabili atomiche `std::atomic<float>` (per i misuratori VU e lo snapshot oscilloscopio a 128 punti `scopeRing`).

---

## 3. Gestione Parametri (APVTS)

Tutti i controlli del plugin sono governati da un unico oggetto centrale `juce::AudioProcessorValueTreeState apvts`:
- **Preset Factory**: 12 preset salvati come strutture `PresetData` applicati mediante `setValueNotifyingHost` per mantenere DAW e GUI sempre in perfetto sincronismo.
- **Automazione DAW**: Tutti i parametri sono esportati con identificatori stabili (`charTilt`, `charAmount`, `moveRate`, `diffTime`, ecc.) permettendo l'automazione continua in qualsiasi DAW (Ableton, FL Studio, Logic, Reaper).
