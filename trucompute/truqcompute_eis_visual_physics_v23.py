#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import lzma
import math
import struct
import zlib
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path

MAGIC=b"TCEVP23\0"
VERSION=23
HEADER_BYTES=128
NODES=206
WIDTH=3840
HEIGHT=2160
PIXELS=WIDTH*HEIGHT
MAX_ORDER=2
BACKENDS={"raw":0,"zlib":1,"lzma":2}
BACKEND_NAMES={v:k for k,v in BACKENDS.items()}

def sha256(b:bytes)->bytes: return hashlib.sha256(b).digest()

def ve(n:int)->bytes:
    if n<0: raise ValueError("negative varint")
    o=bytearray()
    while True:
        b=n&127; n>>=7
        o.append(b|(128 if n else 0))
        if not n: return bytes(o)

def vd(data:bytes,p:int)->tuple[int,int]:
    v=s=0
    while True:
        if p>=len(data) or s>63: raise ValueError("bad varint")
        b=data[p]; p+=1; v|=(b&127)<<s
        if not b&128: return v,p
        s+=7

@dataclass
class Stat:
    count:int=0
    last:int=-1

def order(m:dict[int,Stat])->list[int]:
    return sorted(m,key=lambda x:(-m[x].count,-m[x].last,x))

def update(models,history:list[int],x:int,t:int)->None:
    for n in range(min(MAX_ORDER,len(history))+1):
        c=tuple(history[-n:]) if n else ()
        st=models[n][c].get(x)
        if st is None:
            st=Stat(); models[n][c][x]=st
        st.count+=1; st.last=t

def encode_forcing(ids:list[int])->tuple[bytes,dict]:
    models=[defaultdict(dict) for _ in range(MAX_ORDER+1)]
    hist=[]; out=bytearray(); kc=Counter(); rc=Counter(); pc=Counter()
    for t,x in enumerate(ids):
        sel=-1; rank=x
        for n in range(min(MAX_ORDER,len(hist)),-1,-1):
            c=tuple(hist[-n:]) if n else ()
            m=models[n].get(c)
            if m and x in m:
                rank=order(m).index(x); sel=n; break
        kind=0 if sel<0 else sel+1
        out.extend(ve((rank<<2)|kind))
        kc[kind]+=1; rc[rank]+=1; pc[(kind,rank)]+=1
        update(models,hist,x,t); hist.append(x)
    H=0.0; N=len(ids)
    for c in pc.values():
        p=c/N; H-=p*math.log2(p)
    return bytes(out),{
        "forcing_raw_bytes":len(out),
        "rank0_events":rc[0],
        "rank0_fraction":rc[0]/N if N else 0.0,
        "fallback_events":kc[0],
        "order0_events":kc[1],
        "order1_events":kc[2],
        "order2_events":kc[3],
        "forcing_entropy_bits_per_event":H,
        "forcing_entropy_total_bytes":H*N/8.0,
    }

def decode_forcing(raw:bytes,count:int,alpha:int)->list[int]:
    models=[defaultdict(dict) for _ in range(MAX_ORDER+1)]
    hist=[]; out=[]; p=0
    for t in range(count):
        q,p=vd(raw,p); kind=q&3; rank=q>>2
        if kind==0:
            if rank>=alpha: raise ValueError("fallback outside alphabet")
            x=rank
        else:
            n=kind-1
            if n>MAX_ORDER or n>len(hist): raise ValueError("bad context order")
            c=tuple(hist[-n:]) if n else ()
            m=models[n].get(c)
            if not m: raise ValueError("missing context")
            o=order(m)
            if rank>=len(o): raise ValueError("rank outside context")
            x=o[rank]
        out.append(x); update(models,hist,x,t); hist.append(x)
    if p!=len(raw): raise ValueError("trailing forcing bytes")
    return out

class EIS206:
    __slots__=("selected","changed","unchanged","flips")
    def __init__(self):
        self.selected=-1; self.changed=0; self.unchanged=0; self.flips=0
    def drive(self,x:int)->int:
        if not 0<=x<NODES: raise ValueError("EIS node outside field")
        if x==self.selected:
            self.unchanged+=1
        else:
            self.flips+=1 if self.selected<0 else 2
            self.selected=x; self.changed+=1
        return self.selected

class Painter:
    """Fixed 4K visual material. One byte is the internal palette state of one
    physical pixel. Frames are snapshots of this same matrix at page boundaries."""
    __slots__=("pixels","cursor","frame")
    def __init__(self):
        self.pixels=bytearray(PIXELS)
        self.cursor=0; self.frame=0
    def put(self,x:int)->None:
        self.pixels[self.cursor]=x
        self.cursor+=1
    def full(self)->bool: return self.cursor==PIXELS
    def snapshot(self)->bytes:
        # Completed-state snapshot. This is runtime memory, never artifact state.
        return bytes(self.pixels[:self.cursor])
    def next_frame(self)->None:
        self.cursor=0; self.frame+=1

