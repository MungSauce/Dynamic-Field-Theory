#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
import numpy as np
from signal_field import next_prime_at_least,require_instruments,fwht_rows
from signal_packing import pack_samples
from msf_format import make_header,MAX_ALPHABET

BLOCK_FRAMES=512

def scan(path):
    seen=bytearray(256);h=hashlib.sha256();n=0
    with open(path,"rb") as f:
        for c in iter(lambda:f.read(8<<20),b""):
            n+=len(c);h.update(c)
            for b in set(c):seen[b]=1
    alphabet=bytes(i for i,v in enumerate(seen) if v)
    if not alphabet:raise ValueError("empty source")
    if len(alphabet)>MAX_ALPHABET:raise ValueError(f"alphabet {len(alphabet)} > {MAX_ALPHABET}")
    return n,alphabet,h.digest()

def page_bounds(n,N):
    return (np.arange(N+1,dtype=np.int64)*n)//N

def compose(source,msf,N=128):
    require_instruments(N);source=Path(source);msf=Path(msf)
    n,alphabet,source_sha=scan(source);A=len(alphabet);p=next_prime_at_least(A);signal_base=p*p
    rank=np.zeros(256,dtype=np.int64)
    for j,b in enumerate(alphabet):rank[b]=j
    bounds=page_bounds(n,N);lens=np.diff(bounds);frames=int(np.max((lens+1)//2))
    src=np.memmap(source,dtype=np.uint8,mode="r")
    tmp=Path(str(msf)+".payload.tmp");ph=hashlib.sha256();pbytes=0;samples=0
    try:
        with open(tmp,"wb") as out:
            for t0 in range(0,frames,BLOCK_FRAMES):
                t1=min(frames,t0+BLOCK_FRAMES);B=t1-t0
                F=np.zeros((B,N),dtype=np.int64);R=np.zeros((B,N),dtype=np.int64)
                for i in range(N):
                    s=int(bounds[i]);e=int(bounds[i+1]);L=e-s
                    f1=min(t1,(L+1)//2)
                    if f1>t0:F[:f1-t0,i]=rank[np.asarray(src[s+t0:s+f1])]
                    r1=min(t1,L//2)
                    if r1>t0:R[:r1-t0,i]=rank[np.asarray(src[e-r1:e-t0][::-1])]
                I=fwht_rows(F,p);Q=fwht_rows(R,p)
                signal=(I+p*Q).astype(np.uint16,copy=False).reshape(-1)
                raw=pack_samples(signal,signal_base)
                out.write(raw);ph.update(raw);pbytes+=len(raw);samples+=len(signal)
    finally:
        del src
    payload_sha=ph.digest()
    header=make_header(N=N,A=A,p=p,block_frames=BLOCK_FRAMES,n=n,frames=frames,
                       sample_count=samples,payload_bytes=pbytes,source_sha=source_sha,payload_sha=payload_sha)
    with open(msf,"wb") as out,open(tmp,"rb") as pf:
        out.write(header);out.write(alphabet)
        for c in iter(lambda:pf.read(8<<20),b""):out.write(c)
    tmp.unlink()
    return {
      "format":"MSFR31A1","source_bytes":n,"instrument_count":N,"page_count":N,
      "alphabet_size":A,"shared_directional_note_lexicon":2*A,
      "symbols_per_full_timestamp":2*N,"frame_count":frames,
      "signal_sample_alphabet":signal_base,"signal_sample_count":samples,
      "signal_payload_bytes":pbytes,"complete_msf_bytes":msf.stat().st_size,
      "signal_ratio":pbytes/n,"signal_compression_percent":100*(1-pbytes/n),
      "source_sha256":source_sha.hex(),"payload_sha256":payload_sha.hex(),
      "recording_semantics":"radix-packed literal composite signal samples only",
      "composer_required_after_recording":False
    }

def main():
    ap=argparse.ArgumentParser();ap.add_argument("source");ap.add_argument("msf");ap.add_argument("-n","--instruments",type=int,default=128)
    a=ap.parse_args();print(json.dumps(compose(a.source,a.msf,a.instruments),indent=2))
if __name__=="__main__":main()
