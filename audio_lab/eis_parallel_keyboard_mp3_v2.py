#!/usr/bin/env python3
import argparse, json, math, os, subprocess, wave
import numpy as np

SR=48000

def freqs_for(k):
    # Spread carriers over a broad MP3-survivable band.
    return np.linspace(250.0, 19500.0, k, dtype=np.float64)

def basis(k, cell):
    f=freqs_for(k)
    n=np.arange(cell,dtype=np.float64)/SR
    # fixed phase per carrier, reset each slot
    ph=np.linspace(0.0, np.pi, k, endpoint=False)
    A=np.sin(2*np.pi*n[:,None]*f[None,:] + ph[None,:])
    # column normalize for stable least-squares recovery
    A/=np.sqrt(np.sum(A*A,axis=0,keepdims=True)+1e-30)
    return f,A

def write_pcm(path, states, k, cell):
    f,A=basis(k,cell)
    with wave.open(path,'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        for s in states:
            x=A@s.astype(np.float64)
            peak=np.max(np.abs(x))
            if peak>0: x=x*(0.92/peak)
            q=np.rint(np.clip(x,-1,1)*32767).astype('<i2')
            w.writeframes(q.tobytes())
    return f

def read_pcm(path):
    with wave.open(path,'rb') as w:
        if w.getnchannels()!=1 or w.getsampwidth()!=2 or w.getframerate()!=SR:
            raise RuntimeError("unexpected decoded wav format")
        return np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').astype(np.float64)/32768.0

def decoder_matrix(k,cell):
    f,A=basis(k,cell)
    # One fixed 206/412-channel linear listener. Each output is one tuned control.
    P=np.linalg.pinv(A,rcond=1e-8)
    return P

def decode(x,k,cell,nslots):
    P=decoder_matrix(k,cell)
    out=np.zeros((nslots,k),dtype=np.uint8)
    scores=[]
    for i in range(nslots):
        seg=x[i*cell:(i+1)*cell]
        if len(seg)!=cell: break
        a=P@seg
        aa=np.abs(a)
        m=float(aa.max()) if aa.size else 0.0
        # all active carriers were emitted at equal coefficient before frame scaling.
        # threshold relative to recovered strongest carrier.
        out[i]=(aa >= max(1e-8,0.42*m)).astype(np.uint8)
        scores.append((float(np.max(aa)), float(np.partition(aa,-2)[-2]) if len(aa)>1 else 0.0))
    return out

def mk_states(k,nslots,density,seed):
    rng=np.random.default_rng(seed)
    if density=="onehot":
        s=np.zeros((nslots,k),dtype=np.uint8)
        ids=rng.integers(0,k,size=nslots)
        s[np.arange(nslots),ids]=1
        # force coverage
        m=min(k,nslots)
        s[:m]=0;s[np.arange(m),np.arange(m)]=1
        return s
    p=float(density)
    s=(rng.random((nslots,k))<p).astype(np.uint8)
    empty=np.where(s.sum(axis=1)==0)[0]
    if len(empty):
        s[empty,rng.integers(0,k,size=len(empty))]=1
    return s

def align_decoded(x,states,k,cell):
    # ffmpeg generally compensates encoder delay, but search around zero to be strict.
    maxoff=min(2304,max(0,len(x)-len(states)*cell))
    best=(10**18,0,None)
    probe=min(48,len(states))
    for off in range(0,maxoff+1,max(1,cell//16)):
        r=decode(x[off:],k,cell,probe)
        e=int(np.count_nonzero(r!=states[:probe]))
        if e<best[0]:best=(e,off,r)
        if e==0:return off
    lo=max(0,best[1]-max(1,cell//16));hi=min(maxoff,best[1]+max(1,cell//16))
    for off in range(lo,hi+1):
        r=decode(x[off:],k,cell,probe)
        e=int(np.count_nonzero(r!=states[:probe]))
        if e<best[0]:best=(e,off,r)
        if e==0:return off
    return best[1]

def run(outdir,nslots=512,seed=17):
    os.makedirs(outdir,exist_ok=True)
    results=[]
    for k in (206,412):
      cells=([206,256,320,412,512,768,1024] if k==206 else [412,512,640,824,1024,1536])
      densities=("onehot",0.02,0.05,0.10,0.25)
      for cell in cells:
        for density in densities:
          states=mk_states(k,nslots,density,seed+cell+k+int(100*(density if density!="onehot" else 0)))
          tag=f"k{k}_n{cell}_d{str(density).replace('.','p')}"
          wav=os.path.join(outdir,tag+".wav")
          fs=write_pcm(wav,states,k,cell)
          # verify PCM before MP3
          raw=read_pcm(wav)
          r0=decode(raw,k,cell,nslots)
          pcm_errors=int(np.count_nonzero(r0!=states))
          for br in (320,256,192,160,128,96,64):
            mp3=os.path.join(outdir,tag+f"_{br}.mp3")
            dec=os.path.join(outdir,tag+f"_{br}_dec.wav")
            subprocess.run(["ffmpeg","-loglevel","error","-y","-i",wav,"-codec:a","libmp3lame","-b:a",f"{br}k",mp3],check=True)
            subprocess.run(["ffmpeg","-loglevel","error","-y","-i",mp3,"-ac","1","-ar",str(SR),"-c:a","pcm_s16le",dec],check=True)
            x=read_pcm(dec)
            off=align_decoded(x,states,k,cell)
            rr=decode(x[off:],k,cell,nslots)
            errors=int(np.count_nonzero(rr!=states))
            slot_errors=int(np.count_nonzero(np.any(rr!=states,axis=1)))
            active=int(states.sum())
            duration=nslots*cell/SR
            results.append({
              "carriers":k,"outputs":206 if k==412 else 206,
              "duplicate_tones_per_output":2 if k==412 else 1,
              "cell_samples":cell,"cell_ms":cell/SR*1000,
              "density":density,"slots":nslots,"active_controls":active,
              "pcm_control_errors":pcm_errors,
              "bitrate_kbps":br,"mp3_bytes":os.path.getsize(mp3),
              "duration_sec":duration,"offset_samples":off,
              "control_errors":errors,"slot_errors":slot_errors,
              "exact":errors==0,
              "tone_spacing_hz":float(fs[1]-fs[0]),
              "logical_control_bits":nslots*k,
              "mp3_bits_per_control_state":os.path.getsize(mp3)*8/(nslots*k)
            })
    exact=[r for r in results if r["exact"]]
    best=min(exact,key=lambda z:z["mp3_bits_per_control_state"]) if exact else None
    return {"sample_rate":SR,"best_exact":best,"exact_cases":len(exact),"results":results}

if __name__=="__main__":
    ap=argparse.ArgumentParser();ap.add_argument("--outdir",default="multi206");ap.add_argument("--slots",type=int,default=512)
    a=ap.parse_args()
    print(json.dumps(run(a.outdir,a.slots),indent=2))
