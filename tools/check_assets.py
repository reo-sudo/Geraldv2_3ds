#!/usr/bin/env python3
"""Standard-library preflight checks used before handing builds to the ARM compiler."""
from pathlib import Path
import re,struct,wave
root=Path(__file__).resolve().parent.parent
code=(root/'source/main.c').read_text()
section=code.split('static const char *FROG[FH] = {')[1].split('};',1)[0]
rows=re.findall(r'"([.a-z]+)"',section)
assert len(rows)==19 and all(len(s)==24 for s in rows), 'Frog pixel map must be 24 x 19'
for name,size in [('icon.png',(48,48)),('banner.png',(256,128))]:
    data=(root/'meta'/name).read_bytes()
    assert data[:8]==b'\x89PNG\r\n\x1a\n',f'{name} is not PNG'
    w,h=struct.unpack('>II',data[16:24]);assert (w,h)==size,(name,w,h)
with wave.open(str(root/'meta/banner.wav')) as wav:
    assert wav.getnchannels()==1 and wav.getsampwidth()==2
    assert wav.getnframes()/wav.getframerate()<3.0
for name in ['ribbit','eat','happy','click','sleep','clean','coin','buy','error']:
    p=root/'romfs'/(name+'.pcm')
    assert p.exists() and 100<p.stat().st_size<262144 and p.stat().st_size%2==0,name
for path in ['Makefile','app.rsf','.github/workflows/build.yml','tools/build_cia.sh']:
    assert (root/path).is_file(),path
print('PASS: 24x19 sprite, HOME banner/icon, banner WAV, 9 SFX and CI files')
