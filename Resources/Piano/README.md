# Piano samples

These samples are from **Salamander Grand Piano v3** by **Alexander Holm**
(Yamaha C5, recorded at 48 kHz / 24 bit with two AKG C414 in AB).

- Original: https://archive.org/details/SalamanderGrandPianoV3
- SFZ remap used as source: https://github.com/sfzinstruments/SalamanderGrandPiano
- License: Creative Commons Attribution 3.0 Unported (see LICENSE-CC-BY-3.0.txt)

Changes made for HouseKeys (tools/prepare_piano_samples.py): 4 of the 16 velocity layers
(v4, v8, v12, v16), leading silence removed, tails shortened and faded, encoded as Ogg Vorbis.

## File naming

- `<Note><Octave>v<layer>.ogg`  sustain sample, e.g. `A4v3.ogg` (layer 1 = softest)
- `rel<key>.ogg`                key-release noise, key 1 = A0 ... 88 = C8

The same naming works for your own piano: see "Bruke egne pianosamples" in the main README.
