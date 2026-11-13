# DaMA ChromaCosm — Developer Kit Guide (Guida Sviluppatore)

Benvenuto nel **Dev Kit ufficiale di DaMA ChromaCosm**!
Questa guida e pensata per permettere a te e a qualsiasi sviluppatore o sound designer di esplorare, compilare, debuggare e aggiungere nuovi algoritmi o funzionalita al plugin in pochi minuti.

---

## 🚀 1. Prerequisiti & Setup Rapido (Windows)

Non serve installare configurazioni complesse o pacchetti a pagamento:
1. **Compilatore C++**: [Visual Studio 2022 Community](https://visualstudio.microsoft.com/vs/community/) con il carico di lavoro *"Sviluppo di applicazioni desktop con C++"* installato.
2. **CMake**: Versione 3.22 o superiore ([cmake.org/download](https://cmake.org/download/)).
3. **Framework JUCE**:
   - **Opzione A (Consigliata per portabilita)**: Se non hai JUCE installato, **non fare nulla!** Il nostro script CMake scarichera automaticamente la versione stabile di JUCE 7 via Git (`FetchContent`).
   - **Opzione B**: Se hai gia JUCE sul tuo disco (es. in `C:/JUCE/JUCE`), verra rilevato e utilizzato istantaneamente.

---

## ⚡ 2. Compilazione con Un Solo Clic

Nella cartella `scripts/` trovi utility pronte:

- **Doppio clic su `scripts/build_windows.bat`**:
  Configura la soluzione CMake, genera la soluzione Visual Studio a 64-bit e compila in parallelo VST3 e Standalone in modalita `Release`.
- **Da riga di comando (PowerShell)**:
  ```powershell
  # Compila in Release
  .\scripts\build_windows.ps1

  # Oppure compila in Debug per il debug con breakpoint
  .\scripts\build_windows.ps1 -Config Debug
  ```
- **Se vuoi aprire il progetto in Visual Studio**:
  Apri il file `build\ChromaCosm.sln` direttamente con Visual Studio 2022.

---

## 📁 3. Dove trovare i binari compilati

Dopo la compilazione, i file si trovano in:
- **Plugin VST3**: `build/ChromaCosm_artefacts/Release/VST3/ChromaCosm.vst3`
- **Applicazione Standalone (.exe)**: `build/ChromaCosm_artefacts/Release/Standalone/ChromaCosm.exe`

Per testarlo nella tua DAW su Windows, copia la cartella `ChromaCosm.vst3` in:
`C:\Program Files\Common Files\VST3\DaMA\`

---

## 🛠️ 4. Guida Passo-Passo: Come aggiungere un Nuovo Algoritmo

Tutti i 4 moduli di colorazione risiedono in `Source/ChromaEngine.h`. Aggiungere un nuovo algoritmo e semplicissimo.

### Esempio: Aggiungere "TAPE SATURATOR" al modulo CHARACTER

#### Passo 1: Aggiungi l'enum in `Source/ChromaEngine.h`
```cpp
enum CharacterType {
    Char_Off = 0,
    Char_Drive,
    Char_Sweeten,
    Char_Fuzz,
    Char_Howl,
    Char_Swell,
    Char_TapeSat // <-- IL TUO NUOVO ALGORITMO!
};
```

#### Passo 2: Implementa la formula DSP in `processCharacter()`
In `Source/ChromaEngine.h`, cerca la funzione `processCharacter`:
```cpp
case Char_TapeSat:
{
    // Esempio di curva di saturazione a nastro morbida asimmetrica
    float driveGain = 1.0f + amt * 4.5f;
    float inL = l * driveGain;
    float inR = r * driveGain;
    
    // Asimmetria valvolare e compressione magnetica
    outL = std::tanh (inL + 0.15f * inL * inL) - 0.10f;
    outR = std::tanh (inR + 0.15f * inR * inR) - 0.10f;
    break;
}
```

#### Passo 3: Esponi il nuovo algoritmo all'APVTS (`Source/PluginProcessor.cpp`)
Nel metodo `createParameterLayout()`:
```cpp
params.push_back (std::make_unique<juce::AudioParameterChoice> (
    "charType", "Character Mode",
    juce::StringArray { "OFF", "DRIVE", "SWEETEN", "FUZZ", "HOWL", "SWELL", "TAPE SAT" }, 1));
```

#### Passo 4: Aggiungi il pulsante/LED sulla GUI (`Source/PluginEditor.cpp`)
Nella lista `algoNames` della strip Character in `paint()`:
```cpp
{ "OFF", "DRIVE", "SWEETEN", "FUZZ", "HOWL", "SWELL", "TAPE SAT" }
```
Fatto! Al prossimo avvio il nuovo algoritmo sara attivo, selezionabile e automatizzabile nella DAW.

---

## 🔍 5. Debugging Avanzato

Per fare il debug con breakpoint all'interno del thread audio:
1. Compila in modalita `Debug`:
   ```powershell
   .\scripts\build_windows.ps1 -Config Debug
   ```
2. Apri `build\ChromaCosm.sln` in Visual Studio.
3. Imposta `ChromaCosm_Standalone` come progetto di avvio e premi `F5`.
4. Oppure, per debuggare il plugin VST3 all'interno della tua DAW preferita (Ableton, Reaper):
   - Vai nelle proprieta del progetto `ChromaCosm_VST3` &rarr; *Debug* &rarr; *Comando*.
   - Inserisci il percorso dell'eseguibile della DAW (es. `C:\Program Files\Ableton\Live 12 Suite\Program\Ableton Live 12 Suite.exe`).
   - Premi `F5`: Visual Studio avviera la DAW e aggancera tutti i breakpoint C++ in tempo reale.

---

## 📜 Regole d'Oro per l'Audio Real-Time
1. **Zero allocazioni di memoria** durante `processBlock`: mai usare `new`, `std::vector::resize()` o `std::string` nel ciclo audio.
2. **Usa sempre `sanitizeAudio()`** su qualsiasi anello di retroazione (feedback) per proteggere il sistema da `NaN` o `+/-Inf`.
3. Mantieni i filtri sempre stabili usando la classe `StableSVF` inclusa.
