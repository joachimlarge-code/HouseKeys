# HouseKeys 2

Piano-plugin for FL Studio på Mac (VST3 og AU, Apple Silicon og Intel), med 12 pianoer for house og rolig musikk.

## Installere (enklest)

1. Lukk FL Studio.
2. Åpne **Terminal**, lim inn denne linjen og trykk Enter:
   ```
   curl -fsSL https://raw.githubusercontent.com/joachimlarge-code/housekeys/main/install.sh | bash
   ```
3. Åpne FL Studio → **Options → Manage plugins** → huk av **Rescan plugins with errors** → **Find installed plugins**.
4. Trykk **+** i Channel Rack og velg **HouseKeys**.

Samme kommando oppdaterer også til nyeste versjon.

## Pianoer

| Energisk | | Rolig | |
|---|---|---|---|
| Classic House | 90s house-piano | Grand Piano | Naturlig flygel |
| Rave Stab | Kort og hard | Soft Felt | Mykt og intimt |
| Garage Bounce | Punchy med bite | Deep Rhodes | Varm el-piano |
| Big Room Grand | Stort og lyst | Wurli Dream | Drømmende reed-piano |
| Disco Upright | Honky-tonk groove | Lo-fi Keys | Støvete tape-piano |
| Party Organ | Garage-orgel | Glass Bells | Klokker og glitter |

## Kontroller

**Sound** (hovedknappene)
- **Tone**: mørk ↔ lys
- **Decay**: hvor lenge tonene klinger
- **Width**: hvor bred lyden er i stereo
- **Volume**: utgangsnivå
- **Chord**: én tangent gir en hel akkord (Min7, Min9, Min11, Maj7, Maj9, 7sus4)

**Effekter**: hver seksjon har en egen **ON/OFF**-bryter.
- **Filter**: Cutoff og Reso
- **Chorus**: Mix
- **Delay**: Time (følger tempoet), Feedback og Mix
- **Reverb**: Size og Mix
- **Pump**: sidechain-effekt i takt med FL Studio, med Rate og Depth

Sustain-pedal fungerer. Dobbeltklikk på en knapp for å sette den tilbake til standard.

## Bygge selv (for utviklere)

Hver gang noe lastes opp til `main`, bygger GitHub Actions Mac-versjonen og legger den ut som release «latest».

For å bygge lokalt på Mac (krever Xcode Command Line Tools og CMake):
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j 8
```

Lyddemo uten FL Studio:
```
g++ -O2 -std=c++17 tools/render_demo.cpp -o render_demo && ./render_demo demo.wav
```

## Filer

- `Source/Engine.h`: lydmotoren og alle pianoene. Juster eller legg til pianoer i `pianos()`.
- `Source/PluginProcessor.*`: parametere, MIDI og lagring
- `Source/PluginEditor.*`: grensesnittet
- `install.sh`: installasjonsskriptet

Laget med [JUCE](https://juce.com) (lisens: AGPLv3 eller JUCE-lisens, gratis for personlig bruk).
