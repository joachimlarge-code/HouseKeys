# HouseKeys 3.3

Et house keys-instrument for FL Studio på Mac (VST3 og AU, Apple Silicon og Intel).
Ekte samplet flygel, friske house-pianoer, Rhodes, pads, bass, plucks og synth-akkorder,
med innebygd Air, saturation, filter, chorus, delay, reverb og pump.

## Installere

1. Lukk FL Studio (**Cmd + Q**).
2. Åpne **Terminal**, lim inn denne linjen og trykk Enter:
   ```
   curl -fsSL https://raw.githubusercontent.com/joachimlarge-code/housekeys/main/install.sh | bash
   ```
3. Åpne FL Studio → **Options → Manage plugins** → huk av **Rescan previously verified plugins** → **Find installed plugins**.
4. Trykk **+** i Channel Rack og velg **HouseKeys**.

Samme kommando oppdaterer til nyeste versjon.

## Lyder

| Kategori | Lyder |
|---|---|
| **Real Piano** | Concert Grand · Intimate Felt · Ballad Grand · Octave Grand · Deep Night Piano |
| **House Piano** | Fresh House Piano · Big House Piano · Bright Piano · Supersaw Piano · Saturated Piano · Lo-fi House Piano · Piano House 90s · Sunset Piano · Pumping Chords |
| **Deep House Keys** | Deep House Keys · UK Garage Keys · Deep Wide Stab · Short Organ Stab |
| **Rhodes** | Warm Rhodes · Lo-fi Rhodes |
| **Supersaw & Chords** | Supersaw Chords · Euphoric Supersaw · Supersaw Stab · Melodic Chords · Rave Stab |
| **Synths** | Poly Synth · Analog Brass · Juno Strings · Hollow Synth |
| **Leads** | Fat Saw Lead · Saw Lead · Octave Saw Lead · Wide Saw Lead · Scream Lead · House Lead · Supersaw Lead (mono med glide) |
| **Pads** | Warm Pad · Air Pad · Dark Strings |
| **Bass** | Deep Sub · House Bass · Reese Bass · Organ Bass · Saw Bass · Acid Bass · Pluck Bass |
| **Plucks** | House Pluck · Bright Pluck · Supersaw Pluck · Mallet Pluck · Deep Pluck · Soft Pluck |
| **Ditt sample** | Dra en lydfil inn i pluginen (se under) |

Trykk **LYDER** (eller på navnet) for lydleseren: kategorier til venstre, lydene som store
knapper til høyre. Pilene blar til forrige/neste lyd. Når du bytter lyd, settes alle knapper
og effekter til den lydens egne innstillinger (Chord står alltid på Off). Lagrede prosjekter
beholder dine egne justeringer.

Alle lydene er nivåkalibrert (ca. −13,5 LUFS, pianoer og leads −14,5, bass −17), så volumet hopper ikke når du bytter.

## Hvordan lydene lages

- **Pianoene** bruker ekte samples: *Salamander Grand Piano* (Yamaha C5, 48 kHz/24-bit).
  30 toner (hver lille ters) × 4 anslagsnivåer + egne damper-/release-lyder per tangent.
  Ett lag spilles om gangen (ingen faset blanding mellom opptak), og lagene er nivåtilpasset,
  så anslaget styrer volumet jevnt mens lagene gir klangen. De øverste tangentene klinger
  udempet som på et ekte flygel, og sustain-pedal fungerer.
- **Rhodes/Wurli/bjeller** er FM-syntese med «bark» på hardt anslag.
- **UK house keys** er additiv syntese tilpasset lydeksempler.
- **Big House Piano** stabler pianoet med seg selv en oktav opp, slik klassiske house-pianoer gjør.
- **Pads, bass, plucks, leads og supersaws** er virtuell analog: anti-aliased sagtann/firkant/trekant/sinus
  med opptil 7 unison-stemmer, sub-oscillator, 4-polet filter med egen konvolutt og langsom
  analog bevegelse. Bass og leads er mono med legato (ingen klikk mellom toner), og leadene har glide.

## Kontroller

**Sound**
- **Tone**: mørk ↔ lys
- **Air**: luft og detaljer i toppen. Lager nye, myke overtoner over ~5 kHz og løfter den aller
  øverste toppen, men holdes alltid under selve lyden, så den blir aldri skarp.
