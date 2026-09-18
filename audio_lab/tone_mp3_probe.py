#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, math, os, pathlib, struct, subprocess, sys, wave
from array import array

def sha256(p):
    h=hashlib.sha256()
    with open(p,'rb') as f:
        for b in iter(lambda:f.read(1<<20),b''): h.update(b)
    return h.hexdigest()

def distinct_map(data: bytes):
    vals=sorted(set(data))
    return vals,{b:i for i,b in enumerate(vals)}

def write_amp_wav(data: bytes, vals, rank, out, sr, samples_per_symbol, amp_floor=0.10, amp_ceil=0.95, carrier_hz=3000.0):
    # One character = one amplitude level of the same carrier tone.
    # Calibration preamble: each legal symbol once, then a sync silence block.
    seq = bytes(vals) + data
    frames=array('h')
    phases=[math.sin(2*math.pi*carrier_hz*n/sr) for n in range(samples_per_symbol)]
    denom=max(1,len(vals)-1)
    for b in seq:
        r=rank[b]
        amp=amp_floor+(amp_ceil-amp_floor)*(r/denom)
        scale=int(amp*30000)
        for s in phases:
            frames.append(max(-32767,min(32767,int(scale*s))))
    with wave.open(out,'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(sr)
        w.writeframes(frames.tobytes())
    return len(vals)

def write_freq_wav(data: bytes, vals, rank, out, sr, samples_per_symbol, lo=500.0, hi=10000.0):
    # One character = one deterministic pure tone, selected from 206 frequencies.
    seq=bytes(vals)+data
    frames=array('h')
    denom=max(1,len(vals)-1)
    for b in seq:
        r=rank[b]; f=lo+(hi-lo)*(r/denom)
        for n in range(samples_per_symbol):
            # Hann envelope reduces clicks and MP3 transient splatter.
            env=0.5-0.5*math.cos(2*math.pi*(n+0.5)/samples_per_symbol)
            frames.append(int(28000*env*math.sin(2*math.pi*f*n/sr)))
    with wave.open(out,'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(sr)
        w.writeframes(frames.tobytes())
    return len(vals)

def ffmpeg_mp3(wav,mp3,bitrate):
    subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-i',wav,'-ac','1','-b:a',f'{bitrate}k',mp3],check=True)

def decode_pcm(mp3,raw,sr):
    subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-i',mp3,'-f','s16le','-acodec','pcm_s16le','-ac','1','-ar',str(sr),raw],check=True)

def read_pcm(raw):
    a=array('h'); a.frombytes(open(raw,'rb').read()); return a

def amp_decode(samples, vals, sr, sps, preamble):
    total_symbols=len(samples)//sps
    if total_symbols < preamble: raise RuntimeError('short decoded audio')
    # Estimate RMS per block. Use calibration preamble emitted in rank order to
    # derive actual MP3-distorted centroids, avoiding assumptions about gain.
    rms=[]
    for i in range(total_symbols):
        block=samples[i*sps:(i+1)*sps]
        if not block: break
        rms.append(math.sqrt(sum(float(x)*x for x in block)/len(block)))
    cent=rms[:preamble]
    out=bytearray()
    for x in rms[preamble:]:
        k=min(range(len(cent)), key=lambda j:abs(x-cent[j]))
        out.append(vals[k])
    return bytes(out)

def freq_feature(block, sr, freqs):
    # Correlation against each legal pure tone; exact decoder chooses strongest.
    N=len(block)
    best_i=0; best=-1.0
    for i,f in enumerate(freqs):
        c=s=0.0
        for n,x in enumerate(block):
            ang=2*math.pi*f*n/sr
            c += x*math.cos(ang); s += x*math.sin(ang)
        e=c*c+s*s
        if e>best: best=e;best_i=i
    return best_i

def freq_decode(samples, vals, sr, sps, preamble, lo=500.0, hi=10000.0):
    total_symbols=len(samples)//sps
    denom=max(1,len(vals)-1)
    freqs=[lo+(hi-lo)*(r/denom) for r in range(len(vals))]
    out=bytearray()
    # skip calibration preamble; frequencies themselves are fixed.
    for i in range(preamble,total_symbols):
        block=samples[i*sps:(i+1)*sps]
        out.append(vals[freq_feature(block,sr,freqs)])
    return bytes(out)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('mode',choices=['amp','freq'])
    ap.add_argument('source')
    ap.add_argument('--prefix',type=int,default=65536)
    ap.add_argument('--sr',type=int,default=24000)
    ap.add_argument('--sps',type=int,required=True)
    ap.add_argument('--bitrate',type=int,required=True)
    ap.add_argument('--out-prefix',required=True)
    a=ap.parse_args()

    data=open(a.source,'rb').read(a.prefix)
    vals,rank=distinct_map(data)
    wav=a.out_prefix+'.wav'; mp3=a.out_prefix+'.mp3'; raw=a.out_prefix+'.pcm'
    if a.mode=='amp': pre=write_amp_wav(data,vals,rank,wav,a.sr,a.sps)
    else: pre=write_freq_wav(data,vals,rank,wav,a.sr,a.sps)
    ffmpeg_mp3(wav,mp3,a.bitrate)
    decode_pcm(mp3,raw,a.sr)
    sam=read_pcm(raw)
    if a.mode=='amp': recovered=amp_decode(sam,vals,a.sr,a.sps,pre)
    else: recovered=freq_decode(sam,vals,a.sr,a.sps,pre)
    recovered=recovered[:len(data)]
    exact=recovered==data
    errs=sum(x!=y for x,y in zip(recovered,data)) + abs(len(recovered)-len(data))
    seconds=(len(data)+pre)*a.sps/a.sr
    full_seconds=(1_000_000_000+pre)*a.sps/a.sr
    full_est_bytes=a.bitrate*1000/8*full_seconds
    print(f'MODE={a.mode}')
    print(f'PREFIX_BYTES={len(data)}')
    print(f'DISTINCT={len(vals)}')
    print(f'SR={a.sr}')
    print(f'SPS={a.sps}')
    print(f'BITRATE_KBPS={a.bitrate}')
    print(f'MP3_BYTES={os.path.getsize(mp3)}')
    print(f'PREFIX_SECONDS={seconds:.6f}')
    print(f'ERRORS={errs}')
    print(f'EXACT={"PASS" if exact else "FAIL"}')
    print(f'FULL_GB_MP3_EST_BYTES_AT_SAME_TIMING={int(full_est_bytes)}')
    print(f'FULL_GB_MP3_EST_MB={full_est_bytes/1e6:.3f}')
    if exact:
        print('RECOVERED_SHA256='+hashlib.sha256(recovered).hexdigest())
    return 0 if exact else 3

if __name__=='__main__':
    raise SystemExit(main())