class SnapshotReader:
    __slots__=("alphabet","out","frames_read","snapshot_bytes_read")
    def __init__(self,alphabet:bytes):
        self.alphabet=alphabet; self.out=bytearray()
        self.frames_read=0; self.snapshot_bytes_read=0
    def consume(self,snapshot:bytes)->None:
        a=self.alphabet
        if snapshot and max(snapshot)>=len(a): raise ValueError("snapshot outside alphabet")
        self.out.extend(a[i] for i in snapshot)
        self.frames_read+=1; self.snapshot_bytes_read+=len(snapshot)

def source_ids(source:bytes)->tuple[bytes,list[int]]:
    alphabet=bytes(sorted(set(source)))
    if len(alphabet)>NODES:
        raise ValueError(f"{len(alphabet)} source symbols exceed EIS206")
    lut=[-1]*256
    for i,b in enumerate(alphabet): lut[b]=i
    return alphabet,[lut[b] for b in source]

def replay(ids:list[int],alphabet:bytes,mode:str)->tuple[bytes,dict]:
    """Physics producer and snapshot reader are process-wise separable.

    stream: completed frame snapshot is consumed then discarded immediately.
    buffer: all completed snapshots are collected first, then read afterward.
    """
    if mode not in ("stream","buffer"): raise ValueError("bad replay mode")
    eis=EIS206(); painter=Painter(); reader=SnapshotReader(alphabet)
    buffered=[]; peak_buffer=0; buffer_bytes=0; snapshots=0

    def finish_frame():
        nonlocal buffer_bytes,peak_buffer,snapshots
        snap=painter.snapshot(); snapshots+=1
        if mode=="buffer":
            buffered.append(snap); buffer_bytes+=len(snap)
            peak_buffer=max(peak_buffer,buffer_bytes)
        else:
            reader.consume(snap); peak_buffer=max(peak_buffer,len(snap))
        painter.next_frame()

    for x in ids:
        painter.put(eis.drive(x))
        if painter.full(): finish_frame()
    if painter.cursor: finish_frame()

    if mode=="buffer":
        for snap in buffered:
            reader.consume(snap)
        buffered.clear(); buffer_bytes=0

    return bytes(reader.out),{
        "visual_width":WIDTH,
        "visual_height":HEIGHT,
        "physical_visual_pixels":PIXELS,
        "palette_state_bytes_per_pixel":1,
        "eis_nodes":NODES,
        "frames_snapshotted":snapshots,
        "snapshot_bytes_read":reader.snapshot_bytes_read,
        "snapshot_bytes_retained_in_artifact":0,
        "snapshot_peak_buffer_bytes":peak_buffer,
        "replay_mode":mode,
        "eis_changed_compositions":eis.changed,
        "eis_unchanged_compositions":eis.unchanged,
        "eis_node_flips":eis.flips,
    }

def cb(data:bytes,name:str)->bytes:
    if name=="raw": return data
    if name=="zlib": return zlib.compress(data,9)
    if name=="lzma": return lzma.compress(data,preset=9)
    raise ValueError("bad backend")

def db(data:bytes,bid:int)->bytes:
    n=BACKEND_NAMES.get(bid)
    if n=="raw": return data
    if n=="zlib": return zlib.decompress(data)
    if n=="lzma": return lzma.decompress(data)
    raise ValueError("bad backend")

def header(source_len:int,alpha:int,rawlen:int,bodylen:int,backend:str,h:bytes)->bytes:
    b=bytearray(HEADER_BYTES)
    struct.pack_into("<8sHHBBBBIIQQQ",b,0,MAGIC,VERSION,HEADER_BYTES,
        BACKENDS[backend],MAX_ORDER,alpha,0,WIDTH,HEIGHT,source_len,rawlen,bodylen)
    b[48:80]=h
    struct.pack_into("<I",b,124,zlib.crc32(b[:124])&0xffffffff)
    return bytes(b)

def parse_header(b:bytes)->dict:
    if len(b)!=HEADER_BYTES: raise ValueError("bad header")
    v=struct.unpack_from("<8sHHBBBBIIQQQ",b,0)
    magic,ver,hb,bid,mo,alpha,_r,w,ht,n,rl,bl=v
    if magic!=MAGIC or ver!=VERSION or hb!=HEADER_BYTES: raise ValueError("wrong artifact")
    if bid not in BACKEND_NAMES or mo!=MAX_ORDER or w!=WIDTH or ht!=HEIGHT: raise ValueError("unsupported artifact")
    if not 1<=alpha<=NODES: raise ValueError("bad alphabet")
    if zlib.crc32(b[:124])&0xffffffff!=struct.unpack_from("<I",b,124)[0]: raise ValueError("header CRC")
    return {"backend":bid,"alpha":alpha,"source_len":n,"rawlen":rl,"bodylen":bl,"sha":b[48:80]}

