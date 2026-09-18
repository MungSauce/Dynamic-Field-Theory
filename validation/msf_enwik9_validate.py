#!/usr/bin/env python3
from __future__ import annotations
import argparse, binascii, hashlib, mmap, os, struct, tempfile, time, zlib
from pathlib import Path

MAGIC=b"MSF412V1"; VERSION=1; HEADER_SIZE=144; INSTRUMENTS=32; PITCH_COUNT=412; MAX_ALPHABET=206
HEADER=struct.Struct("<8sHHIHHHHQQQQQ32s32sI12s")
assert HEADER.size==HEADER_SIZE

def sha(path):
    h=hashlib.sha256()
    with open(path,"rb") as f:
        for c in iter(lambda:f.read(8<<20),b""): h.update(c)
    return h.digest()

def scan(path):
    seen=bytearray(256); h=hashlib.sha256(); n=0
    with open(path,"rb") as f:
        for c in iter(lambda:f.read(8<<20),b""):
            n+=len(c); h.update(c)
            for b in set(c): seen[b]=1
    return n,bytes(i for i,v in enumerate(seen) if v),h.digest()

def field_blocks(mm,n,table,sentinel,block_pairs=4<<20):
    pairs=(n+1)//2; rev_count=n//2
    slots=((pairs+31)//32)*32
    start=0
    while start<slots:
        end=min(slots,start+block_pairs); count=end-start
        out=bytearray([sentinel])*(2*count)
        fe=min(end,pairs); fc=max(0,fe-start)
        if fc: out[0:2*fc:2]=mm[start:fe].translate(table)
        rs=min(start,rev_count); re=min(end,rev_count); rc=max(0,re-rs)
        if rc:
            rev=mm[n-re:n-rs][::-1].translate(table); rel=rs-start
            out[2*rel+1:2*(rel+rc):2]=rev
        yield bytes(out)
        start=end

def pack_header(flags,a,codec,n,pairs,frames,field_bytes,payload_bytes,src_sha,pay_sha):
    vals=[MAGIC,VERSION,HEADER_SIZE,flags,INSTRUMENTS,PITCH_COUNT,a,codec,n,pairs,frames,field_bytes,payload_bytes,src_sha,pay_sha,0,b"\0"*12]
    base=HEADER.pack(*vals); vals[-2]=binascii.crc32(base)&0xffffffff
    return HEADER.pack(*vals)

def unpack_header(data):
    if len(data)!=HEADER_SIZE: raise ValueError("bad header size")
    v=list(HEADER.unpack(data))
    magic,ver,hs,flags,inst,pitches,a,codec,n,pairs,frames,field_bytes,payload_bytes,src_sha,pay_sha,stored_crc,res=v
    if (magic,ver,hs,inst,pitches)!=(MAGIC,VERSION,HEADER_SIZE,INSTRUMENTS,PITCH_COUNT): raise ValueError("header constants fail")
    if a>MAX_ALPHABET: raise ValueError("alphabet too large")
    v[-2]=0
    if (binascii.crc32(HEADER.pack(*v))&0xffffffff)!=stored_crc: raise ValueError("header crc fail")
    if pairs!=(n+1)//2 or frames!=(pairs+31)//32 or field_bytes!=frames*64: raise ValueError("geometry fail")
    return dict(flags=flags,a=a,codec=codec,n=n,pairs=pairs,frames=frames,field_bytes=field_bytes,payload_bytes=payload_bytes,src_sha=src_sha,pay_sha=pay_sha)

def compose(src,msf,level=6):
    n,alphabet,src_sha=scan(src)
    if len(alphabet)>MAX_ALPHABET: raise ValueError(f"{len(alphabet)} symbols > 206")
    rank=bytearray(256)
    for i,b in enumerate(alphabet): rank[b]=i
    table=bytes(rank); sentinel=len(alphabet)
    pairs=(n+1)//2; frames=(pairs+31)//32; field_bytes=frames*64
    with tempfile.NamedTemporaryFile(delete=False,dir=str(Path(msf).parent),prefix="msf_payload_") as t: tmp=t.name
    try:
        co=zlib.compressobj(level=level); ph=hashlib.sha256(); pbytes=0
        with open(tmp,"wb") as pf:
            if n:
                with open(src,"rb") as f, mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as mm:
                    for block in field_blocks(mm,n,table,sentinel):
                        x=co.compress(block)
                        if x: pf.write(x); ph.update(x); pbytes+=len(x)
            x=co.flush()
            if x: pf.write(x); ph.update(x); pbytes+=len(x)
        hdr=pack_header(7,len(alphabet),1,n,pairs,frames,field_bytes,pbytes,src_sha,ph.digest())
        with open(msf,"wb") as out, open(tmp,"rb") as pf:
            out.write(hdr); out.write(alphabet)
            for c in iter(lambda:pf.read(8<<20),b""): out.write(c)
        return dict(source_bytes=n,alphabet_size=len(alphabet),pair_count=pairs,frame_count=frames,canonical_field_bytes=field_bytes,payload_bytes=pbytes,complete_msf_bytes=os.path.getsize(msf),source_sha256=src_sha.hex(),payload_sha256=ph.hexdigest())
    finally:
        try: os.remove(tmp)
        except FileNotFoundError: pass

def listen(msf,outpath):
    with open(msf,"rb") as f:
        h=unpack_header(f.read(HEADER_SIZE)); alphabet=f.read(h["a"])
        if len(alphabet)!=h["a"] or len(set(alphabet))!=len(alphabet): raise ValueError("alphabet fail")
        payload=f.read(h["payload_bytes"])
        if len(payload)!=h["payload_bytes"] or f.read(1): raise ValueError("payload length/trailing fail")
        if hashlib.sha256(payload).digest()!=h["pay_sha"]: raise ValueError("payload hash fail")
    field=zlib.decompress(payload)
    if len(field)!=h["field_bytes"]: raise ValueError("field length fail")
    sentinel=h["a"]; dec=bytearray(256)
    for i,b in enumerate(alphabet): dec[i]=b
    dec=bytes(dec); rev_count=h["n"]//2
    with open(outpath,"w+b") as out:
        out.truncate(h["n"]); fd=out.fileno()
        slots=len(field)//2; block=4<<20; start=0
        def write_at(data,off):
            if hasattr(os,"pwrite"): os.pwrite(fd,data,off)
            else: out.seek(off); out.write(data)
        while start<slots:
            end=min(slots,start+block); pair=field[2*start:2*end]
            fr=pair[0::2]; rr=pair[1::2]
            fve=min(end,h["pairs"]); fvc=max(0,fve-start)
            if fvc:
                x=fr[:fvc]
                if max(x)>=h["a"]: raise ValueError("forward state fail")
                write_at(x.translate(dec),start)
            if any(v!=sentinel for v in fr[fvc:]): raise ValueError("forward pad fail")
            rve=min(end,rev_count); rvc=max(0,rve-start)
            if rvc:
                x=rr[:rvc]
                if max(x)>=h["a"]: raise ValueError("reverse state fail")
                write_at(x.translate(dec)[::-1],h["n"]-rve)
            if any(v!=sentinel for v in rr[rvc:]): raise ValueError("reverse pad fail")
            start=end
    got=sha(outpath)
    if got!=h["src_sha"]: raise ValueError("recovered source SHA-256 fail")
    return got.hex()

def main():
    ap=argparse.ArgumentParser(); ap.add_argument("source"); ap.add_argument("--report",default="benchmark_report.json"); a=ap.parse_args()
    src=Path(a.source); msf=Path("enwik9.msf"); rec=Path("recovered_enwik9")
    expected="159b85351e5f76e60cbe32e04c677847a9ecba3adc79addab6f4c6c7aa3744bc"
    if sha(src).hex()!=expected: raise SystemExit("source SHA mismatch before compose")
    t=time.perf_counter(); c=compose(src,msf,6); t1=time.perf_counter()
    src.unlink()
    recovered=listen(msf,rec); t2=time.perf_counter()
    report={**c,"pitch_count":412,"instrument_count":32,"logical_symbols_per_full_frame":64,"complete_ratio":c["complete_msf_bytes"]/c["source_bytes"],"recovered_sha256":recovered,"source_removed_before_decode":True,"compose_seconds":t1-t,"listen_seconds":t2-t1,"status":"PASS"}
    import json; Path(a.report).write_text(json.dumps(report,indent=2)+"\n"); print(json.dumps(report,indent=2))
if __name__=="__main__": main()
