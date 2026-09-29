#!/usr/bin/env python3
"""Synthesize Ularn's sound effects at build time: one <event>.wav per SOUND("event")
in the game (src/*.c, asserted) into <out>, plus <out>/sounds.json {event: [file]}.
Ularn has no sounds of its own (none upstream), so these are made for it: lower,
triangle-wave tones for the deeper, demon-haunted Larn. Stdlib only.
Usage (repo root): python3 web/mksounds.py <out>"""
import glob, json, math, os, random, re, struct, sys, wave

R = 22050
rnd = random.Random(1987)

def tone(f0, f1, dur, vol=.45, dec=2.0, w='tri'):
    out, ph, n = [], 0.0, int(R * dur)
    for i in range(n):
        t = i / n
        ph += f0 * (f1 / f0) ** t / R
        x = 4 * abs(ph % 1 - .5) - 1 if w == 'tri' else math.sin(2 * math.pi * ph)
        out.append(x * vol * (1 - t) ** dec)
    return out

def noise(dur, vol=.5, dec=2.0, lp=.3, swell=False):
    out, y, n = [], 0.0, int(R * dur)
    for i in range(n):
        t = i / n
        y += lp * (rnd.uniform(-1, 1) - y)          # one-pole low-pass: small lp = duller
        out.append(y * vol * (math.sin(math.pi * t) if swell else (1 - t) ** dec))
    return out

def mix(*parts):
    out = [0.0] * max(map(len, parts))
    for p in parts:
        for i, x in enumerate(p):
            out[i] += x
    return out

def notes(fs, d=.07, **k):
    return sum((tone(f, f, d, **k) for f in fs), [])

SOUNDS = {
    'hit':         lambda: mix(noise(.1, .8, 3, .25), tone(160, 55, .1, .4)),
    'miss':        lambda: noise(.2, .35, lp=.45, swell=True),
    'kill':        lambda: mix(noise(.15, .5, 2, .15), tone(240, 40, .4, .45, 1)),
    'mon_hit':     lambda: mix(noise(.15, .9, 2, .08), tone(100, 35, .15, .45)),
    'money1':      lambda: notes([880, 1175], .06, vol=.3, dec=.6) + tone(1175, 1175, .18, .3),
    'level':       lambda: notes([440, 554, 659], .09, vol=.35, dec=.3) + tone(880, 880, .35, .35, 1.5),
    'cast_spell':  lambda: mix(notes([659, 831, 988, 1319], .05, vol=.25, dec=.4), tone(330, 660, .2, .15, 1, 'sin')) + tone(1319, 494, .25, .2, 1.5, 'sin'),
    'opendoor':    lambda: tone(90, 140, .25, .35, .8) + noise(.05, .5, 4, .15),
    'shutdoor':    lambda: mix(noise(.1, .9, 4, .1), tone(80, 50, .12, .5, 2)),
    'pickup':      lambda: noise(.05, .4, 3, .4) + tone(660, 880, .06, .2, 1),
    'stairs_up':   lambda: notes([330, 370, 415, 440], .07, vol=.3, dec=.5),
    'stairs_down': lambda: notes([440, 415, 370, 330], .07, vol=.3, dec=.5),
    'store5':      lambda: tone(1319, 1319, .08, .3, 1) + tone(1760, 1760, .4, .3, 2, 'sin'),
    'death':       lambda: notes([294, 277, 262], .25, vol=.35, dec=.3) + tone(247, 98, 1.0, .35, 1.2),
}

events = set()
for f in glob.glob('src/*.c'):
    events |= set(re.findall(r'SOUND\s*\("(\w+)"', open(f, encoding='latin-1').read()))
assert events and events <= set(SOUNDS), 'events without a sound: %s' % sorted(events - set(SOUNDS))

out = sys.argv[1]
os.makedirs(out, exist_ok=True)
for ev in sorted(events):
    s = SOUNDS[ev]()
    with wave.open(os.path.join(out, ev + '.wav'), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(R)
        w.writeframes(b''.join(struct.pack('<h', int(max(-1, min(1, x)) * 32000)) for x in s))
json.dump({ev: [ev + '.wav'] for ev in sorted(events)}, open(os.path.join(out, 'sounds.json'), 'w'))
