#!/usr/bin/env python3
import argparse, json, math, os, wave, subprocess, tempfile
import numpy as np

SR=48000
C=206

def make_freqs(ncar, fmin=400.0, fmax=19500.0):
    # Equal spacing across MP3-friendly audible band.
    return np.linspace(fmin,fmax,ncar,dtype=np.float64)

def synth(ids, hop, pulse, banks=1, sample_bits=16):
    ncar=C*banks
    freqs=make_freqs(ncar)
    N=(len(ids)-1)*hop+pulse
    x=np.zeros(N,dtype=np.float64)
    n=np.arange(pulse,dtype=np.float64)
    # Hann pulse gives finite support and better spectral isolation.
    win=np.hanning(pulse)
    bank_state=np.zeros(C,dtype=np.uint8)
    for i,sym in enumerate(ids):
        b=0
        if banks==2:
            # Alternate duplicate carrier for each repeated occurrence of a character.
            b=int(bank_state[int(sym)]); bank_state[int(sym)]^=1
        k=int(sym)+b*C
        tone=np.sin(2*np.pi*freqs[k]*n/SR)*win
        x[i*hop:i*hop+pulse]+=tone
    # Normalize composite waveform globally; preserve linear superposition.
    peak=float(np.max(np.abs(x))) or 1.0
    x*=0.92/peak
    if sample_bits==16:
        q=np.rint(np.clip(x,-1,1)*32767).astype('<i2')
    else:
        q=np.rint((np.clip(x,-1,1)*127.5)+127.5).astype(np.uint8)
    return q,freqs

