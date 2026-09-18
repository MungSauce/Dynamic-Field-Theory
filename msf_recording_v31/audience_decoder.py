#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
import numpy as np
from signal_field import inverse_fwht_rows
from signal_packing import unpack_samples,packed_bytes_for_samples
from msf_format import HEADER_SIZE,parse_header

def page_bounds(n,N):
    return (np.arange(N+1,dtype=np.int64)*n)//N

def listen(msf,output):
    msf=Path(msf);output=Path(output)
    with open(msf,"rb") as f:
        h=parse_header(f.read(HEADER_SIZE));alphabet=f.read(h["A"]);payload_offset=HEADER_SIZE+h["A"]
    if len(alphabet)!=h["A"] or len(set(alphabet))!=len(alphabet):raise ValueError("alphabet")
    if msf.stat().st_size!=payload_offset+h["payload_bytes"]:raise ValueError("size")
    ph=hashlib.sha256()
    with open(msf,"rb") as f:
        f.seek(payload_offset)
        for b in iter(lambda:f.read(8<<20),b""):ph.update(b)
    if ph.digest()!=h["payload_sha"]:raise ValueError("payload hash")

    N,A,p,n,frames=h["N"],h["A"],h["p"],h["n"],h["frames"];signal_base=p*p
    bounds=page_bounds(n,N);lens=np.diff(bounds);alphabet_arr=np.frombuffer(alphabet,dtype=np.uint8)
    out=np.memmap(output,dtype=np.uint8,mode="w+",shape=(n,))
    bf=h["block_frames"];left_samples=h["sample_count"]
    with open(msf,"rb") as f:
        f.seek(payload_offset)
        for t0 in range(0,frames,bf):
            t1=min(frames,t0+bf);count=(t1-t0)*N
            nbytes=packed_bytes_for_samples(count)
            raw=f.read(nbytes)
            if len(raw)!=nbytes:raise ValueError("short signal payload")
            signal=unpack_samples(raw,signal_base,count).astype(np.int64).reshape(t1-t0,N)
            I=signal%p;Q=signal//p
            F=inverse_fwht_rows(I,p);R=inverse_fwht_rows(Q,p)
            for i in range(N):
                s=int(bounds[i]);e=int(bounds[i+1]);L=e-s
                f1=min(t1,(L+1)//2)
                if f1>t0:
                    vals=F[:f1-t0,i]
                    if np.any(vals>=A):raise ValueError("invalid forward note")
                    out[s+t0:s+f1]=alphabet_arr[vals]
                r1=min(t1,L//2)
                if r1>t0:
                    vals=R[:r1-t0,i]
                    if np.any(vals>=A):raise ValueError("invalid reverse note")
                    out[e-r1:e-t0]=alphabet_arr[vals][::-1]
            left_samples-=count
        if f.read(1):raise ValueError("trailing payload")
    out.flush();del out
    rh=hashlib.sha256()
    with open(output,"rb") as f:
        for b in iter(lambda:f.read(8<<20),b""):rh.update(b)
    got=rh.digest()
    if got!=h["source_sha"]:raise ValueError("source hash")
    return {"recovered_bytes":n,"recovered_sha256":got.hex(),"exact_reconstruction":True,
            "decoder_input":"packed recorded signal only","composer_state_used":False}

def main():
    ap=argparse.ArgumentParser();ap.add_argument("msf");ap.add_argument("output")
    a=ap.parse_args();print(json.dumps(listen(a.msf,a.output),indent=2))
if __name__=="__main__":main()
