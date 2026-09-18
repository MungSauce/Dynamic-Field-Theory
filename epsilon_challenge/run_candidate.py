#!/usr/bin/env python3
from __future__ import annotations
import hashlib, json, os, pathlib, shutil, subprocess, sys, time, traceback

ROOT=pathlib.Path(__file__).resolve().parent
TOOLS=ROOT/"tools"
EXPECTED_BYTES=1_000_000_000
EXPECTED_SHA="159b85351e5f76e60cbe32e04c677847a9ecba3adc79addab6f4c6c7aa3744bc"
EXPECTED_MD5="e206c3450ac99950df65bf70ef61a12d"

def sha256(p):
    h=hashlib.sha256()
    with open(p,"rb") as f:
        for b in iter(lambda:f.read(8<<20),b""): h.update(b)
    return h.hexdigest()

def md5(p):
    h=hashlib.md5()
    with open(p,"rb") as f:
        for b in iter(lambda:f.read(8<<20),b""): h.update(b)
    return h.hexdigest()

def run(args, *, stdout=None, stderr=None):
    t=time.time()
    subprocess.run([str(x) for x in args], check=True, stdout=stdout, stderr=stderr)
    return time.time()-t

def verify_source(p):
    p=pathlib.Path(p)
    return p.stat().st_size==EXPECTED_BYTES and sha256(p)==EXPECTED_SHA and md5(p)==EXPECTED_MD5

def verify_recovered(p):
    p=pathlib.Path(p)
    return p.exists() and p.stat().st_size==EXPECTED_BYTES and sha256(p)==EXPECTED_SHA

def compile_cpp(src, out):
    run(["g++","-O3","-std=c++17",src,"-o",out])
    subprocess.run(["strip",out],check=False)
    return os.path.getsize(out)

def xz_encode(inp,out):
    with open(out,"wb") as f:
        return run(["xz","-9e","--threads=1","-c",inp],stdout=f,stderr=subprocess.DEVNULL)

def xz_decode(inp,out):
    with open(out,"wb") as f:
        return run(["xz","-dc",inp],stdout=f,stderr=subprocess.DEVNULL)

def zstd_encode(inp,out):
    return run(["zstd","-q","--ultra","-22","-T1","-f",inp,"-o",out],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)

def zstd_decode(inp,out):
    return run(["zstd","-q","-d","-f",inp,"-o",out],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)

def add_result(results,name,carrier,decoder_bytes,exact,elapsed,chain,error=None):
    cb=os.path.getsize(carrier) if os.path.exists(carrier) else None
    results.append({
      "name":name,
      "carrier_bytes":cb,
      "decoder_bytes":decoder_bytes,
      "complete_bytes":(cb+decoder_bytes) if cb is not None else None,
      "exact":bool(exact),
      "elapsed_s":elapsed,
      "chain":chain,
      "error":error,
    })

