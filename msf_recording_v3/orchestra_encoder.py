#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, json, mmap, os
from pathlib import Path
import numpy as np
from signal_field import next_prime_at_least, require_instruments, fwht_rows
from msf_format import make_header, MAX_ALPHABET

DEFAULT_BLOCK_FRAMES=512

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
    idx=np.arange(N+1,dtype=np.int64)
    return (idx*n)//N

def compose(source,msf,N=128,block_frames=DEFAULT_BLOCK_FRAMES):
    require_instruments(N)
    source=Path(source);msf=Path(msf)
    n,alphabet,source_sha=scan(source);A=len(alphabet);p=next_prime_at_least(A)
    if p*p>65536:raise ValueError("alphabet/modulus exceeds 16-bit complex sample")
    rank=np.zeros(256,dtype=np.int64)
    for j,b in enumerate(alphabet):rank[b]=j
    bounds=page_bounds(n,N);lengths=np.diff(bounds)
    frames=int(np.max((lengths+1)//2))
    payload_tmp=Path(str(msf)+".payload.tmp")
    ph=hashlib.sha256();payload_bytes=0;sample_count=0

    src=np.memmap(source,dtype=np.uint8,mode="r")
    try:
        with open(payload_tmp,"wb") as out:
            for t0 in range(0,frames,block_frames):
                t1=min(frames,t0+block_frames);B=t1-t0
                forward=np.zeros((B,N),dtype=np.int64)
                reverse=np.zeros((B,N),dtype=np.int64)
                for i in range(N):
                    s=int(bounds[i]);e=int(bounds[i+1]);L=e-s
                    f1=min(t1,(L+1)//2)
                    if f1>t0:
                        forward[:f1-t0,i]=rank[np.asarray(src[s+t0:s+f1])]
                    r1=min(t1,L//2)
                    if r1>t0:
                        # rows t0..r1-1 correspond to e-1-t.
                        reverse[:r1-t0,i]=rank[np.asarray(src[e-r1:e-t0][::-1])]
                # Orchestra: every instrument's note pair is multiplied by its
                # procedural Walsh timbre, then all instruments are summed.
                I=fwht_rows(forward,p)
                Q=fwht_rows(reverse,p)
                complex_samples=(I + p*Q).astype("<u2",copy=False)
                raw=complex_samples.tobytes(order="C")
                out.write(raw);ph.update(raw)
                payload_bytes+=len(raw);sample_count+=complex_samples.size
    finally:
        del src

    payload_sha=ph.digest()
    header=make_header(N=N,A=A,p=p,block_frames=block_frames,n=n,frames=frames,
                       sample_count=sample_count,payload_bytes=payload_bytes,
                       source_sha=source_sha,payload_sha=payload_sha)
    with open(msf,"wb") as out,open(payload_tmp,"rb") as pf:
        out.write(header);out.write(alphabet)
        for c in iter(lambda:pf.read(8<<20),b""):out.write(c)
    payload_tmp.unlink()
    result={
      "format":"MSFREC03","source_bytes":n,"instrument_count":N,"page_count":N,
      "alphabet_size":A,"shared_directional_note_lexicon":2*A,
      "symbols_per_full_timestamp":2*N,"frame_count":frames,
      "sample_modulus":p,"complex_samples":sample_count,
      "payload_bytes":payload_bytes,"complete_msf_bytes":msf.stat().st_size,
      "bytes_per_timestamp":payload_bytes/frames,
      "source_bytes_per_timestamp":n/frames,
      "source_sha256":source_sha.hex(),"payload_sha256":payload_sha.hex(),
      "recording_semantics":"N complex machine samples/timestamp; Walsh-modified instruments are superposed; no decoded instrument-state stream retained",
      "composer_required_after_recording":False
    }
    return result

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("source");ap.add_argument("msf",nargs="?")
    ap.add_argument("-n","--instruments",type=int,default=128)
    a=ap.parse_args();out=Path(a.msf) if a.msf else Path(a.source).with_suffix(".msf")
    print(json.dumps(compose(a.source,out,a.instruments),indent=2))
if __name__=="__main__":main()
