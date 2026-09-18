#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, json, math, os, subprocess, wave, lzma
from pathlib import Path
import numpy as np

def sha(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()

def pack_pcm(q: np.ndarray, bits: int) -> bytes:
    q=np.asarray(q)
    if bits==16:
        return q.astype("<i2",copy=False).tobytes()
    if bits==24:
        v=q.astype(np.int64) & 0xFFFFFF
        o=np.empty((v.size,3),dtype=np.uint8)
        o[:,0]=(v & 255).astype(np.uint8)
        o[:,1]=((v>>8)&255).astype(np.uint8)
        o[:,2]=((v>>16)&255).astype(np.uint8)
        return o.tobytes()
    raise ValueError("sample bits must be 16 or 24")

def unpack_pcm(raw: bytes, bits: int) -> np.ndarray:
    if bits==16:
        return np.frombuffer(raw,dtype="<i2").astype(np.int32)
    if bits==24:
        b=np.frombuffer(raw,dtype=np.uint8).reshape(-1,3)
        v=(b[:,0].astype(np.int32) | (b[:,1].astype(np.int32)<<8) | (b[:,2].astype(np.int32)<<16))
        neg=(v & 0x800000)!=0
        v[neg]-=1<<24
        return v
    raise ValueError("sample bits must be 16 or 24")

def fixed_pad(n: int) -> np.ndarray:
    # Source-independent balanced pad avoids the high-PAPR all-zero tail.
    if n<=0: return np.empty(0,dtype=np.uint8)
    return (np.arange(n,dtype=np.uint32)*1103515245 + 12345 >> 30).astype(np.uint8) & 1

def encode(data: bytes, wav_path: str, sr: int, block: int, carriers: int,
           bits_axis: int, channels: int, sample_bits: int, drive: float):
    if carriers >= block//2: raise ValueError("carriers must be below Nyquist bin")
    if bits_axis < 1 or bits_axis > 24: raise ValueError("bits-axis")
    cap=carriers*2*bits_axis*channels
    bits=np.unpackbits(np.frombuffer(data,dtype=np.uint8))
    blocks=(len(bits)+cap-1)//cap
    pad=blocks*cap-len(bits)
    if pad: bits=np.concatenate((bits,fixed_pad(pad)))
    weights=(1<<np.arange(bits_axis-1,-1,-1,dtype=np.int64))
    maxv=(1<<bits_axis)-1
    A=drive*block/(2*carriers*math.sqrt(2))
    lim=(1<<(sample_bits-1))-1
    out=np.empty((blocks*block,channels),dtype=np.int32)
    bins=np.arange(1,carriers+1)
    p=0
    peak=0.0
    for bi in range(blocks):
        for ch in range(channels):
            z=bits[p:p+carriers*2*bits_axis].reshape(carriers,2,bits_axis); p+=carriers*2*bits_axis
            sym=(z*weights).sum(axis=2)
            iq=(2.0*sym/maxv)-1.0
            X=np.zeros(block,dtype=np.complex128)
            vals=A*(iq[:,0]+1j*iq[:,1])
            X[bins]=vals
            X[-bins]=np.conj(vals)
            x=np.fft.ifft(X).real
            peak=max(peak,float(np.max(np.abs(x))))
            q=np.rint(np.clip(x,-1.0,1.0)*lim).astype(np.int32)
            out[bi*block:(bi+1)*block,ch]=q
    with wave.open(wav_path,"wb") as w:
        w.setnchannels(channels); w.setsampwidth(sample_bits//8); w.setframerate(sr)
        w.writeframes(pack_pcm(out.reshape(-1),sample_bits))
    duration=blocks*block/sr
    return {
        "blocks":blocks,"pad_bits":int(pad),"bits_per_event":cap,
        "bytes_per_event":cap/8.0,"duration_s":duration,
        "payload_bps":len(data)*8/duration,"nominal_capacity_bps":cap*sr/block,
        "pcm_raw_ceiling_bps":sr*sample_bits*channels,
        "pcm_utilization":((cap*sr/block)/(sr*sample_bits*channels)),
        "preclip_peak":peak,"clipped":bool(peak>1.0),"fft_drive":drive
    }

def decode(wav_path: str, nbytes: int, block: int, carriers: int, bits_axis: int, drive: float):
    with wave.open(wav_path,"rb") as w:
        channels=w.getnchannels(); sample_bits=w.getsampwidth()*8; frames=w.getnframes()
        raw=w.readframes(frames)
    q=unpack_pcm(raw,sample_bits).reshape(-1,channels)
    lim=(1<<(sample_bits-1))-1
    x=q.astype(np.float64)/lim
    if len(x)%block: raise ValueError("unaligned")
    A=drive*block/(2*carriers*math.sqrt(2))
    maxv=(1<<bits_axis)-1
    bins=np.arange(1,carriers+1)
    outbits=[]
    shifts=np.arange(bits_axis-1,-1,-1,dtype=np.int64)
    for bi in range(len(x)//block):
        for ch in range(channels):
            X=np.fft.fft(x[bi*block:(bi+1)*block,ch])
            c=X[bins]/A
            vi=np.clip(np.rint((c.real+1)*maxv/2),0,maxv).astype(np.int64)
            vq=np.clip(np.rint((c.imag+1)*maxv/2),0,maxv).astype(np.int64)
            for i in range(carriers):
                outbits.extend(((int(vi[i])>>shifts)&1).tolist())
                outbits.extend(((int(vq[i])>>shifts)&1).tolist())
    return np.packbits(np.asarray(outbits[:nbytes*8],dtype=np.uint8)).tobytes()

def pcm_sha(path: str) -> str:
    with wave.open(path,"rb") as w:
        return sha(w.readframes(w.getnframes()))

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("source"); ap.add_argument("--prefix",type=int,default=1048576)
    ap.add_argument("--sr",type=int,default=48000); ap.add_argument("--block",type=int,default=64)
    ap.add_argument("--carriers",type=int,default=31); ap.add_argument("--bits-axis",type=int,default=12)
    ap.add_argument("--channels",type=int,default=1); ap.add_argument("--sample-bits",type=int,choices=[16,24],default=16)
    ap.add_argument("--drive",type=float,default=1.4); ap.add_argument("--out-prefix",default="qam")
    a=ap.parse_args()
    data=Path(a.source).read_bytes()[:a.prefix]
    wav=a.out_prefix+".wav"; flac=a.out_prefix+".flac"; rt=a.out_prefix+".rt.wav"
    meta=encode(data,wav,a.sr,a.block,a.carriers,a.bits_axis,a.channels,a.sample_bits,a.drive)
    recovered=decode(wav,len(data),a.block,a.carriers,a.bits_axis,a.drive)
    exact=recovered==data
    wb=Path(wav).read_bytes()
    xz=lzma.compress(wb,format=lzma.FORMAT_XZ,preset=lzma.PRESET_EXTREME|9)
    Path(a.out_prefix+".wav.xz").write_bytes(xz)
    subprocess.run(["flac","-8","-f","-o",flac,wav],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    subprocess.run(["flac","-d","-f","-o",rt,flac],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    result={
        "codec":"orchestral_qam_v2","source_bytes":len(data),"source_sha256":sha(data),
        "recovered_sha256":sha(recovered),"exact":exact,
        "sr":a.sr,"block_samples":a.block,"carriers_per_channel":a.carriers,
        "bits_per_axis":a.bits_axis,"bits_per_carrier":2*a.bits_axis,
        "channels":a.channels,"sample_bits":a.sample_bits,**meta,
        "wav_bytes":os.path.getsize(wav),"flac_bytes":os.path.getsize(flac),
        "wav_xz_bytes":len(xz),"flac_pcm_exact":pcm_sha(wav)==pcm_sha(rt)
    }
    for k in ("wav_bytes","flac_bytes","wav_xz_bytes"):
        result[k.replace("_bytes","_ratio_vs_source")]=result[k]/len(data)
    print(json.dumps(result,indent=2))
    raise SystemExit(0 if exact and result["flac_pcm_exact"] else 3)

if __name__=="__main__": main()

# CANONICAL REPLICATION NOTICE (2026-09-18): predecessor/lineage artifact. Current protocol: compression/EIS_K32_FORMAL_REPLICATION_V1.md