def structural_variant(results, name, src, artifact, exe, encode_args, decode_builder):
    decoder_bytes=os.path.getsize(exe)
    rec=f"{name}.recovered"
    t=run(encode_args,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    hidden=f"{src}.hidden"
    os.replace(src,hidden)
    try:
        td=run(decode_builder(artifact,rec),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        exact=verify_recovered(rec)
    finally:
        os.replace(hidden,src)
    add_result(results,name,artifact,decoder_bytes,exact,t+td,[name])
    if os.path.exists(rec): os.remove(rec)

    # Epsilon's transform-chain relation explicitly permits reversible composition.
    xart=artifact+".xz"
    tx=xz_encode(artifact,xart)
    restored=artifact+".restored"
    xz_decode(xart,restored)
    rec=f"{name}.xz.recovered"
    hidden=f"{src}.hidden"
    os.replace(src,hidden)
    try:
        td=run(decode_builder(restored,rec),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        exact=verify_recovered(rec)
    finally:
        os.replace(hidden,src)
    add_result(results,name+"_then_xz9e",xart,decoder_bytes,exact,t+tx+td,[name,"xz9e"])
    for p in (restored,rec):
        if os.path.exists(p): os.remove(p)

def main():
    if len(sys.argv)!=3:
        raise SystemExit("usage: run_candidate.py FAMILY ENWIK9")
    family,src=sys.argv[1],sys.argv[2]
    if not verify_source(src): raise SystemExit("canonical enwik9 verification failed")
    results=[]
    started=time.time()
    try:
        if family=="standard":
            out="raw.xz"; rec="raw.xz.recovered"
            t=xz_encode(src,out); td=xz_decode(out,rec)
            add_result(results,"raw_xz9e",out,0,verify_recovered(rec),t+td,["xz9e"])
            os.remove(rec)
            out="raw.zst"; rec="raw.zst.recovered"
            t=zstd_encode(src,out); td=zstd_decode(out,rec)
            add_result(results,"raw_zstd22",out,0,verify_recovered(rec),t+td,["zstd22"])
            os.remove(rec)

        elif family=="lccp_v1":
            exe="./lccp_v1"
            compile_cpp(TOOLS/"lccp_v1.cpp",exe)
            structural_variant(results,"lccp_v1",src,"v1.lccp",exe,
                [exe,"e",src,"v1.lccp","100000000"],
                lambda art,rec:[exe,"d",art,rec])

        elif family=="recursive_grammar":
            exe="./recursive_grammar"
            compile_cpp(TOOLS/"lccp_recursive_grammar.cpp",exe)
            structural_variant(results,"recursive_grammar",src,"enwik9.rgd",exe,
                [exe,"encode",src,"enwik9.rgd","32768","256","32"],
                lambda art,rec:[exe,"decode",art,rec])

        elif family=="sequenced_graph":
            exe="./lccp_seq"
            compile_cpp(TOOLS/"lccp_seq_prototype.cpp",exe)
            structural_variant(results,"sequenced_graph",src,"enwik9.lcs",exe,
                [exe,"encode",src,"enwik9.lcs","256","256"],
                lambda art,rec:[exe,"decode",art,rec])

        elif family=="lightseq":
            exe="./lccp_expand"
            compile_cpp(TOOLS/"lccp_lightseq_expand.cpp",exe)
            for mode in ("0","1"):
                nm=f"lightseq_mode{mode}"
                art=f"enwik9.m{mode}.lls"
                structural_variant(results,nm,src,art,exe,
                    [exe,"encode",src,art,mode],
                    lambda a,r,e=exe:[e,"decode",a,r])

        elif family=="lcg4":
            exe="./lccp_lcg4"
            compile_cpp(TOOLS/"lccp_lcg4.cpp",exe)
            wrapper=TOOLS/"lcg4_block_container.py"
            structural_variant(results,"lcg4_blocks",src,"enwik9.l4b",exe,
                [sys.executable,wrapper,"e",exe,src,"enwik9.l4b","--block-size","2000000","--max-rules","256"],
                lambda art,rec:[sys.executable,wrapper,"d",exe,art,rec])
        else:
            raise ValueError(f"unknown family {family}")
    except Exception as e:
        results.append({
          "name":family+"_family_failure","carrier_bytes":None,"decoder_bytes":None,
          "complete_bytes":None,"exact":False,"elapsed_s":time.time()-started,
          "chain":[],"error":repr(e)+"\n"+traceback.format_exc(limit=4)
        })

    report={
      "family":family,
      "source_bytes":EXPECTED_BYTES,
      "source_sha256":EXPECTED_SHA,
      "variants":results,
      "family_elapsed_s":time.time()-started,
      "policy":"retain only byte-exact candidates; compare complete bytes; preserve failures"
    }
    out=pathlib.Path(f"candidate_{family}.json")
    out.write_text(json.dumps(report,indent=2))
    print(json.dumps(report,indent=2))

if __name__=="__main__":
    main()
