#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
#
# SPDX-License-Identifier: MIT
"""Zeichnet die beiden Badges, die jeder bekommen kann, aus dem Logo:

- nutzer.png: das F, wie es ist - für alle, die ChattiFlexii nutzen
- supporter.webp: das F mit goldenem Balken, über den ein Lichtstreifen
  wandert, mit Funken und Glitzer - animiert, als WebP, damit die Ränder
  glatt bleiben

    python3 server/chatti-badges/bilder/make.py

Danach in den Speicher des Servers legen, wie in README.md beschrieben.
"""
import importlib.util, math, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter
from pathlib import Path
ZIEL=Path(__file__).resolve().parent
WURZEL=ZIEL.parent.parent.parent
spec=importlib.util.spec_from_file_location('logo',WURZEL/'scripts'/'make-logo.py')
L=importlib.util.module_from_spec(spec); spec.loader.exec_module(L)
N=L.SEITE
bogen=L.strich(L.BALKEN); koerper=np.maximum(L.strich(L.STAMM),L.strich(L.MITTE))
marke=np.maximum(bogen,koerper)
ys,xs=np.where(marke>0.01)
cx=(xs.min()+xs.max())/2; cy=(ys.min()+ys.max())/2; half=max(xs.max()-xs.min(),ys.max()-ys.min())/2+40
box=(int(cx-half),int(cy-half),int(cx+half),int(cy+half))
y,x=np.mgrid[0:N,0:N]

def compose(top_color_fn, stem_rgb, extra=None):
    img=np.zeros((N,N,4))
    shade=L.weich(bogen,12,14,4)
    stem=np.array(stem_rgb,float)[None,None,:]*(1-0.20*shade)[...,None]
    rgb=stem*koerper[...,None]
    a=koerper.copy()
    top=top_color_fn()
    rgb=rgb*(1-bogen[...,None])+top*bogen[...,None]
    a=np.maximum(a,bogen)
    img[...,:3]=rgb; img[...,3]=a
    return img

def purple():
    licht=np.clip(1-np.hypot(x-260,y-200)/480,0,1)
    return np.array(L.BOGEN,float)[None,None,:]+licht[...,None]*np.array([8,30,0.])

def gold(shine=None):
    t=np.clip((x-250)/560,0,1)[...,None]
    g=np.array([255,224,120.])*(1-t)+np.array([240,160,40.])*t
    licht=np.clip(1-np.hypot(x-300,y-210)/420,0,1)[...,None]
    g=g+licht*np.array([0,20,40.])
    if shine is not None:
        # ein heller Streifen, der über den Balken wandert
        d=np.abs((x-y*0.5)-shine)
        g=g+np.clip(1-d/55,0,1)[...,None]*np.array([90,90,140.])
    return np.clip(g,0,255)

def to_img(arr,size):
    im=Image.fromarray(np.clip(arr,0,255).astype(np.uint8)).crop(box)
    return im.resize((size,size),Image.LANCZOS)

def star(draw,cx,cy,r,alpha):
    # vierzackiger Funke
    pts=[]
    for k in range(8):
        ang=math.pi/4*k - math.pi/2
        rr=r if k%2==0 else r*0.28
        pts.append((cx+rr*math.cos(ang),cy+rr*math.sin(ang)))
    draw.polygon(pts,fill=(255,250,225,int(255*alpha)))

# Nutzer
user=compose(purple,L.KOERPER)
to_img(user*np.array([1,1,1,255])[None,None,:]/np.array([1,1,1,1]),72).save(ZIEL/'nutzer.png')

# Supporter, animiert
frames=[]
F=24
for f in range(F):
    t=f/F
    shine=-300+t*1600
    arr=compose(lambda: gold(shine), (150,140,160))
    arr[...,3]*=255
    im=to_img(arr,288)
    d=ImageDraw.Draw(im)
    # zwei Funken, versetzt pulsierend
    # fünf Funken, jeder in seinem Takt
    for (sx,sy,base,ph,speed) in [(246,44,40,0.0,1),(272,104,22,0.45,2),(60,30,26,0.25,1),(150,22,18,0.7,2),(200,130,16,0.15,1)]:
        p=max(0.0,math.sin(2*math.pi*(t*speed+ph)))
        if p<=0.02: continue
        r=base*(0.3+0.7*p)
        star(d,sx,sy,r,0.35+0.65*p)
    # Glitzer: kleine Punkte auf dem Gold, die aufblitzen
    import random
    rnd=random.Random(7)
    for k in range(14):
        gx=rnd.uniform(40,250); gy=rnd.uniform(20,90); ph=rnd.random()
        q=max(0.0,math.sin(2*math.pi*(t*2+ph)))**3
        if q>0.05:
            rr=2+3*q
            d.ellipse([gx-rr,gy-rr,gx+rr,gy+rr],fill=(255,255,240,int(255*q)))
    frames.append(im.resize((72,72),Image.LANCZOS))
frames[0].save(ZIEL/'supporter.webp',save_all=True,append_images=frames[1:],duration=70,loop=0,lossless=True)
