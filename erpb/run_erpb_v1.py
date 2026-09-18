#!/usr/bin/env python3
from __future__ import annotations
import hashlib, json, os, pathlib, shutil, subprocess, sys, time

ROOT=pathlib.Path(__file__).resolve().parents[1]
ERPB=ROOT/"erpb"
TOOLS=ROOT/"epsilon_challenge"/"tools"
EXPECTED_BYTES=1_000_000_000
EXPECTED_SHA="159b85351e5f76e60cbe32e04c677847a9ecba3adc79addab6f4c6c7aa3744bc"

def sha256(p):
    h=hashlib.sha256()
    with open(p,"rb") as f:
        for b in iter(lambda:f.read(8<<20),b""): h.update(b)
    return h.hexdigest()

def run(cmd, *, stdout=None, stderr=None, check=True):
    t=time.time()
    p=subprocess.run([str(x) for x in cmd],stdout=stdout,stderr=stderr,check=check)
    return p.returncode,time.time()-t

def compile_cpp(src,out):
    run(["g++","-O3","-std=c++17",src,"-o",out])
    subprocess.run(["strip",out],check=False)
    return os.path.getsize(out)

def xz(inp,out,preset="-9e"):
    with open(out,"wb") as f:
        rc,dt=run(["xz",preset,"--threads=1","-c",inp],stdout=f,stderr=subprocess.DEVNULL,check=False)
    if rc: raise RuntimeError("xz failed")
    return dt

def unxz(inp,out):
    with open(out,"wb") as f:
        rc,dt=run(["xz","-dc",inp],stdout=f,stderr=subprocess.DEVNULL,check=False)
    if rc: raise RuntimeError("unxz failed")
    return dt

def exact(path, expected_bytes=EXPECTED_BYTES, expected_sha=EXPECTED_SHA):
    p=pathlib.Path(path)
    return p.exists() and p.stat().st_size==expected_bytes and sha256(p)==expected_sha

def cold_decode(source, decode_cmd, recovered):
    hidden=str(source)+".hidden"
    os.replace(source,hidden)
    try:
        rc,dt=run(decode_cmd,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=False)
        ok=(rc==0 and exact(recovered))
    finally:
        os.replace(hidden,source)
    return ok,dt

def record(rows, channel, variant, carrier, decoder_bytes, exact_ok, elapsed, metadata=None):
    cb=os.path.getsize(carrier)
    rows.append({
        "channel":channel,
        "variant":variant,
        "carrier_bytes":cb,
        "carrier_bits":cb*8,
        "custom_decoder_bytes":decoder_bytes,
        "complete_bytes":cb+decoder_bytes,
        "complete_bits":8*(cb+decoder_bytes),
        "ratio_vs_source":(cb+decoder_bytes)/EXPECTED_BYTES,
        "bytes_saved_vs_source":EXPECTED_BYTES-(cb+decoder_bytes),
        "exact":bool(exact_ok),
        "elapsed_s":elapsed,
        "metadata":metadata or {}
    })

def wrapped_candidate(rows, channel, variant, source, artifact, decoder_bytes, decode_builder, encode_elapsed, metadata=None):
    rec=variant+".recovered"
    ok,td=cold_decode(source,decode_builder(artifact,rec),rec)
    if os.path.exists(rec): os.remove(rec)
    record(rows,channel,variant+"_raw",artifact,decoder_bytes,ok,encode_elapsed+td,metadata)
    xart=artifact+".xz"
    tx=xz(artifact,xart,"-9e")
    restored=artifact+".restored"
    unxz(xart,restored)
    rec=variant+".xz.recovered"
    ok2,td2=cold_decode(source,decode_builder(restored,rec),rec)
    record(rows,channel,variant+"_xz9e",xart,decoder_bytes,ok2,encode_elapsed+tx+td2,metadata)
    for p in (restored,rec):
        if os.path.exists(p): os.remove(p)

