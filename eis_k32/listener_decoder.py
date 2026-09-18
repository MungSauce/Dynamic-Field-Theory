#!/usr/bin/env python3
from __future__ import annotations
import argparse
from pathlib import Path
import numpy as np
from eis_k32_format import (
    LANES,SYMBOLS_PER_LANE,BASIS_ID_DFT32,MODULATION_ID_QAM16X16,
    carrier_bins,carrier_scale,qam16x16_decode,unpack_samples,sha256_bytes,read_file
)

def listen(path:str,drive:float=1.0)->bytes:
    h,payload=read_file(path)
    if h.lanes!=LANES or h.symbols_per_lane!=SYMBOLS_PER_LANE:
        raise ValueError("unsupported lane/symbol geometry")
    if h.basis_id!=BASIS_ID_DFT32 or h.modulation_id!=MODULATION_ID_QAM16X16:
        raise ValueError("unsupported basis/modulation")
    q=unpack_samples(payload,h.sample_bits)
    expected=h.frame_count*h.frame_samples
    if len(q)!=expected: raise ValueError("frame/sample count mismatch")
    x=q.astype(np.float64)/((1<<(h.sample_bits-1))-1)
    x=x.reshape(h.frame_count,h.frame_samples)
    bins=carrier_bins(h.frame_samples,h.lanes)
    A=carrier_scale(h.frame_samples,drive,h.lanes)
    out=np.empty((h.frame_count,h.lanes),dtype=np.uint8)
    batch=2048
    for p in range(0,h.frame_count,batch):
        z=np.fft.fft(x[p:p+batch],axis=1)[:,bins]/A
        out[p:p+len(z)]=qam16x16_decode(z.real,z.imag)
    data=out.reshape(-1).tobytes()[:h.source_length]
    if sha256_bytes(data)!=h.source_sha256:
        raise ValueError("decoded source SHA-256 mismatch")
    return data

def main():
    ap=argparse.ArgumentParser(description="EIS-K32 Listener/Decoder")
    ap.add_argument("input");ap.add_argument("output")
    ap.add_argument("--drive",type=float,default=1.0)
    a=ap.parse_args()
    data=listen(a.input,a.drive)
    Path(a.output).write_bytes(data)
    print(f"DECODED_BYTES={len(data)}")
    print("SOURCE_SHA256=PASS")

if __name__=="__main__": main()
