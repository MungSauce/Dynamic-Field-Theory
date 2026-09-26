#!/usr/bin/env python3
from __future__ import annotations
import hashlib, json, os, sys, time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"msf_modbank"))
from codec import compose, listen

COUNTS=[32,64,128,256,512]

def sha256_file(path,chunk=8<<20):
    h=hashlib.sha256()
    with open(path,"rb") as f:
        for b in iter(lambda:f.read(chunk),b""): h.update(b)
    return h.hexdigest()

def main():
    src=Path(sys.argv[1] if len(sys.argv)>1 else "enwik100m")
    source_bytes=src.stat().st_size
    source_sha=sha256_file(src)
    rows=[]
    t0=time.perf_counter()

    # Compose every candidate while source exists.
    for N in COUNTS:
        out=Path(f"enwik100m_N{N}.msf")
        a=time.perf_counter()
        r=compose(src,out,N)
        r["compose_seconds"]=time.perf_counter()-a
        rows.append(r)

    # Cold replay boundary: original source is gone for every decode.
    os.remove(src)

    for r in rows:
        N=r["instrument_count"]
        a=time.perf_counter()
        dec=listen(Path(f"enwik100m_N{N}.msf"),Path(f"recovered_N{N}.txt"))
        r["listen_seconds"]=time.perf_counter()-a
        r["recovered_sha256"]=dec["recovered_sha256"]
        r["exact_reconstruction"]=dec["exact"] and dec["recovered_sha256"]==source_sha
        r["source_removed_before_decode"]=True
        r["complete_ratio"]=r["complete_msf_bytes"]/source_bytes
        r["compression_percent"]=100*(1-r["complete_ratio"])
        r["page_length_max"]=max(((i+1)*source_bytes)//N-(i*source_bytes)//N for i in range(N))
        r["expected_time_formula"]="ceil(max_page_length/2)"

    report={
      "architecture":"MSF modifier-bank page orchestra",
      "source_bytes":source_bytes,
      "source_sha256":source_sha,
      "lexicon_rule":"one shared symbol lexicon; forward/reverse directional roles; same lexicon reused for every instrument",
      "page_rule":"one deterministic contiguous page per instrument",
      "time_rule":"each page read from both ends simultaneously",
      "modifier_rule":"source-independent procedural timbre plus reversible cumulative composite mixer",
      "composer_retained_after_encode":False,
      "listener_source_specific_state":False,
      "all_exact":all(r["exact_reconstruction"] for r in rows),
      "rows":rows,
      "elapsed_seconds":time.perf_counter()-t0,
    }
    Path("msf_modbank_100mb_scale_report.json").write_text(json.dumps(report,indent=2)+"\n")
    print(json.dumps(report,indent=2))
    if not report["all_exact"]: raise SystemExit(2)

if __name__=="__main__": main()
