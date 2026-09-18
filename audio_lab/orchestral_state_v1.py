#!/usr/bin/env python3
import argparse, hashlib, json, math, os, subprocess, wave, lzma
from pathlib import Path
import numpy as np

def sha(b): return hashlib.sha256(b).hexdigest()

def encode(data, wav_path, sr, block, carriers, order, channels):
    if order < 2 or order & (order-1): raise ValueError("order must be power of two")
    if carriers >= block//2: raise ValueError("carriers must be < block/2")
    bpt = int(math.log2(order))
    bits = np.unpackbits(np.frombuffer(data, dtype=np.uint8))
    cap = carriers*bpt*channels
    blocks = (len(bits)+cap-1)//cap
    pad = blocks*cap-len(bits)
    if pad: bits=np.pad(bits,(0,pad))
    amp = 0.44*block/carriers
    bins=np.arange(1,carriers+1)
    out=np.empty((blocks*block,channels),dtype=np.int16)
    p=0
    weights=(1 << np.arange(bpt-1,-1,-1,dtype=np.int64))
    for bi in range(blocks):
        for ch in range(channels):
            z=bits[p:p+carriers*bpt].reshape(carriers,bpt); p += carriers*bpt
            sym=(z*weights).sum(axis=1)
            X=np.zeros(block,dtype=np.complex128)
            vals=amp*np.exp(1j*(2*np.pi*sym/order))
            X[bins]=vals; X[-bins]=np.conj(vals)
            x=np.fft.ifft(X).real
            out[bi*block:(bi+1)*block,ch]=np.rint(np.clip(x,-0.999969,0.999969)*32767).astype(np.int16)
    with wave.open(wav_path,"wb") as w:
        w.setnchannels(channels); w.setsampwidth(2); w.setframerate(sr)
        w.writeframes(out.astype("<i2",copy=False).tobytes())
    duration=blocks*block/sr
    return dict(blocks=blocks,pad_bits=int(pad),bits_per_event=cap,
                bytes_per_event=cap/8,duration_s=duration,
                payload_bps=(len(data)*8/duration if duration else 0))

def decode(wav_path,nbytes,block,carriers,order):
    bpt=int(math.log2(order))
    with wave.open(wav_path,"rb") as w:
        channels=w.getnchannels(); frames=w.getnframes(); raw=w.readframes(frames)
    x=np.frombuffer(raw,dtype="<i2").astype(np.float64).reshape(-1,channels)/32767.0
    if len(x)%block: raise ValueError("unaligned WAV")
    bins=np.arange(1,carriers+1); bits=[]
    for bi in range(len(x)//block):
        for ch in range(channels):
            X=np.fft.fft(x[bi*block:(bi+1)*block,ch])
            phase=np.mod(np.angle(X[bins]),2*np.pi)
            syms=np.rint(phase*order/(2*np.pi)).astype(np.int64)%order
            for v in syms:
                for s in range(bpt-1,-1,-1): bits.append((int(v)>>s)&1)
    return np.packbits(np.asarray(bits[:nbytes*8],dtype=np.uint8)).tobytes()

def pcm_sha(p):
    with wave.open(p,"rb") as w: b=w.readframes(w.getnframes())
    return sha(b)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("source"); ap.add_argument("--prefix",type=int,default=32768)
    ap.add_argument("--sr",type=int,default=48000); ap.add_argument("--block",type=int,default=48)
    ap.add_argument("--carriers",type=int,default=23); ap.add_argument("--order",type=int,default=16384)
    ap.add_argument("--channels",type=int,default=1); ap.add_argument("--out-prefix",default="orch")
    a=ap.parse_args()
    data=Path(a.source).read_bytes()[:a.prefix]
    wav=a.out_prefix+".wav"; flac=a.out_prefix+".flac"; rt=a.out_prefix+".rt.wav"
    meta=encode(data,wav,a.sr,a.block,a.carriers,a.order,a.channels)
    rec=decode(wav,len(data),a.block,a.carriers,a.order)
    exact=rec==data
    wb=Path(wav).read_bytes()
    xb=lzma.compress(wb,format=lzma.FORMAT_XZ,preset=lzma.PRESET_EXTREME|9)
    Path(a.out_prefix+".wav.xz").write_bytes(xb)
    subprocess.run(["flac","-8","-f","-o",flac,wav],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    subprocess.run(["flac","-d","-f","-o",rt,flac],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    result=dict(codec="orchestral_state_psk_v1",source_bytes=len(data),source_sha256=sha(data),
                recovered_sha256=sha(rec),exact=exact,sr=a.sr,block_samples=a.block,
                carriers_per_channel=a.carriers,psk_order=a.order,channels=a.channels,**meta,
                wav_bytes=os.path.getsize(wav),flac_bytes=os.path.getsize(flac),
                wav_xz_bytes=len(xb),flac_pcm_exact=(pcm_sha(wav)==pcm_sha(rt)))
    result["source_to_wav_ratio"]=result["wav_bytes"]/len(data)
    result["source_to_flac_ratio"]=result["flac_bytes"]/len(data)
    result["source_to_wav_xz_ratio"]=result["wav_xz_bytes"]/len(data)
    print(json.dumps(result,indent=2))
    raise SystemExit(0 if exact and result["flac_pcm_exact"] else 3)

if __name__=="__main__": main()
