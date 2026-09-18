#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json,mmap,os
from pathlib import Path
from msf_format import make_header,MAX_ALPHABET
from signal_basis import page_bounds,frame_count,apply_modifier,sample_bytes_for,mix_field,spectral_permute

def scan(path):
    seen=bytearray(256);h=hashlib.sha256();n=0
    with open(path,"rb") as f:
        for c in iter(lambda:f.read(8<<20),b""):
            n+=len(c);h.update(c)
            for b in set(c):seen[b]=1
    alphabet=bytes(i for i,v in enumerate(seen) if v)
    if not alphabet: raise ValueError("empty source")
    if len(alphabet)>MAX_ALPHABET: raise ValueError("alphabet exceeds 206")
    return n,alphabet,h.digest()

def compose(source,msf,N=128):
    if N<1 or N>4096 or N&(N-1): raise ValueError("N must be power of two")
    source=Path(source);msf=Path(msf)
    n,alphabet,source_sha=scan(source);A=len(alphabet);B=A*A
    ranks=[0]*256
    for j,b in enumerate(alphabet):ranks[b]=j
    frames=frame_count(n,N);sb=sample_bytes_for(B,N)
    payload_tmp=Path(str(msf)+".payload.tmp")
    ph=hashlib.sha256();payload_bytes=0
     with open(source,"rb") as sf,open(payload_tmp,"wb") as out:
        mm=mmap.mmap(sf.fileno(),0,access=mmap.ACCESS_READ)
        try:
            for t in range(frames):
                instrument_states=[0]*N
                for i in range(N):
                    s,e=page_bounds(n,N,i);L=e-s
                    fv=ranks[mm[s+t]] if t<(L+1)//2 else 0
                    rv=ranks[mm[e-1-t]] if t<L//2 else 0
                    pair=fv+A*rv
                    instrument_states[i]=apply_modifier(pair,i,B)

                # Audio-adjacent signal stage: procedural timbres first, then a
                # reversible full-orchestra filter-bank mix. The recording has
                # no page-aligned coefficients. Only the resulting composite
                # signal sample is serialized.
                mixed=spectral_permute(mix_field(instrument_states,B))
                sample=0
                for d in reversed(mixed):
                    sample=sample*B+d
                raw=sample.to_bytes(sb,"little")
                out.write(raw);ph.update(raw);payload_bytes+=sb
        finally:mm.close()
    payload_sha=ph.digest()
    header=make_header(N,A,sb,n,frames,payload_bytes,source_sha,payload_sha)
    with open(msf,"wb") as out,open(payload_tmp,"rb") as pf:
        out.write(header);out.write(alphabet)
        for c in iter(lambda:pf.read(8<<20),b""):out.write(c)
    payload_tmp.unlink()
    return {
      "format":"MSFREC04","source_bytes":n,"instrument_count":N,"page_count":N,
      "alphabet_size":A,"shared_directional_note_lexicon":2*A,
      "symbols_per_full_timestamp":2*N,"frame_count":frames,
      "recorded_samples":frames,"recorded_samples_per_timestamp":1,
      "sample_bytes":sb,"payload_bytes":payload_bytes,
      "complete_msf_bytes":msf.stat().st_size,
      "signal_ratio":payload_bytes/n,
      "signal_compression_percent":100*(1-payload_bytes/n),
      "source_sha256":source_sha.hex(),"payload_sha256":payload_sha.hex(),
      "recording_semantics":"one high-precision composite signal sample per timestamp after procedural timbre + reversible full-orchestra filter-bank mixing",
      "composer_required_after_recording":False
    }

def main():
    ap=argparse.ArgumentParser();ap.add_argument("source");ap.add_argument("msf")
    ap.add_argument("-n","--instruments",type=int,default=128)
    a=ap.parse_args();print(json.dumps(compose(a.source,a.msf,a.instruments),indent=2))
if __name__=="__main__":main()