def encode(source:bytes,backend:str)->tuple[bytes,dict]:
    if not source: raise ValueError("empty source")
    alphabet,ids=source_ids(source)
    raw,fm=encode_forcing(ids); body=cb(raw,backend)
    art=header(len(source),len(alphabet),len(raw),len(body),backend,sha256(source))+alphabet+body
    rs,ms=replay(ids,alphabet,"stream")
    rb,mb=replay(ids,alphabet,"buffer")
    if rs!=source or rb!=source: raise AssertionError("physics replay mismatch")
    return art,{
        **fm,
        "source_bytes":len(source),
        "alphabet_symbols":len(alphabet),
        "alphabet_bytes_retained":len(alphabet),
        "forcing_backend":backend,
        "forcing_compressed_bytes":len(body),
        "artifact_bytes":len(art),
        "artifact_ratio_vs_source":len(art)/len(source),
        "direct_zlib_bytes":len(zlib.compress(source,9)),
        "direct_lzma_bytes":len(lzma.compress(source,preset=9)),
        "stream_peak_snapshot_buffer_bytes":ms["snapshot_peak_buffer_bytes"],
        "buffered_peak_snapshot_buffer_bytes":mb["snapshot_peak_buffer_bytes"],
        "frames_snapshotted":ms["frames_snapshotted"],
        "snapshot_bytes_retained_in_artifact":0,
        "physics_exact_stream":True,
        "physics_exact_buffered":True,
    }

def decode(artifact:bytes,mode:str="stream")->tuple[bytes,dict]:
    h=parse_header(artifact[:HEADER_BYTES])
    a0=HEADER_BYTES; a1=a0+h["alpha"]; e1=a1+h["bodylen"]
    if e1!=len(artifact): raise ValueError("length mismatch")
    alphabet=artifact[a0:a1]
    raw=db(artifact[a1:e1],h["backend"])
    if len(raw)!=h["rawlen"]: raise ValueError("forcing length mismatch")
    ids=decode_forcing(raw,h["source_len"],h["alpha"])
    source,m=replay(ids,alphabet,mode)
    if len(source)!=h["source_len"] or sha256(source)!=h["sha"]: raise ValueError("cold exact replay failed")
    return source,m

def probe(path:str|Path,prefix:int=0)->dict:
    with open(path,"rb") as f: src=f.read(prefix) if prefix else f.read()
    out={"source":str(path),"prefix":prefix,"codec_script_bytes":Path(__file__).stat().st_size,"backends":{}}
    for backend in BACKENDS:
        art,m=encode(src,backend)
        a,sa=decode(art,"stream"); b,sb=decode(art,"buffer")
        if a!=src or b!=src: raise AssertionError("cold replay failed")
        m["cold_stream_exact"]=True; m["cold_buffered_exact"]=True
        m["cold_stream_peak_snapshot_buffer_bytes"]=sa["snapshot_peak_buffer_bytes"]
        m["cold_buffered_peak_snapshot_buffer_bytes"]=sb["snapshot_peak_buffer_bytes"]
        out["backends"][backend]=m
    return out

def main():
    ap=argparse.ArgumentParser(description="TruCompute EIS visual physics v23")
    s=ap.add_subparsers(dest="cmd",required=True)
    p=s.add_parser("encode"); p.add_argument("source"); p.add_argument("artifact"); p.add_argument("--backend",choices=BACKENDS,default="lzma")
    p=s.add_parser("decode"); p.add_argument("artifact"); p.add_argument("output"); p.add_argument("--mode",choices=["stream","buffer"],default="stream")
    p=s.add_parser("probe"); p.add_argument("source"); p.add_argument("--prefix",type=int,default=0)
    a=ap.parse_args()
    if a.cmd=="encode":
        art,m=encode(Path(a.source).read_bytes(),a.backend); Path(a.artifact).write_bytes(art); print(json.dumps(m,indent=2,sort_keys=True))
    elif a.cmd=="decode":
        src,m=decode(Path(a.artifact).read_bytes(),a.mode); Path(a.output).write_bytes(src)
        print(json.dumps({"decoded_bytes":len(src),"sha256":sha256(src).hex(),"exact":True,**m},indent=2,sort_keys=True))
    else:
        print(json.dumps(probe(a.source,a.prefix),indent=2,sort_keys=True))

if __name__=="__main__": main()
