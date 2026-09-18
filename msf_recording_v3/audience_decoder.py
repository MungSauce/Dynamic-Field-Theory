#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, json
from pathlib import Path
import numpy as np
from signal_field import inverse_fwht_rows
from msf_format import HEADER_SIZE, parse_header

def page_bounds(n,N):
    idx=np.arange(N+1,dtype=np.int64)
    return (idx*n)//N

def listen(msf,output):
    msf=Path(msf);output=Path(output)
    with open(msf,"rb") as f:
        h=parse_header(f.read(HEADER_SIZE))
        alphabet=f.read(h["A"])
        if len(alphabet)!=h["A"] or len(set(alphabet))!=len(alphabet):raise ValueError("alphabet")
        payload_offset=HEADER_SIZE+h["A"]
    if msf.stat().st_size!=payload_offset+h["payload_bytes"]:raise ValueError("size")
    # Audience sees only the recording.
    ph=hashlib.sha256()
    with open(msf,"rb") as f:
        f.seek(payload_offset)
        for b in iter(lambda:f.read(8<<20),b""):ph.update(b)
    if ph.digest()!=h["payload_sha"]:raise ValueError("payload hash")

    N,A,p,n,frames=h["N"],h["A"],h["p"],h["n"],h["frames"]
    bounds=page_bounds(n,N);lengths=np.diff(bounds)
    alphabet_arr=np.frombuffer(alphabet,dtype=np.uint8)
    out=np.memmap(output,dtype=np.uint8,mode="w+",shape=(n,))
    samples=np.memmap(msf,dtype="<u2",mode="r",offset=payload_offset,shape=(frames,N))
    bf=h["block_frames"]
    try:
        for t0 in range(0,frames,bf):
            t1=min(frames,t0+bf)
            block=np.asarray(samples[t0:t1],dtype=np.int64)
            I=block%p;Q=block//p
            # Audience bank: Walsh correlation isolates each instrument from
            # the same recorded composite samples.
            forward=inverse_fwht_rows(I,p)
            reverse=inverse_fwht_rows(Q,p)
            for i in range(N):
                s=int(bounds[i]);e=int(bounds[i+1]);L=e-s
                f1=min(t1,(L+1)//2)
                if f1>t0:
                    vals=forward[:f1-t0,i]
                    if np.any(vals>=A):raise ValueError(f"invalid forward note instrument={i}")
                    out[s+t0:s+f1]=alphabet_arr[vals]
                r1=min(t1,L//2)
                if r1>t0:
                    vals=reverse[:r1-t0,i]
                    if np.any(vals>=A):raise ValueError(f"invalid reverse note instrument={i}")
                    out[e-r1:e-t0]=alphabet_arr[vals][::-1]
        out.flush()
    finally:
        del samples;del out
    rh=hashlib.sha256()
    with open(output,"rb") as f:
        for b in iter(lambda:f.read(8<<20),b""):rh.update(b)
    got=rh.digest()
    if got!=h["source_sha"]:raise ValueError("recovered source hash mismatch")
    return {"recovered_bytes":n,"recovered_sha256":got.hex(),"exact_reconstruction":True,
            "decoder_input":"recorded .msf only","composer_state_used":False}

def main():
    ap=argparse.ArgumentParser();ap.add_argument("msf");ap.add_argument("output")
    a=ap.parse_args();print(json.dumps(listen(a.msf,a.output),indent=2))
if __name__=="__main__":main()
