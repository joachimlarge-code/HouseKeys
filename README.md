# HouseKeys

FM house-piano for FL Studio på Mac (VST3 og AU, Apple Silicon og Intel).
Lyden lages med FM-syntese, så det er ingen samples å installere.

## Lyder (presets)

| Preset | Beskrivelse |
|---|---|
| 90s House Piano | Lyst, stabbete klassisk house-piano |
| Deep House Keys | Mykt, Rhodes-aktig med bjelleaktig anslag |
| Garage Organ | Orgel som holder tonen så lenge du holder tangenten |
| Lo-fi Piano | Tape-wobble, sus og mørkere topp |
| Min7 Stab | Kort og hard, spiller en moll 7-akkord fra én tangent |
| Pumping Min9 | Moll 9-akkorder som pumper i takt med prosjektet |
| Dark Filtered | Mørk, filtrert moll 11 for intro og breakdown |

## Kontroller

- **Model**: grunnlyden (House Piano, Deep Keys, Organ, Stab)
- **Chord**: én tangent gir en hel akkord (Min7, Min9, Min11, Maj7, Maj9, 7sus4). Sett til Off for å spille vanlig.
- **Sound**: Bright (hvor lys/skarp), Layer (oktav-/bjellelaget), Detune (bredde), Tape (lo-fi)
- **Envelope**: Attack, Decay (hvor lenge tonen klinger), Release, Velocity (hvor mye anslaget betyr)
- **Filter**: Cutoff og Reso. Automatiser Cutoff for klassiske filter-sweeps.
- **Effects**: Chorus, Reverb, Size (romstørrelse), Comp (kompressor)
- **Pump**: sidechain-effekt i takt med FL Studio. Depth = hvor mye, Rate = 1/4, 1/8 eller 1/16.
- **Volume**: utgangsnivå

Sustain-pedal (CC64) fungerer. Dobbeltklikk på en knapp for å sette den tilbake.

---

## Bygge plugin-en

Du kan velge mellom å bygge på din egen Mac (A) eller la GitHub bygge den for deg (B).

### A) Bygg på din egen Mac

1. Åpne **Terminal** og installer Apples utviklerverktøy:
   ```
   xcode-select --install
   ```
2. Installer CMake. Med Homebrew:
   ```
   brew install cmake
   ```
   (Uten Homebrew: last ned fra cmake.org, og velg «Tools → How to Install For Command Line Use» i appen.)
3. Gå til mappen der du pakket ut HouseKeys og bygg:
   ```
   cd ~/Downloads/HouseKeys
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release -j 8
   ```
   Første gang lastes JUCE ned automatisk, så det tar noen minutter.

Når bygget er ferdig, legges plugin-en automatisk her:
- `~/Library/Audio/Plug-Ins/VST3/HouseKeys.vst3`
- `~/Library/Audio/Plug-Ins/Components/HouseKeys.component` (AU)

Hvis bygget klager på manglende Xcode, installer full **Xcode** fra App Store og kjør `sudo xcode-select -s /Applications/Xcode.app` før du prøver igjen.

### B) La GitHub bygge den (ingen verktøy på Macen)

1. Lag et nytt repo på github.com og last opp hele HouseKeys-mappen, inkludert den skjulte `.github`-mappen (trykk Cmd+Shift+. i Finder for å vise skjulte mapper).
2. Gå til fanen **Actions**. Bygget «Build Mac» starter av seg selv.
3. Når det er grønt, last ned **HouseKeys-mac** nederst på siden.
4. Pakk ut og flytt `HouseKeys.vst3` til `~/Library/Audio/Plug-Ins/VST3/`.
5. Siden filen er lastet ned fra nettet, må du fjerne karantenen i Terminal:
   ```
   xattr -cr ~/Library/Audio/Plug-Ins/VST3/HouseKeys.vst3
   ```

---

## Bruke den i FL Studio

1. Åpne FL Studio → **Options → Manage plugins**.
2. Trykk **Find installed plugins** (skann). HouseKeys dukker opp under Generators.
3. Huk av stjernen ved HouseKeys for å legge den i favorittlisten.
4. Legg den til i Channel Rack med **+** og velg HouseKeys.

Tips: Pump-effekten følger tempoet i FL Studio, så den sitter alltid i takt med kicken.

## Demo uten FL Studio

`tools/render_demo.cpp` spiller alle presetene til en WAV-fil uten JUCE:
```
g++ -O2 -std=c++17 tools/render_demo.cpp -o render_demo && ./render_demo demo.wav
```

## Filer

- `Source/Engine.h`: lydmotoren (FM-stemmer, akkorder, filter, effekter)
- `Source/Presets.h`: presetene. Legg til egne her.
- `Source/PluginProcessor.*`: kobling mot FL Studio (parametere, MIDI, lagring)
- `Source/PluginEditor.*`: grensesnittet

Laget med [JUCE](https://juce.com) (lisens: AGPLv3 eller JUCE-lisens, gratis for personlig bruk).
