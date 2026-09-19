#!/usr/bin/env python3
from __future__ import annotations
import argparse, math
from pathlib import Path
import numpy as np
from eis_k32_format import (
    Header,LANES,SYMBOLS_PER_LANE,BASIS_ID_DFT32,MODULATION_ID_QAM16X16,
    FLAG_FINAL_PAD,carrier_bins,carrier_scale,qam16x16_encode,pack_samples,
    sha256_bytes,write_file
)

def compose(data:bytes,frame_samples:int=96,sample_bits:int=16,drive:float=1.0):
    if sample_bits!=16: raise ValueError("reference composer supports 16-bit EIS-K32 payloads")
    original=data
    pad=(-len(data))%LANES
    if pad: data=data+bytes(pad)
    u=np.frombuffer(data,dtype=np.uint8).reshape(-1,LANES)
    I,Q=qam16x16_encode(u)
    bins=carrier_bins(frame_samples,LANES)
    A=carrier_scale(frame_samples,drive,LANES)
    lim=(1<<(sample_bits-1))-1
    out=np.empty((len(u),frame_samples),dtype=np.int32)
    peak=0.0
    batch=2048
    for p in range(0,len(u),batch):
        ii=I[p:p+batch]; qq=Q[p:p+batch]
        X=np.zeros((len(ii),frame_samples),dtype=np.complex128)
        vals=A*(ii+1j*qq)
        X[:,bins]=vals
        X[:,-bins]=np.conj(vals)
        x=np.fft.ifft(X,axis=1).real
        peak=max(peak,float(np.max(np.abs(x))))
        if peak>1.0:
            raise ValueError(f"composite signal clips at peak {peak:.6f}; lower drive")
        out[p:p+len(ii)]=np.rint(x*lim).astype(np.int32)
    payload=pack_samples(out,sample_bits)
    h=Header(
        flags=FLAG_FINAL_PAD if pad else 0,
        lanes=LANES,symbols_per_lane=SYMBOLS_PER_LANE,
        frame_samples=frame_samples,sample_bits=sample_bits,
        basis_id=BASIS_ID_DFT32,modulation_id=MODULATION_ID_QAM16X16,
        source_length=len(original),frame_count=len(u),payload_bytes=len(payload),
        source_sha256=sha256_bytes(original),payload_sha256=sha256_bytes(payload)
    )
    return h,payload,{"pad_bytes":pad,"prequant_peak":peak,"bins":bins.tolist(),"drive":drive}

def main():
    ap=argparse.ArgumentParser(description="EIS-K32 Composer/Encoder")
    ap.add_argument("source");ap.add_argument("output")
    ap.add_argument("--frame-samples",type=int,default=96)
    ap.add_argument("--sample-bits",type=int,default=16,choices=[16])
    ap.add_argument("--drive",type=float,default=1.0)
    a=ap.parse_args()
    data=Path(a.source).read_bytes()
    h,payload,meta=compose(data,a.frame_samples,a.sample_bits,a.drive)
    write_file(a.output,h,payload)
    print(f"SOURCE_BYTES={len(data)}")
    print(f"EIS_BYTES={128+len(payload)}")
    print(f"FRAMES={h.frame_count}")
    print(f"LANES={h.lanes}")
    print(f"FRAME_SAMPLES={h.frame_samples}")
    print(f"PREQUANT_PEAK={meta['prequant_peak']:.9f}")

if __name__=="__main__": main()
