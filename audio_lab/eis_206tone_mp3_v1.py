#!/usr/bin/env python3
import argparse, json, math, wave, os, subprocess
import numpy as np

SR=48000
N_TONES=206
FMIN=300.0
FMAX=19000.0
FREQS=np.linspace(FMIN,FMAX,N_TONES)

def write_wav(path, ids, cell):
    phase=np.arange(cell,dtype=np.float64)/SR
    with wave.open(path,'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        for k in ids:
            x=0.92*np.sin(2*np.pi*FREQS[int(k)]*phase)
            q=np.rint(np.clip(x,-1,1)*32767).astype('<i2')
            w.writeframes(q.tobytes())

def read_wav(path):
    with wave.open(path,'rb') as w:
        assert w.getnchannels()==1 and w.getsampwidth()==2
        sr=w.getframerate()
        x=np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').astype(np.float64)/32768.0
    return sr,x

def decode_cells(x, cell, nsyms, offset=0):
    # correlation bank; each listener is effectively one tuned frequency channel.
    n=np.arange(cell,dtype=np.float64)/SR
    win=np.hanning(cell) if cell>4 else np.ones(cell)
    basis=np.exp(-2j*np.pi*FREQS[:,None]*n[None,:])*win[None,:]
    out=np.empty(nsyms,dtype=np.int16)
    for i in range(nsyms):
        seg=x[offset+i*cell:offset+(i+1)*cell]
        if len(seg)<cell: out[i]=-1; continue
        z=basis@seg
        out[i]=int(np.argmax(np.abs(z)))
    return out

def best_offset(x, ids, cell):
    # MP3 encoders may add/remove encoder delay. Search a small window by a preamble.
    pre=min(64,len(ids))
    best=(10**9,0)
    maxoff=min(4096,max(0,len(x)-pre*cell))
    step=max(1,cell//16)
    for off in range(0,maxoff+1,step):
        r=decode_cells(x,cell,pre,off)
        e=int(np.count_nonzero(r!=ids[:pre]))
        if e<best[0]: best=(e,off)
        if e==0: break
    # refine around best coarse offset
    lo=max(0,best[1]-step); hi=min(maxoff,best[1]+step)
    for off in range(lo,hi+1):
        r=decode_cells(x,cell,pre,off)
        e=int(np.count_nonzero(r!=ids[:pre]))
        if e<best[0]: best=(e,off)
        if e==0: return off
    return best[1]

def run(outdir, symbols=4096, seed=7):
    os.makedirs(outdir,exist_ok=True)
    rng=np.random.default_rng(seed)
    # deterministic preamble covers all 206 tones, then random source-like choices.
    ids=np.concatenate([np.arange(N_TONES,dtype=np.int16),rng.integers(0,N_TONES,size=symbols-N_TONES,dtype=np.int16)])
    results=[]
    for cell in (2048,1024,512,256,128,64):
        wav=f"{outdir}/c{cell}.wav"
        write_wav(wav,ids,cell)
        wav_size=os.path.getsize(wav)
        for br in (320,256,192,160,128,96,64):
            mp3=f"{outdir}/c{cell}_{br}.mp3"
            dec=f"{outdir}/c{cell}_{br}_dec.wav"
            subprocess.run(["ffmpeg","-loglevel","error","-y","-i",wav,"-codec:a","libmp3lame","-b:a",f"{br}k",mp3],check=True)
            subprocess.run(["ffmpeg","-loglevel","error","-y","-i",mp3,"-ac","1","-ar",str(SR),"-c:a","pcm_s16le",dec],check=True)
            sr,x=read_wav(dec); assert sr==SR
            off=best_offset(x,ids,cell)
            rec=decode_cells(x,cell,len(ids),off)
            errors=int(np.count_nonzero(rec!=ids))
            results.append({
                "cell_samples":cell,"cell_ms":cell/SR*1000.0,
                "bitrate_kbps":br,"symbols":int(len(ids)),
                "errors":errors,"exact":errors==0,"offset_samples":off,
                "wav_bytes":wav_size,"mp3_bytes":os.path.getsize(mp3),
                "bytes_per_symbol_mp3":os.path.getsize(mp3)/len(ids),
                "tone_spacing_hz":float(FREQS[1]-FREQS[0]),
                "fmin_hz":FMIN,"fmax_hz":FMAX
            })
    exact=[r for r in results if r["exact"]]
    best=min(exact,key=lambda r:r["bytes_per_symbol_mp3"]) if exact else None
    return {"tones":N_TONES,"sample_rate":SR,"symbols":int(len(ids)),"best_exact":best,"results":results}

if __name__=="__main__":
    ap=argparse.ArgumentParser(); ap.add_argument("--outdir",default="mp3_206"); ap.add_argument("--symbols",type=int,default=4096)
    a=ap.parse_args()
    print(json.dumps(run(a.outdir,a.symbols),indent=2))

# CANONICAL REPLICATION NOTICE (2026-09-18): predecessor/lineage artifact. Current protocol: compression/EIS_K32_FORMAL_REPLICATION_V1.md