- **Attack / Decay / Release**: hvor raskt lyden starter, hvor lenge den klinger, hvor lenge den henger igjen
- **Width**: stereobredde (bass under 120 Hz holdes alltid mono)
- **Volume**
- **Chord**: én tangent gir en hel akkord (Min7, Min9, Min11, Maj7, Maj9, 7sus4)

**Effekter** (hver med ON/OFF): Filter · Saturation · Chorus · Delay (følger tempoet) · Reverb · Pump (sidechain i takt)

**Vibrato** (egen seksjon med Rate og Depth). Mod-hjulet på keyboardet gir også vibrato.

**Automation:** høyreklikk på en knapp. Da merkes den som «sist vridd» i FL Studio, og menyen viser veien:
**Tools → Last tweaked → Create automation clip** (eller **Add → Automation from last tweaked parameter**).
FL Studio lar ikke plugins lage clipet selv, så dette er den raskeste veien.

## Ditt sample (dra og slipp)

Dra en lydfil (wav, aiff, flac, ogg, mp3) fra Finder eller FL-browseren rett inn i pluginen.
Den fjerner stillhet, jevner ut nivået og analyserer lyden:

- **Én tone:** grunntonen finnes og finstemmes til riktig cent, så den stemmer med resten av låta.
- **Akkord / stab:** pluginen ser at samplet selv er en akkord og spiller det som **chord stab**:
  én tangent om gangen, transponert slik at laveste tone i akkorden havner på tangenten du spiller.
  Da blir det ikke grøt når du spiller flere toner.
- **Trommer / støy:** spilles fra C4.

**Modus** (vises når Ditt sample er valgt): *Auto* velger selv, *Poly* spiller alle toner samtidig,
*Chord stab* spiller én om gangen. Samplet lagres i FL-prosjektet.

## Bruke egne pianosamples

Pluginen leser først denne mappen, og bruker de innebygde samplene hvis den er tom:

```
~/Library/Application Support/HouseKeys/Piano/
```

Navngiving (wav, flac, ogg eller aiff, mono eller stereo, hvilken som helst samplerate):

| Fil | Betyr |
|---|---|
| `C4v1.wav` | tonen C4 (midt-C), anslagslag 1 (svakest) |
| `C4v4.wav` | tonen C4, anslagslag 4 (hardest) |
| `F#3v2.wav` | F#3, lag 2 (bruk `#`, ikke `b`) |
| `rel40.wav` | valgfri release-lyd for tangent 40 (1 = A0, 88 = C8) |

- Du trenger ikke alle tonene. Hver tangent bruker nærmeste innspilte tone (minst hver lille ters anbefales).
- Lagene nummereres 1, 2, 3 … uten hull. Like mange lag per tone er best.
- Klipp bort stillhet før anslaget. Pluginen nivåtilpasser lagene selv.
- Start FL Studio på nytt etter at du har lagt inn eller fjernet samples. Nederst til venstre i pluginen står det hvilke samples som er lastet.

## Bygge selv (for utviklere)

GitHub Actions bygger Mac-versjonen ved hver push til `main` og legger den ut som release «latest».

Lokalt på Mac (Xcode Command Line Tools + CMake):
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j 8
```

Render-verktøy som spiller alle lydene gjennom motoren til en WAV-fil:
```
cmake -B build -DHK_BUILD_TOOLS=ON && cmake --build build --target hk_render
./build/hk_render_artefacts/Release/hk_render demo.wav
```

## Filer

- `Source/Sounds.h`: alle lydene og standardinnstillingene deres
- `Source/Engine.h`: stemmer, sampler, synth og effektkjede
- `Source/Dsp.h`: filtre, reverb, Air, saturation
- `Source/SampleBank.h`, `Source/SampleLoader.*`: sample-banken og lasting (deles mellom alle instanser)
- `Source/PluginProcessor.*`, `Source/PluginEditor.*`: FL Studio-kobling og grensesnitt
- `Resources/Piano/`: pianosamplene (se lisens der)
- `tools/prepare_piano_samples.py`: hvordan samplene ble laget fra Salamander-originalene

## Lisenser og kreditering

- Piano: **Salamander Grand Piano v3 av Alexander Holm**, CC-BY 3.0 (`Resources/Piano/`)
- Laget med [JUCE](https://juce.com) (AGPLv3 eller JUCE-lisens, gratis for personlig bruk)
