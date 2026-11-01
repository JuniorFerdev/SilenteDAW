"""Reproduce the original vector brand mark into the macOS icon; requires Pillow."""
from pathlib import Path
from PIL import Image, ImageDraw
import subprocess
root = Path(__file__).resolve().parent.parent
scale = 3
im = Image.new('RGBA', (1024*scale,1024*scale), (0,0,0,0))
d = ImageDraw.Draw(im)
def box(coords): return tuple(int(x*scale) for x in coords)
d.rounded_rectangle(box((64,64,960,960)), radius=204*scale, fill='#151c25', outline='#293544', width=2*scale)
points=[]
def line(x,y): points.append((x*2*scale,y*2*scale))
def curve(p0,p1,p2,p3):
    for i in range(1,81):
        t=i/80; u=1-t
        line(u*u*u*p0[0]+3*u*u*t*p1[0]+3*u*t*t*p2[0]+t*t*t*p3[0],u*u*u*p0[1]+3*u*u*t*p1[1]+3*u*t*t*p2[1]+t*t*t*p3[1])
line(356,144);line(226,144)
curve((226,144),(181,144),(158,168),(158,202))
curve((158,202),(158,236),(184,260),(226,260)); line(286,260)
curve((286,260),(328,260),(354,284),(354,318))
curve((354,318),(354,352),(331,376),(286,376)); line(156,376)
d.line(points,fill='#70E0C4',width=80*scale,joint='curve')
for x,y in points: d.ellipse((x-40*scale,y-40*scale,x+40*scale,y+40*scale),fill='#70E0C4')
for x in (224,800): d.rounded_rectangle(box((x-12,456,x+12,568)),radius=12*scale,fill='#70E0C4')
im=im.resize((1024,1024),Image.Resampling.LANCZOS)
im.save(root/'assets'/'Silente.png')
iconset=root/'assets'/'Silente.iconset';iconset.mkdir(exist_ok=True)
for size in (16,32,128,256,512):
    for multiplier in (1,2):
        name=f'icon_{size}x{size}'+('@2x' if multiplier==2 else '')+'.png'
        im.resize((size*multiplier,size*multiplier),Image.Resampling.LANCZOS).save(iconset/name)
subprocess.run(['iconutil','-c','icns',str(iconset),'-o',str(root/'assets'/'Silente.icns')],check=True)
