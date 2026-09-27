#!/usr/bin/env python3
"""Copy the Dubtrain samples for the sound events Ularn raises (SOUND() in
src/*.c, grep for it) into <out> and write <out>/sounds.json {event: [files]}.
Same as ~/Games/larn/web/sounds.py; web/ularn.js plays them via rvip-sound.js."""
import json, os, shutil, sys
PACK = os.path.expanduser('~/Downloads/Dubtrain Angband Sound Pack v3.1.0')
EVENTS = ['hit', 'miss', 'kill', 'mon_hit', 'money1', 'level', 'cast_spell', 'opendoor', 'shutdoor',
          'stairs_up', 'stairs_down', 'store5', 'death']
EXTRA = {'pickup': ['plm_chest_latch.wav']}   # no Dubtrain event: a close sample
out = sys.argv[1]
cfg = {}
for line in open(os.path.join(PACK, 'sound.cfg'), encoding='latin-1'):
    if '=' in line and not line.lstrip().startswith('#'):
        k, v = line.split('=', 1)
        cfg[k.strip()] = v.split()
os.makedirs(out, exist_ok=True)
used = {e: cfg.get(e, []) for e in EVENTS}
used.update(EXTRA)
# DASP's 'miss' is a bow sample; Ularn's miss is a melee swing (RVIP-Finetuning, Sound)
used['miss'] = ['plc_miss_swish.wav']
for files in used.values():
    for f in files:
        shutil.copy(os.path.join(PACK, f), out)
json.dump(used, open(os.path.join(out, 'sounds.json'), 'w'))
