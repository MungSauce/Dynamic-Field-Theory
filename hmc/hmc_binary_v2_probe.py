#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, json, lzma, os, pathlib
import numpy as np

def sha(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("source")
    ap.add_argument("--prefix",type=int,default=8*1024*1024)
    ap.add_argument("--out",default="hmc_binary_v2")
    a=ap.parse_args()

    src=pathlib.Path(a.source).read_bytes()[:a.prefix]
    arr=np.frombuffer(src,dtype=np.uint8)
    syms=np.unique(arr)
    # Exact HMC semantics: each page has exactly N binary slots.
    # We omit one page because one-hot completeness makes the final page implicit.
    counts=np.bincount(arr,minlength=256)
    implicit=int(syms[np.argmin([counts[s] for s in syms])])  # deterministic: rarest page omitted
    explicit=[int(s) for s in syms if int(s)!=implicit]

    page_payloads=[]
    reconstructed=np.full(len(arr),implicit,dtype=np.uint8)
    page_rows=[]
    concat=bytearray()

    for s in explicit:
        bits=(arr==s).astype(np.uint8)
        packed=np.packbits(bits,bitorder="little").tobytes()
        comp=lzma.compress(packed,format=lzma.FORMAT_XZ,preset=lzma.PRESET_EXTREME|9)
        page_payloads.append((s,packed,comp))
        concat += packed
        recovered_bits=np.unpackbits(np.frombuffer(packed,dtype=np.uint8),bitorder="little")[:len(arr)]
        reconstructed[recovered_bits.astype(bool)]=s
        page_rows.append({
            "symbol":s,
            "ones":int(bits.sum()),
            "zeros":int(len(arr)-bits.sum()),
            "raw_bitmap_bytes":len(packed),
            "xz_bytes":len(comp),
            "xz_ratio_vs_bitmap":len(comp)/len(packed) if packed else 1.0,
        })

    concat_xz=lzma.compress(bytes(concat),format=lzma.FORMAT_XZ,preset=lzma.PRESET_EXTREME|9)
    direct_xz=lzma.compress(src,format=lzma.FORMAT_XZ,preset=lzma.PRESET_EXTREME|9)
    per_page_xz=sum(len(c) for _,_,c in page_payloads)
    exact=reconstructed.tobytes()==src

    meta_bytes=16 + len(explicit)*2  # conservative simple symbol/count-ish header placeholder
    result={
        "codec":"HMC-binary-v2-probe",
        "source_bytes":len(src),
        "source_sha256":sha(src),
        "distinct_symbols":int(len(syms)),
        "explicit_pages":len(explicit),
        "implicit_symbol":implicit,
        "logical_slots_per_page":len(src),
        "logical_raw_bitmap_stack_bytes":len(explicit)*((len(src)+7)//8),
        "per_page_xz_payload_bytes":per_page_xz,
        "per_page_xz_complete_estimate_bytes":per_page_xz+meta_bytes,
        "concatenated_bitmap_xz_bytes":len(concat_xz),
        "direct_source_xz_bytes":len(direct_xz),
        "per_page_xz_ratio_vs_source":(per_page_xz+meta_bytes)/len(src),
        "concat_bitmap_xz_ratio_vs_source":len(concat_xz)/len(src),
        "direct_xz_ratio_vs_source":len(direct_xz)/len(src),
        "exact_reconstruction":exact,
        "recovered_sha256":sha(reconstructed.tobytes()),
        "pages":page_rows,
    }
    pathlib.Path(a.out+".json").write_text(json.dumps(result,indent=2))
    print(json.dumps({k:v for k,v in result.items() if k!="pages"},indent=2))
    raise SystemExit(0 if exact else 3)

if __name__=="__main__":
    main()

# CANONICAL REPLICATION NOTICE (2026-09-18): predecessor/lineage artifact. Current protocol: compression/EIS_K32_FORMAL_REPLICATION_V1.md