def probe_periodic(transform, source):
    probe="erpb_probe.bin"
    n=64*1024*1024
    with open(source,"rb") as a, open(probe,"wb") as b: shutil.copyfileobj(a,b,length=n)
    # copyfileobj will consume full source if length is buffer size, so truncate explicitly.
    with open(probe,"r+b") as f: f.truncate(n)
    candidates=[]
    lags=[2,4,8,16,32,64,128,256,512,1024,4096,16384,65536,262144,1048576]
    for mode in ("delta","xor"):
        for lag in lags:
            art=f"probe_{mode}_{lag}.erp"
            op="encode-period-"+mode
            rc,te=run([transform,op,probe,art,str(lag)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=False)
            if rc: continue
            xx=art+".xz"
            try: tx=xz(art,xx,"-6")
            except Exception: continue
            candidates.append({"mode":mode,"lag":lag,"xz_bytes":os.path.getsize(xx),"elapsed_s":te+tx})
            os.remove(art); os.remove(xx)
    os.remove(probe)
    candidates.sort(key=lambda x:(x["xz_bytes"],x["mode"],x["lag"]))
    return candidates

def main():
    if len(sys.argv)!=2: raise SystemExit("usage: run_erpb_v1.py /path/to/enwik9")
    source=sys.argv[1]
    if not exact(source): raise SystemExit("canonical enwik9 verification failed")
    rows=[]; started=time.time()

    transform="./erpb_transform_v1"
    rg="./erpb_recursive_grammar"
    apc="./erpb_apc"
    transform_bytes=compile_cpp(ERPB/"erpb_transform_v1.cpp",transform)
    rg_bytes=compile_cpp(TOOLS/"lccp_recursive_grammar.cpp",rg)
    apc_bytes=compile_cpp(TOOLS/"apc_final_v1.cpp",apc)

    # Residual projector / conventional control.
    out="residual_raw.xz"; t=xz(source,out,"-9e")
    rec="residual.recovered"; td=unxz(out,rec)
    record(rows,"residual","raw_xz9e",out,0,exact(rec),t+td,{"outer":"xz-9e"})
    os.remove(rec)

    # Repetition projector.
    art="repetition.rgd"
    rc,te=run([rg,"encode",source,art,"32768","256","32"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=False)
    if rc==0:
        wrapped_candidate(rows,"repetition","recursive_grammar_32768_256_32",source,art,rg_bytes,
                          lambda a,r:[rg,"decode",a,r],te,
                          {"max_rules":32768,"batch_pairs":256,"rounds":32})

    # Context projector.
    for page in (262144,1048576,4194304):
        art=f"context_{page}.apc"
        rc,te=run([apc,"encode",source,art,str(page)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=False)
        if rc==0:
            wrapped_candidate(rows,"context",f"apc_page_{page}",source,art,apc_bytes,
                              lambda a,r:[apc,"decode",a,r],te,{"page_bytes":page})

    # Transition projector.
    art="transition.erp"
    rc,te=run([transform,"encode-transition",source,art],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=False)
    if rc==0:
        wrapped_candidate(rows,"transition","delta_prev_byte",source,art,transform_bytes,
                          lambda a,r:[transform,"decode",a,r],te,{"lag":1,"operation":"delta"})

    # Periodic projector: tune on a non-winning prefix, then run full corpus on finalists.
    probe=probe_periodic(transform,source)
    finalists=[]
    for c in probe:
        key=(c["mode"],c["lag"])
        if key not in finalists: finalists.append(key)
        if len(finalists)>=4: break
    for mode,lag in finalists:
        art=f"period_{mode}_{lag}.erp"
        rc,te=run([transform,"encode-period-"+mode,source,art,str(lag)],
                  stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=False)
        if rc==0:
            wrapped_candidate(rows,"periodic",f"{mode}_lag_{lag}",source,art,transform_bytes,
                              lambda a,r:[transform,"decode",a,r],te,
                              {"lag":lag,"operation":mode,"selected_by_64MiB_probe":True})

    exact_rows=[r for r in rows if r["exact"]]
    exact_rows.sort(key=lambda r:(r["complete_bytes"],r["channel"],r["variant"]))
    by_channel={}
    for c in ("repetition","transition","context","periodic","residual"):
        xs=[r for r in exact_rows if r["channel"]==c]
        by_channel[c]=xs[0] if xs else None

    report={
        "benchmark":"ERPB-v1",
        "basis":"DFT-inspired common-substrate projector comparison",
        "source_bytes":EXPECTED_BYTES,
        "source_sha256":EXPECTED_SHA,
        "channels":["repetition","transition","context","periodic","residual"],
        "periodic_probe":probe,
        "variants":rows,
        "channel_incumbents":by_channel,
        "overall_incumbent":exact_rows[0] if exact_rows else None,
        "plateau_status":{
            "repetition":"single established recursive-grammar baseline in v1",
            "transition":"first-order delta baseline in v1",
            "context":"three page scales; local tested plateau only",
            "periodic":"64 MiB parameter discovery followed by four full-corpus finalists",
            "residual":"xz-9e control",
            "global_optimum_claim":False
        },
        "elapsed_s":time.time()-started
    }
    pathlib.Path("ERPB_V1_RESULT.json").write_text(json.dumps(report,indent=2))
    print(json.dumps({
        "overall_incumbent":report["overall_incumbent"],
        "channel_incumbents":report["channel_incumbents"],
        "elapsed_s":report["elapsed_s"]
    },indent=2))

if __name__=="__main__": main()
