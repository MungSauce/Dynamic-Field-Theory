#!/usr/bin/env python3
from __future__ import annotations
import hashlib, json, os, pathlib, subprocess, sys, time, traceback

ROOT=pathlib.Path(__file__).resolve().parent
TOOLS=ROOT/"tools"
EXPECTED_BYTES=1_000_000_000
EXPECTED_SHA="159b85351e5f76e60cbe32e04c677847a9ecba3adc79addab6f4c6c7aa3744bc"
EXPECTED_MD5="e206c3450ac99950df65bf70ef61a12d"

def digest(path, alg):
    h=getattr(hashlib,alg)()
    with open(path,"rb") as f:
        for b in iter(lambda:f.read(8<<20),b""): h.update(b)
    return h.hexdigest()

def verify(path):
    p=pathlib.Path(path)
    return p.exists() and p.stat().st_size==EXPECTED_BYTES and digest(p,"sha256")==EXPECTED_SHA and digest(p,"md5")==EXPECTED_MD5

def run(cmd, **kw):
    t=time.time(); subprocess.run([str(x) for x in cmd],check=True,**kw); return time.time()-t

def compile_cpp(src,out):
    run(["g++","-O3","-std=c++17",src,"-o",out])
    subprocess.run(["strip",out],check=False)
    return os.path.getsize(out)

def encode_wrap(kind, inp, out):
    t=time.time()
    if kind=="xz":
        with open(out,"wb") as f: subprocess.run(["xz","-9e","--threads=1","-c",inp],check=True,stdout=f,stderr=subprocess.DEVNULL)
    elif kind=="zstd":
        subprocess.run(["zstd","-q","--ultra","-22","-T1","-f",inp,"-o",out],check=True)
    else: raise ValueError(kind)
    return time.time()-t

def decode_wrap(kind, inp, out):
    t=time.time()
    if kind=="xz":
        with open(out,"wb") as f: subprocess.run(["xz","-dc",inp],check=True,stdout=f,stderr=subprocess.DEVNULL)
    elif kind=="zstd":
        subprocess.run(["zstd","-q","-d","-f",inp,"-o",out],check=True)
    return time.time()-t

