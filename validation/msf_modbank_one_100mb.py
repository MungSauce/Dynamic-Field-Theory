#!/usr/bin/env python3
from __future__ import annotations
import hashlib, json, os, sys, time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"msf_modbank"))
from codec import compose, listen, page_bounds

def sha256_file(path,chunk=8<<20):
    h=hashlib.sha256()
    with open(path,"rb") as f:
        for b in iter(lambda:f.read(chunk),b""): h.update(b)
    return h.hexdigest()

def main():
    if len(sys.argv)<3: raise SystemExit("usage: script SOURCE N")
    src=Path(sys.argv[1]); N=int(sys.argv[2])
    source_bytes=src.stat().st_size; source_sha=sha256_file(src)
    msf=Path(f"enwik100m_N{N}.msf"); rec=Path(f"recovered_N{N}.txt")
    t=time.perf_counter(); r=compose(src,msf,N); t1=time.perf_counter()
    os.remove(src)
    d=listen(msf,rec); t2=time.perf_counter()
    r.update({
      "recovered_sha256":d["recovered_sha256"],
      "exact_reconstruction":d["exact"] and d["recovered_sha256"]==source_sha,
      "source_removed_before_decode":True,
      "complete_ratio":r["complete_msf_bytes"]/source_bytes,
      "compression_percent":100*(1-r["complete_msf_bytes"]/source_bytes),
      "page_length_max":max(page_bounds(source_bytes,N,i)[1]-page_bounds(source_bytes,N,i)[0] for i in range(N)),
      "compose_seconds":t1-t,"listen_seconds":t2-t1,
      "source_sha256_expected":source_sha,
      "architecture":"MSF modifier-bank page orchestra",
    })
    Path(f"msf_modbank_N{N}_report.json").write_text(json.dumps(r,indent=2)+"\n")
    print(json.dumps(r,indent=2))
    if not r["exact_reconstruction"]: raise SystemExit(2)
if __name__=="__main__":main()
