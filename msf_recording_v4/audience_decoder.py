#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
from msf_format import HEADER_SIZE,parse_header
from signal_basis import page_bounds,instrument_slot,remove_modifier

def listen(msf,output):
    msf=Path(msf);output=Path(output)
    with open(msf,"rb") as f:
        h=parse_header(f.read(HEADER_SIZE));alphabet=f.read(h["A"])
        if len(alphabet)!=h["A"] or len(set(alphabet))!=len(alphabet):raise ValueError("alphabet")
        payload_offset=HEADER_SIZE+h["A"]
        payload=f.read(h["payload_bytes"])
        if f.read(1):raise ValueError("trailing bytes")
    if hashlib.sha256(payload).digest()!=h["payload_sha"]:raise ValueError("payload hash")
    N,A,n,frames,sb=h["N"],h["A"],h["n"],h["frames"],h["sample_bytes"];B=A*A
    if len(payload)!=frames*sb:raise ValueError("payload geometry")
    out=bytearray(n)
    # The audience knows only the generic signal basis. Each conceptual listener
    # reads its procedural basis coefficient from the one scalar recording sample.
    slots=[instrument_slot(i,N) for i in range(N)]
    divisors=[pow(B,s) for s in slots]
    off=0
    for t in range(frames):
        sample=int.from_bytes(payload[off:off+sb],"little");off+=sb
        for i in range(N):
            y=(sample//divisors[i])%B
            pair=remove_modifier(y,i,B);fv=pair%A;rv=pair//A
            s,e=page_bounds(n,N,i);L=e-s
            if t<(L+1)//2:out[s+t]=alphabet[fv]
            if t<L//2:out[e-1-t]=alphabet[rv]
    Path(output).write_bytes(out)
    got=hashlib.sha256(out).digest()
    if got!=h["source_sha"]:raise ValueError("source hash mismatch")
    return {"recovered_bytes":n,"recovered_sha256":got.hex(),"exact_reconstruction":True,
            "decoder_input":"recorded .msf only","recorded_samples_per_timestamp":1}

def main():
    ap=argparse.ArgumentParser();ap.add_argument("msf");ap.add_argument("output")
    a=ap.parse_args();print(json.dumps(listen(a.msf,a.output),indent=2))
if __name__=="__main__":main()