def cold_verify(src, decode_cmd):
    hidden=src+".hidden"; rec="candidate.recovered"
    if os.path.exists(rec): os.remove(rec)
    os.replace(src,hidden)
    try:
        t=run(decode_cmd(rec),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        ok=verify(rec)
    finally:
        os.replace(hidden,src)
        if os.path.exists(rec): os.remove(rec)
    return ok,t

def record(rows,name,carrier,decoder_bytes,elapsed,chain,config,exact,error=None):
    cb=os.path.getsize(carrier) if carrier and os.path.exists(carrier) else None
    rows.append({
      "name":name,"carrier_bytes":cb,"decoder_bytes":decoder_bytes,
      "complete_bytes":cb+decoder_bytes if cb is not None else None,
      "exact":bool(exact),"elapsed_s":elapsed,"chain":chain,"config":config,"error":error
    })

def structural(rows,src,name,artifact,decoder_bytes,encode_cmd,decode_cmd):
    t=run(encode_cmd,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    ok,td=cold_verify(src,lambda rec:decode_cmd(artifact,rec))
    record(rows,name,artifact,decoder_bytes,t+td,[name],{"encode_cmd":[str(x) for x in encode_cmd]},ok)
    if not ok: return
    for wrap in ("xz","zstd"):
        w=artifact+"."+("xz" if wrap=="xz" else "zst")
        tw=encode_wrap(wrap,artifact,w)
        restored=artifact+".restored"
        decode_wrap(wrap,w,restored)
        ok2,td2=cold_verify(src,lambda rec,r=restored:decode_cmd(r,rec))
        record(rows,name+"_then_"+wrap,w,decoder_bytes,t+tw+td2,[name,wrap],
               {"encode_cmd":[str(x) for x in encode_cmd],"wrapper":wrap},ok2)
        if os.path.exists(restored): os.remove(restored)

def main():
    if len(sys.argv)!=3: raise SystemExit("usage: max_search.py FAMILY ENWIK9")
    fam,src=sys.argv[1:]
    if not verify(src): raise SystemExit("canonical source verification failed")
    rows=[]; started=time.time()
    try:
      if fam=="standard":
        for wrap in ("xz","zstd"):
            out="raw."+("xz" if wrap=="xz" else "zst")
            te=encode_wrap(wrap,src,out)
            rec="std.recovered"; td=decode_wrap(wrap,out,rec); ok=verify(rec); os.remove(rec)
            record(rows,"raw_"+wrap,out,0,te+td,[wrap],{},ok)

      elif fam=="lccp_v1":
        exe="./lccp_v1"; db=compile_cpp(TOOLS/"lccp_v1.cpp",exe)
        sizes=[1_000_000,10_000_000,100_000_000,500_000_000]
        best=None
        for tile in sizes:
            art=f"v1_{tile}.lccp"
            structural(rows,src,f"lccp_v1_tile_{tile}",art,db,[exe,"e",src,art,str(tile)],lambda a,r:[exe,"d",a,r])
            cur=min((x for x in rows if x["exact"] and x["name"].startswith(f"lccp_v1_tile_{tile}")),key=lambda x:x["complete_bytes"],default=None)
            if cur and (best is None or cur["complete_bytes"]<best["complete_bytes"]): best=cur

      elif fam=="recursive_grammar":
        exe="./recursive_grammar"; db=compile_cpp(TOOLS/"lccp_recursive_grammar.cpp",exe)
        configs=[(8192,128,16),(32768,256,32),(65536,512,48)]
        prev=None
        for mr,bp,rd in configs:
            art=f"rgd_{mr}_{bp}_{rd}.rgd"
            structural(rows,src,f"recursive_grammar_{mr}_{bp}_{rd}",art,db,
                       [exe,"encode",src,art,str(mr),str(bp),str(rd)],lambda a,r:[exe,"decode",a,r])
            exact=[x for x in rows if x["exact"] and x["name"].startswith(f"recursive_grammar_{mr}_{bp}_{rd}")]
            cur=min(exact,key=lambda x:x["complete_bytes"],default=None)
            if prev and cur and cur["complete_bytes"]>=prev["complete_bytes"]: break
            if cur: prev=cur

      elif fam=="sequenced_graph":
        exe="./lccp_seq"; db=compile_cpp(TOOLS/"lccp_seq_prototype.cpp",exe)
        for w,h in ((128,128),(256,256),(512,256),(512,512)):
            art=f"seq_{w}_{h}.lcs"
            structural(rows,src,f"sequenced_graph_{w}x{h}",art,db,
                       [exe,"encode",src,art,str(w),str(h)],lambda a,r:[exe,"decode",a,r])

      elif fam=="lightseq":
        exe="./lccp_expand"; db=compile_cpp(TOOLS/"lccp_lightseq_expand.cpp",exe)
        for mode in (0,1):
            art=f"light_{mode}.lls"
            structural(rows,src,f"lightseq_mode{mode}",art,db,
                       [exe,"encode",src,art,str(mode)],lambda a,r:[exe,"decode",a,r])

      elif fam=="lcg4":
        exe="./lccp_lcg4"; db=compile_cpp(TOOLS/"lccp_lcg4.cpp",exe)
        wrapper=TOOLS/"lcg4_block_container.py"
        configs=[(500_000,128),(2_000_000,256),(4_000_000,512),(8_000_000,1024)]
        prev=None
        for bs,mr in configs:
            art=f"lcg4_{bs}_{mr}.l4b"
            structural(rows,src,f"lcg4_blocks_{bs}_{mr}",art,db,
                       [sys.executable,wrapper,"e",exe,src,art,"--block-size",str(bs),"--max-rules",str(mr)],
                       lambda a,r:[sys.executable,wrapper,"d",exe,a,r])
            exact=[x for x in rows if x["exact"] and x["name"].startswith(f"lcg4_blocks_{bs}_{mr}")]
            cur=min(exact,key=lambda x:x["complete_bytes"],default=None)
            if prev and cur and cur["complete_bytes"]>=prev["complete_bytes"]: break
            if cur: prev=cur
      else:
        raise ValueError(fam)
    except Exception as e:
      rows.append({"name":fam+"_search_failure","carrier_bytes":None,"decoder_bytes":None,"complete_bytes":None,
                   "exact":False,"elapsed_s":time.time()-started,"chain":[],"config":{},"error":repr(e)+"\n"+traceback.format_exc(limit=6)})
    report={"family":fam,"task":"minimum complete lossless representation; no target size",
            "source_bytes":EXPECTED_BYTES,"source_sha256":EXPECTED_SHA,
            "variants":rows,"elapsed_s":time.time()-started,
            "plateau_claim":"family-local tested neighborhood only"}
    pathlib.Path(f"candidate_max_{fam}.json").write_text(json.dumps(report,indent=2))
    print(json.dumps(report,indent=2))

if __name__=="__main__": main()
