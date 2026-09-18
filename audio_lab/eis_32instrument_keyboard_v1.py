#!/usr/bin/env python3
import argparse, json, math, os, subprocess, wave, hashlib
from pathlib import Path
import numpy as np

CARRIERS=32
BITS_AXIS=4
BYTES_PER_EVENT=CARRIERS
SR=48000

def sha(b): return hashlib.sha256(b).hexdigest()

def bins_for(block):
    hi=block//2-1
    if hi < CARRIERS: raise ValueError("block too short for 32 complex carriers")
    # spread as far as possible across all usable positive-frequency bins
    b=np.rint(np.linspace(1,hi,CARRIERS)).astype(int)
    # enforce uniqueness deterministically
    for i in range(1,len(b)):
        if b[i]<=b[i-1]: b[i]=b[i-1]+1
    if b[-1]>hi: raise ValueError("insufficient bins")
    return b

def encode_bytes(data, block, sample_bits, drive):
    pad=(-len(data))%BYTES_PER_EVENT
    if pad: data=data+bytes([0])*pad
    u=np.frombuffer(data,dtype=np.uint8).reshape(-1,CARRIERS)
    hi=(u>>4).astype(np.float64)
    lo=(u&15).astype(np.float64)
    I=(2*hi/15.0)-1.0
    Q=(2*lo/15.0)-1.0
    bins=bins_for(block)
    A=drive*block/(2*CARRIERS*math.sqrt(2))
    lim=(1<<(sample_bits-1))-1
    out=np.empty((len(u),block),dtype=np.int32)
    peak=0.0
    batch=2048
    for p in range(0,len(u),batch):
        ii=I[p:p+batch]; qq=Q[p:p+batch]
        X=np.zeros((len(ii),block),dtype=np.complex128)
        vals=A*(ii+1j*qq)
        X[:,bins]=vals
        X[:,-bins]=np.conj(vals)
        x=np.fft.ifft(X,axis=1).real
        peak=max(peak,float(np.max(np.abs(x))))
        out[p:p+len(ii)]=np.rint(np.clip(x,-1,1)*lim).astype(np.int32)
    return out,pad,peak,bins,A

def decode_pcm(q,nbytes,block,sample_bits,drive):
    lim=(1<<(sample_bits-1))-1
    x=q.astype(np.float64)/lim
    bins=bins_for(block)
    A=drive*block/(2*CARRIERS*math.sqrt(2))
    nblocks=len(x)//block
    x=x[:nblocks*block].reshape(nblocks,block)
    out=np.empty((nblocks,CARRIERS),dtype=np.uint8)
    batch=2048
    for p in range(0,nblocks,batch):
        z=np.fft.fft(x[p:p+batch],axis=1)[:,bins]/A
        vi=np.clip(np.rint((z.real+1)*15/2),0,15).astype(np.uint8)
        vq=np.clip(np.rint((z.imag+1)*15/2),0,15).astype(np.uint8)
        out[p:p+len(vi)]=(vi<<4)|vq
    return out.reshape(-1).tobytes()[:nbytes]

