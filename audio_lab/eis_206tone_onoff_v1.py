#!/usr/bin/env python3
import argparse, json, math, os, wave
import numpy as np

N=412
CARRIERS=206
SR=48000
AMP=0.9
THRESH=0.30

def templates():
    out=np.empty((CARRIERS,N),dtype=np.int16)
    for c in range(CARRIERS):
        h=np.zeros(N//2+1,dtype=np.complex128)
        h[c+1]=AMP
        x=np.fft.irfft(h,n=N)
        out[c]=np.rint(np.clip(x,-1,1)*32767).astype(np.int16)
    return out

def scan_alphabet(path):
    s=set()
    with open(path,'rb') as f:
        while True:
            b=f.read(8<<20)
            if not b: break
            s.update(b)
    return bytes(sorted(s))

def encode(src, fullsrc, outwav, mapfile, limit):
    alpha=scan_alphabet(fullsrc)
    if len(alpha)!=206:
        raise SystemExit(f"expected 206 symbols, got {len(alpha)}")
    open(mapfile,'wb').write(alpha)
    lut=np.full(256,-1,dtype=np.int16)
    for i,b in enumerate(alpha): lut[b]=i
    t=templates()
    with open(src,'rb') as f:
        data=f.read(limit)
    idx=lut[np.frombuffer(data,dtype=np.uint8)]
    if (idx<0).any(): raise SystemExit("unmapped byte")
    with wave.open(outwav,'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        batch=2048
        for p in range(0,len(idx),batch):
            q=t[idx[p:p+batch]].reshape(-1)
            w.writeframes(q.astype('<i2',copy=False).tobytes())
    return len(data)

def decode(inwav,mapfile,out):
    alpha=np.frombuffer(open(mapfile,'rb').read(),dtype=np.uint8)
    if len(alpha)!=206: raise SystemExit("bad map")
    with wave.open(inwav,'rb') as w:
        assert w.getnchannels()==1 and w.getsampwidth()==2
        total=w.getnframes()
        if total%N: raise SystemExit("nonintegral frames")
        count=total//N
        with open(out,'wb') as g:
            batch=1024
            for p in range(0,count,batch):
                m=min(batch,count-p)
                raw=w.readframes(m*N)
                q=np.frombuffer(raw,dtype='<i2').reshape(m,N).astype(np.float64)/32767.0
                h=np.fft.rfft(q,axis=1)
                mags=np.abs(h[:,1:207])
                # one-hot source: active carrier is the strongest fixed bin.
                ids=np.argmax(mags,axis=1)
                peaks=mags[np.arange(m),ids]
                if np.any(peaks<THRESH): raise SystemExit("weak active tone")
                g.write(alpha[ids].tobytes())
    return count

def selftest(seed=1):
    rng=np.random.default_rng(seed)
    tests=[np.zeros(CARRIERS,dtype=bool),np.ones(CARRIERS,dtype=bool)]
    for i in range(CARRIERS):
        x=np.zeros(CARRIERS,dtype=bool);x[i]=1;tests.append(x)
    for den in (0.01,0.1,0.5,0.9):
        for _ in range(200): tests.append(rng.random(CARRIERS)<den)
    err=0;min_on=1e9;max_off=0.0
    for bits in tests:
        h=np.zeros(N//2+1,dtype=np.complex128)
        h[1:207]=bits.astype(float)*AMP
        x=np.fft.irfft(h,n=N)
        q=np.rint(np.clip(x,-1,1)*32767).astype(np.int16)
        r=np.fft.rfft(q.astype(np.float64)/32767.0)
        mags=np.abs(r[1:207])
        dec=mags>THRESH
        err+=int(np.count_nonzero(dec!=bits))
        if bits.any(): min_on=min(min_on,float(mags[bits].min()))
        if (~bits).any(): max_off=max(max_off,float(mags[~bits].max()))
    return dict(states=len(tests),bit_errors=err,min_on_mag=min_on,max_off_mag=max_off)

if __name__=="__main__":
    ap=argparse.ArgumentParser()
    sp=ap.add_subparsers(dest="cmd",required=True)
    p=sp.add_parser("selftest")
    p=sp.add_parser("encode");p.add_argument("src");p.add_argument("fullsrc");p.add_argument("wav");p.add_argument("map");p.add_argument("--limit",type=int,default=50000)
    p=sp.add_parser("decode");p.add_argument("wav");p.add_argument("map");p.add_argument("out")
    a=ap.parse_args()
    if a.cmd=="selftest": print(json.dumps(selftest(),indent=2))
    elif a.cmd=="encode": print("SOURCE_BYTES",encode(a.src,a.fullsrc,a.wav,a.map,a.limit))
    else: print("DECODED_BYTES",decode(a.wav,a.map,a.out))
