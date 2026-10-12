#!/usr/bin/env python3
"""Gerald Pondside Deluxe assets: generated from ORIGINAL pixel sprites and synth waveforms.
Run from repository root: python3 tools/gen_assets.py (requires Pillow).
Existing PCM files are regenerated, all deterministic. Python 3.10+.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import math, random, wave, struct, re
root = Path(__file__).resolve().parent.parent
meta, romfs = root/'meta', root/'romfs'
meta.mkdir(exist_ok=True); romfs.mkdir(exist_ok=True)
source = (root/'source/main.c').read_text()
section = source.split('static const char *FROG[FH] = {', 1)[1].split('};', 1)[0]
rows = re.findall(r'"([.a-z]+)"', section)
assert len(rows) == 19 and all(len(x) <= 24 for x in rows)
P = {'o': '#1f4b30', 'g': '#4fbd41', 'h': '#86e760', 'd': '#308436',
     'l': '#c9f096', 'w': '#fffde1', 'k': '#183227', 'p': '#f0767d'}
frog = Image.new('RGBA', (24, 19))
for y,row in enumerate(rows):
    for x,ch in enumerate(row):
        if ch in P: frog.putpixel((x,y),tuple(bytes.fromhex(P[ch][1:]))+(255,))

def scene(w=128,h=64):
    im=Image.new('RGBA',(w,h),'#80cce7'); d=ImageDraw.Draw(im)
    d.rectangle((0,25,w,h),fill='#a2e5e8')
    d.rectangle((0,40,w,h),fill='#2e93b3')
    # distant forest and riverbank
    for i in range(19):
        x=i*8-2;y=32-(i*3)%9
        d.rectangle((x,y,x+12,42),fill='#4a9c80')
        d.rectangle((x+3,y-7,x+9,y),fill='#4a9c80')
    d.rectangle((0,39,w,40),fill='#9bd9c5')
    # tree, canopy, dock
    d.rectangle((1,10,10,49),fill='#754b34')
    d.rectangle((4,11,7,48),fill='#a36d45')
    for x,y in [(0,0),(7,0),(12,6),(0,12),(19,0),(21,12)]:
        d.rectangle((x,y,x+13,y+12),fill='#24754e')
        d.rectangle((x+2,y+2,x+9,y+5),fill='#4caa62')
    d.rectangle((0,43,42,47),fill='#a36a39')
    for x in range(2,39,8):d.rectangle((x,43,x+1,46),fill='#5b3b2b')
    d.rectangle((8,47,11,53),fill='#5c3e2d')
    # pond lily pads
    for x,y in [(28,56),(46,48),(85,54),(100,46),(114,58)]:
        d.rectangle((x-8,y-1,x+8,y+2),fill='#226845')
        d.rectangle((x-6,y-3,x+6,y),fill='#54aa5a')
    for x,y in [(19,48),(105,52)]:
        d.rectangle((x-5,y,x+5,y+1),fill='#4d9658')
        d.rectangle((x-2,y-5,x+2,y),fill='#eda4b1')
        d.rectangle((x-1,y-3,x+1,y),fill='#ffe48e')
    for x,y in [(11,53),(71,56),(90,43),(118,59)]:
        d.rectangle((x,y,x+8,y),fill='#7dcad1')
    d.rectangle((28,49,80,53),fill='#226e49')
    d.rectangle((32,46,76,50),fill='#4da459')
    return im

sc=scene()
# Main 3DS banner: official-size 256x128, custom pixel title in the 3DS HOME bubble.
b=sc.resize((256,128),Image.Resampling.NEAREST)
F=frog.resize((80,63),Image.Resampling.NEAREST)
b.alpha_composite(F,(50,42))
d=ImageDraw.Draw(b)
d.rounded_rectangle((137,18,252,100),radius=5,fill='#17334b',outline='#d9f3b2',width=2)
# Pixel text drawn via bitmap font; Pillow's default font is a built-in bitmap.
ft=ImageFont.load_default()
d.text((150,30),'GERALD',font=ft,fill='#c6f68c',stroke_width=0)
d.text((152,51),'THE FROG',font=ft,fill='#ffe18b')
d.text((144,75),'POND DELUXE',font=ft,fill='#fcf4d6')
d.rectangle((140,107,250,112),fill='#397845')
# upscale threshold keeps stronger pixel feel, sans gradients.
b.convert('RGB').save(meta/'banner.png')
# Proper 48x48 HOME-menu icon.
i=sc.resize((48,48),Image.Resampling.NEAREST)
F2=frog.resize((39,31),Image.Resampling.NEAREST)
i.alpha_composite(F2,(4,13))
i.convert('RGBA').save(meta/'icon.png')

RATE=22050
random.seed(1234)
def note(hz,duration,vol=.30,kind='square',slide=0):
    n=max(1,int(RATE*duration));out=[]; phase=0.0
    for idx in range(n):
        t=idx/n;phase+=(hz+slide*t)/RATE
        z=phase%1
        raw = (1 if z<.5 else -1) if kind=='square' else (4*abs(z-.5)-1 if kind=='tri' else math.sin(phase*2*math.pi))
        envelope=min(1,idx/130.)*(1-t)**.75
        out.append(vol*raw*envelope)
    return out
def wait(t):return [0.0]*int(RATE*t)
def croak(t=.19):
    out=[];n=int(RATE*t)
    for j in range(n):
        v=j/RATE; f=150+30*math.sin(v*37)
        out.append(.35*math.sin(2*math.pi*f*v)*math.sin(j*math.pi/n))
    return out
def pcm(samples):
    return b''.join(struct.pack('<h',int(max(-1,min(1,s))*32767)) for s in samples)
sfx={
 'ribbit':croak(.18)+wait(.06)+croak(.25),
 'eat':note(600,.09,.34,'square',-180)+note(400,.12,.30,'tri',-230),
 'happy':sum((note(fr,.11,.28,'square') for fr in (523,659,784,1047)),[]),
 'click':note(1150,.05,.25),
 'sleep':sum((note(fr,.26,.23,'sine') for fr in (392,330,262)),[]),
 'clean':sum((note(550+i*55,.06,.2,'sine',280)+wait(.03) for i in range(6)),[]),
 'coin':note(740,.09,.28)+note(1080,.16,.32,'tri'),
 'buy':note(440,.10,.28)+note(660,.10,.26)+note(880,.22,.30,'tri'),
 'error':note(280,.13,.22,'square',-80)+note(180,.13,.16,'square')
}
for nm,samples in sfx.items(): (romfs/(nm+'.pcm')).write_bytes(pcm(samples))
jingle=croak(.2)+wait(.08)+sum((note(fr,.13,.28) for fr in (392,523,659,784)),[])+note(1047,.35,.30)
with wave.open(str(meta/'banner.wav'),'wb') as out:
    out.setnchannels(1);out.setsampwidth(2);out.setframerate(RATE);out.writeframes(pcm(jingle))
print('Generated banner.png, icon.png, banner.wav and',len(sfx),'sound effects')