def write_wav(path,q,bits):
    with wave.open(path,"wb") as w:
        w.setnchannels(1);w.setsampwidth(bits//8);w.setframerate(SR)
        if bits==16:
            w.writeframes(q.astype("<i2").tobytes())
        elif bits==24:
            v=q.astype(np.int64).reshape(-1)&0xFFFFFF
            o=np.empty((v.size,3),dtype=np.uint8)
            o[:,0]=(v&255).astype(np.uint8);o[:,1]=((v>>8)&255).astype(np.uint8);o[:,2]=((v>>16)&255).astype(np.uint8)
            w.writeframes(o.tobytes())
        else: raise ValueError(bits)

def read_wav(path):
    with wave.open(path,"rb") as w:
        bits=w.getsampwidth()*8
        raw=w.readframes(w.getnframes())
    if bits==16:
        q=np.frombuffer(raw,dtype="<i2").astype(np.int32)
    elif bits==24:
        b=np.frombuffer(raw,dtype=np.uint8).reshape(-1,3)
        q=b[:,0].astype(np.int32)|(b[:,1].astype(np.int32)<<8)|(b[:,2].astype(np.int32)<<16)
        q[q&0x800000!=0]-=1<<24
    else: raise ValueError(bits)
    return q,bits

def run(source,outdir,prefix):
    os.makedirs(outdir,exist_ok=True)
    data=Path(source).read_bytes()[:prefix]
    rows=[]
    # Stage 1: find the densest exact PCM points first.
    for sample_bits in (16,24):
      for block in (66,72,80,96,112,128,160,192):
        for drive in (0.8,1.0,1.2,1.4):
          q,pad,peak,bins,A=encode_bytes(data,block,sample_bits,drive)
          tag=f"b{block}_s{sample_bits}_d{drive}"
          wav=os.path.join(outdir,tag+".wav")
          write_wav(wav,q,sample_bits)
          rq,_=read_wav(wav)
          rec=decode_pcm(rq,len(data),block,sample_bits,drive)
          exact=rec==data
          row={
            "tag":tag,"block_samples":block,"sample_bits":sample_bits,"drive":drive,
            "carriers":CARRIERS,"listeners":CARRIERS,"bytes_per_event":BYTES_PER_EVENT,
            "payload_bytes":len(data),"events":math.ceil(len(data)/BYTES_PER_EVENT),
            "pcm_exact":exact,"pcm_errors":sum(a!=b for a,b in zip(rec,data)),
            "wav_bytes":os.path.getsize(wav),"preclip_peak":peak,"clipped":peak>1.0,
            "bin_first":int(bins[0]),"bin_last":int(bins[-1]),
            "min_bin_gap":int(np.min(np.diff(bins))),
            "tone_span_hz":float((bins[-1]-bins[0])*SR/block),
            "bytes_per_source_byte_wav":os.path.getsize(wav)/len(data)
          }
          rows.append(row)
    pcm=[r for r in rows if r["pcm_exact"] and not r["clipped"]]
    best_pcm=min(pcm,key=lambda r:r["bytes_per_source_byte_wav"]) if pcm else None

    # Stage 2: MP3 only on the six smallest exact PCM candidates.
    candidates=sorted(pcm,key=lambda r:r["bytes_per_source_byte_wav"])[:6]
    mp=[]
    for r in candidates:
      wav=os.path.join(outdir,r["tag"]+".wav")
      for br in (320,256,192,160,128,112,96,80,64,56,48,40,32):
        mp3=os.path.join(outdir,r["tag"]+f"_{br}.mp3")
        dw=os.path.join(outdir,r["tag"]+f"_{br}_dec.wav")
        subprocess.run(["ffmpeg","-loglevel","error","-y","-i",wav,"-codec:a","libmp3lame","-b:a",f"{br}k",mp3],check=True)
        subprocess.run(["ffmpeg","-loglevel","error","-y","-i",mp3,"-ac","1","-ar",str(SR),"-c:a","pcm_s16le",dw],check=True)
        dq,dbits=read_wav(dw)
        drec=decode_pcm(dq,len(data),r["block_samples"],16,r["drive"])
        errors=sum(a!=b for a,b in zip(drec,data))
        trial={"bitrate_kbps":br,"mp3_bytes":os.path.getsize(mp3),"errors":errors,
               "exact":errors==0,"ratio":os.path.getsize(mp3)/len(data)}
        r.setdefault("mp3_trials",[]).append(trial)
        if trial["exact"]:
          mp.append((trial["ratio"],br,r,trial))
    best_mp3=None
    if mp:
      ratio,br,r,t=min(mp,key=lambda x:x[0])
      best_mp3={"ratio":ratio,"bitrate_kbps":br,"block_samples":r["block_samples"],
                "source_sample_bits":r["sample_bits"],"drive":r["drive"],"mp3_bytes":t["mp3_bytes"]}
    return {
      "codec":"EIS 32-instrument / 32-listener byte keyboard v1",
      "mapping":"each fixed carrier/listener emits one 8-bit letter state (4-bit I + 4-bit Q); 32 letters per event",
      "source_bytes":len(data),"source_sha256":sha(data),
      "best_pcm":best_pcm,"best_mp3":best_mp3,
      "mp3_candidates_tested":[r["tag"] for r in candidates],
      "rows":rows
    }
if __name__=="__main__":
    ap=argparse.ArgumentParser();ap.add_argument("source");ap.add_argument("--prefix",type=int,default=262144);ap.add_argument("--outdir",default="eis32")
    a=ap.parse_args();print(json.dumps(run(a.source,a.outdir,a.prefix),indent=2))