def write_wav(path,q,bits):
    with wave.open(path,'wb') as w:
        w.setnchannels(1); w.setsampwidth(bits//8); w.setframerate(SR); w.writeframes(q.tobytes())

def wav_float(path):
    with wave.open(path,'rb') as w:
        bits=w.getsampwidth()*8; sr=w.getframerate(); raw=w.readframes(w.getnframes())
    if bits==16: x=np.frombuffer(raw,dtype='<i2').astype(np.float64)/32768.0
    elif bits==8: x=(np.frombuffer(raw,dtype=np.uint8).astype(np.float64)-127.5)/127.5
    else: raise ValueError(bits)
    return sr,x

def build_templates(freqs,pulse):
    n=np.arange(pulse,dtype=np.float64)
    win=np.hanning(pulse)
    # normalized complex matched filters
    T=np.exp(-2j*np.pi*freqs[:,None]*n[None,:]/SR)*win[None,:]
    norms=np.sqrt(np.sum(np.abs(T)**2,axis=1))
    return T/norms[:,None]

def decode(x, count, hop, pulse, freqs, banks):
    T=build_templates(freqs,pulse)
    out=np.empty(count,dtype=np.int16)
    # For each keypress time, listen to all channels to identify newly triggered tone.
    # Subtract previously reconstructed ringing tones so overlap becomes decoder work.
    recon=np.zeros_like(x)
    n=np.arange(pulse,dtype=np.float64)
    win=np.hanning(pulse)
    # derive source amplitude scaling adaptively from residual; only identity matters.
    for i in range(count):
        p=i*hop
        seg=x[p:p+pulse]-recon[p:p+pulse]
        if len(seg)<pulse:
            out[i]=-1; continue
        z=T@seg
        k=int(np.argmax(np.abs(z)))
        out[i]=k%C
        # estimate complex coefficient only enough to cancel this detected carrier.
        # Since synthesis uses zero-phase sine, least-squares scalar on known real pulse.
        basis=np.sin(2*np.pi*freqs[k]*n/SR)*win
        den=float(np.dot(basis,basis))
        a=float(np.dot(seg,basis)/den) if den else 0.0
        recon[p:p+pulse]+=a*basis
    return out

def run(symbols=12000, seed=42):
    rng=np.random.default_rng(seed)
    # realistic skew isn't needed for channel floor; include every symbol then random.
    ids=np.concatenate([np.arange(C,dtype=np.int16),rng.integers(0,C,size=symbols-C,dtype=np.int16)])
    rows=[]
    # pulse 1024 gives ~46.9 Hz DFT-bin width; 206 carriers across 19.1 kHz => ~93 Hz spacing.
    for banks,pulse in ((1,1024),(2,2048)):
      for hop in (64,32,16,8,4,2,1):
        q,freqs=synth(ids,hop,pulse,banks,16)
        with tempfile.TemporaryDirectory() as td:
            wav=os.path.join(td,"x.wav");write_wav(wav,q,16)
            _,x=wav_float(wav)
            rec=decode(x,len(ids),hop,pulse,freqs,banks)
            err=int(np.count_nonzero(rec!=ids))
            wavb=os.path.getsize(wav)
            row={"banks":banks,"carriers":C*banks,"pulse_samples":pulse,"hop_samples":hop,
                 "symbols":len(ids),"errors":err,"exact":err==0,"wav_bytes":wavb,
                 "bytes_per_symbol_wav":wavb/len(ids),"symbols_per_second":SR/hop,
                 "projected_wav_bytes_1e9":int(44+((1_000_000_000-1)*hop+pulse)*2),
                 "tone_spacing_hz":float(freqs[1]-freqs[0])}
            if err==0:
                # Test lossy MP3 only on exact WAV cases.
                for br in (320,256,192,128,96,64):
                    mp3=os.path.join(td,f"x_{br}.mp3");decw=os.path.join(td,f"d_{br}.wav")
                    subprocess.run(["ffmpeg","-loglevel","error","-y","-i",wav,"-codec:a","libmp3lame","-b:a",f"{br}k",mp3],check=True)
                    subprocess.run(["ffmpeg","-loglevel","error","-y","-i",mp3,"-ac","1","-ar",str(SR),"-c:a","pcm_s16le",decw],check=True)
                    _,mx=wav_float(decw)
                    # ffmpeg normally removes encoder delay in decoded WAV; trim/pad conservatively.
                    if len(mx)<len(x): mx=np.pad(mx,(0,len(x)-len(mx)))
                    elif len(mx)>len(x): mx=mx[:len(x)]
                    mr=decode(mx,len(ids),hop,pulse,freqs,banks)
                    me=int(np.count_nonzero(mr!=ids))
                    row[f"mp3_{br}_bytes"]=os.path.getsize(mp3)
                    row[f"mp3_{br}_errors"]=me
                    row[f"mp3_{br}_exact"]=me==0
                    row[f"mp3_{br}_bytes_per_symbol"]=os.path.getsize(mp3)/len(ids)
            rows.append(row)
            if err: break # smaller hops will be harder; stop this bank
    exact=[r for r in rows if r["exact"]]
    bestwav=min(exact,key=lambda r:r["bytes_per_symbol_wav"],default=None)
    mp3c=[]
    for r in exact:
        for br in (320,256,192,128,96,64):
            if r.get(f"mp3_{br}_exact"):
                mp3c.append((r[f"mp3_{br}_bytes_per_symbol"],br,r))
    bestmp3=None
    if mp3c:
        b,br,r=min(mp3c,key=lambda x:x[0])
        bestmp3={"bytes_per_symbol":b,"bitrate_kbps":br,"banks":r["banks"],"hop_samples":r["hop_samples"],
                 "pulse_samples":r["pulse_samples"],"projected_mp3_bytes_1e9":int(b*1_000_000_000)}
    return {"model":"overlapped music-powered keyboard; one tone onset per character, pulses ring concurrently",
            "sample_rate":SR,"best_exact_wav":bestwav,"best_exact_mp3":bestmp3,"rows":rows}

if __name__=="__main__":
    ap=argparse.ArgumentParser();ap.add_argument("--symbols",type=int,default=12000);a=ap.parse_args()
    print(json.dumps(run(a.symbols),indent=2))

# CANONICAL REPLICATION NOTICE (2026-09-18): predecessor/lineage artifact. Current protocol: compression/EIS_K32_FORMAL_REPLICATION_V1.md
