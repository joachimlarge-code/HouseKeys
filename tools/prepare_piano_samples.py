#!/usr/bin/env python3
"""
Prepares Salamander Grand Piano v3 samples (CC-BY 3.0, Alexander Holm) for HouseKeys.

Input : a folder with the original Salamander FLAC files, e.g.
        https://github.com/sfzinstruments/SalamanderGrandPiano  (folder "Samples")
Output: Resources/Piano/<Note><Octave>v<layer>.ogg  and  rel<key>.ogg

  - keeps 4 of the 16 velocity layers (v4, v8, v12, v16 -> layer 1..4)
  - removes the silence before the hammer hits (keeps 1 ms pre-roll)
  - cuts the tail once it has decayed ~60 dB (with a max length per register) and fades out
  - encodes to Ogg Vorbis q7 (48 kHz stereo)

Usage: python3 tools/prepare_piano_samples.py <salamander Samples dir> <output dir>
"""
import os, sys, subprocess, tempfile
import numpy as np, soundfile as sf

SRC, DST = sys.argv[1], sys.argv[2]
os.makedirs(DST, exist_ok=True)
NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
LAYERS = [4, 8, 12, 16]

def midi(name):
    n, octv = name[:-1], int(name[-1])
    return NAMES.index(n) + (octv + 1) * 12

def env_db(x, sr, win=0.02):
    m = np.abs(x).max(axis=1)
    hop = int(win * sr)
    frames = len(m) // hop
    e = np.sqrt((x[:frames * hop] ** 2).mean(axis=1).reshape(frames, hop).mean(axis=1) + 1e-20)
    return 20 * np.log10(e), hop

def encode(x, sr, path):
    with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as t:
        tmp = t.name
    sf.write(tmp, x.astype(np.float32), sr, subtype='FLOAT')
    subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-i', tmp, '-c:a', 'libvorbis', '-q:a', '7', path], check=True)
    os.remove(tmp)

def process(x, sr, max_len, floor_db, fade_s):
    if x.ndim == 1:
        x = np.stack([x, x], 1)
    peak = np.abs(x).max()
    onset = int(np.argmax(np.abs(x).max(axis=1) > peak * 10 ** (-40 / 20)))
    x = x[max(0, onset - int(0.001 * sr)):]
    e, hop = env_db(x, sr)
    above = np.where(e > e.max() + floor_db)[0]
    end = (above[-1] + 1) * hop if len(above) else len(x)
    end = min(end, int(max_len * sr), len(x))
    x = x[:end].copy()
    f = min(int(fade_s * sr), len(x) // 2)
    x[-f:] *= (0.5 + 0.5 * np.cos(np.linspace(0, np.pi, f)))[:, None]
    return x

notes = sorted({f[:-len('v16.flac')] for f in os.listdir(SRC) if f.endswith('v16.flac')}, key=midi)
total = 0.0
for n in notes:
    m = midi(n)
    max_len = 8.0 if m < 48 else (6.5 if m < 72 else (5.0 if m < 88 else 3.5))
    for li, v in enumerate(LAYERS, start=1):
        x, sr = sf.read(os.path.join(SRC, f'{n}v{v}.flac'))
        y = process(x, sr, max_len, -60.0, 0.35)
        encode(y, sr, os.path.join(DST, f'{n}v{li}.ogg'))
        total += len(y) / sr
    print(f'{n:4s} (midi {m:3d})  {len(y)/sr:5.1f} s')

for k in range(1, 89):
    p = os.path.join(SRC, f'rel{k}.flac')
    if not os.path.exists(p):
        continue
    x, sr = sf.read(p)
    y = process(x, sr, 1.5, -50.0, 0.1)
    encode(y, sr, os.path.join(DST, f'rel{k}.ogg'))
print(f'total sustain audio: {total:.0f} s')
