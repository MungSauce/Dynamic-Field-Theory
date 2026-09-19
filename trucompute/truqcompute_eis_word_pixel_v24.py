#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, json, lzma, math, struct, zlib
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path

MAGIC=b"TCEWP24\0"; VERSION=24; HEADER_BYTES=160
NODES=206; STATES=NODES**3
WIDTH=3840; HEIGHT=2160; PIXELS=WIDTH*HEIGHT; MAX_ORDER=2
WORD=0; WS=1
BACKENDS={"raw":0,"zlib":1,"lzma":2}; BACKEND_NAMES={v:k for k,v in BACKENDS.items()}

def sha(b): return hashlib.sha256(b).digest()
def ve(n):
    o=bytearray()
    while True:
        b=n&127; n>>=7; o.append(b|(128 if n else 0))
        if not n:return bytes(o)
def vd(d,p):
    v=s=0
    while True:
        if p>=len(d) or s>63: raise ValueError("bad varint")
        b=d[p];p+=1;v|=(b&127)<<s
        if not b&128:return v,p
        s+=7

def tokenize(src:bytes):
    """Words are maximal non-whitespace runs. One ordinary following ASCII
    space is implicit. Any other whitespace run is an exact override token."""
    out=[]; i=0; n=len(src); wsset=b" \t\r\n\v\f"
    while i<n:
        if src[i] in wsset:
            j=i+1
            while j<n and src[j] in wsset:j+=1
            w=src[i:j]
            # A single ordinary separator space after a word is universal and free.
            # At BOF or EOF it must remain explicit to preserve exact bytes.
            if not (w==b" " and out and out[-1][0]==WORD and j<n):
                out.append((WS,w))
            i=j
        else:
            j=i+1
            while j<n and src[j] not in wsset:j+=1
            out.append((WORD,src[i:j])); i=j
    return out

def render(tokens, dictionary):
    out=bytearray(); prev=None
    for tid in tokens:
        kind,payload=dictionary[tid]
        if kind==WORD:
            out.extend(payload); out.append(32)
        else:
            if prev==WORD and out and out[-1]==32: out.pop()
            out.extend(payload)
        prev=kind
    if prev==WORD and out and out[-1]==32: out.pop()
    return bytes(out)

def make_dictionary(tokens):
    f=Counter(tokens)
    items=sorted(f,key=lambda t:(-f[t],t[0],t[1]))
    ids={t:i for i,t in enumerate(items)}
    raw=bytearray(ve(len(items)))
    for k,p in items:
        raw.append(k);raw.extend(ve(len(p)));raw.extend(p)
    return items,ids,bytes(raw)

def parse_dictionary(raw):
    n,p=vd(raw,0); out=[]
    for _ in range(n):
        if p>=len(raw):raise ValueError("truncated dictionary")
        k=raw[p];p+=1
        if k not in (WORD,WS):raise ValueError("bad token kind")
        ln,p=vd(raw,p); e=p+ln
        if e>len(raw):raise ValueError("truncated token")
        out.append((k,raw[p:e]));p=e
    if p!=len(raw):raise ValueError("trailing dictionary")
    return out

@dataclass
class Stat:
    count:int=0; last:int=-1
def ordered(m): return sorted(m,key=lambda x:(-m[x].count,-m[x].last,x))
def upd(models,hist,x,t):
    for n in range(min(MAX_ORDER,len(hist))+1):
        c=tuple(hist[-n:]) if n else ()
        s=models[n][c].get(x)
        if s is None:s=Stat();models[n][c][x]=s
        s.count+=1;s.last=t

def forcing_encode(ids):
    models=[defaultdict(dict) for _ in range(3)];hist=[];out=bytearray();kc=Counter();rc=Counter();pc=Counter()
    for t,x in enumerate(ids):
        sel=-1;rank=x
        for n in range(min(2,len(hist)),-1,-1):
            c=tuple(hist[-n:]) if n else ();m=models[n].get(c)
            if m and x in m: rank=ordered(m).index(x);sel=n;break
        kind=0 if sel<0 else sel+1;out.extend(ve((rank<<2)|kind))
        kc[kind]+=1;rc[rank]+=1;pc[(kind,rank)]+=1;upd(models,hist,x,t);hist.append(x)
    H=0.;N=len(ids)
    for c in pc.values():
        p=c/N;H-=p*math.log2(p)
    return bytes(out),{"event_count":N,"forcing_raw_bytes":len(out),"rank0_events":rc[0],"rank0_fraction":rc[0]/N if N else 0.,
        "fallback_events":kc[0],"order0_events":kc[1],"order1_events":kc[2],"order2_events":kc[3],
        "forcing_entropy_bits_per_event":H,"forcing_entropy_total_bytes":H*N/8}

def forcing_decode(raw,count,unique):
    models=[defaultdict(dict) for _ in range(3)];hist=[];out=[];p=0
    for t in range(count):
        q,p=vd(raw,p);kind=q&3;rank=q>>2
        if kind==0:
            if rank>=unique:raise ValueError("fallback outside lexicon")
            x=rank
        else:
            n=kind-1;c=tuple(hist[-n:]) if n else ();m=models[n].get(c)
            if not m:raise ValueError("missing context")
            o=ordered(m)
            if rank>=len(o):raise ValueError("rank outside context")
            x=o[rank]
        out.append(x);upd(models,hist,x,t);hist.append(x)
    if p!=len(raw):raise ValueError("trailing forcing")
    return out

def rgb206(i):
    if not 0<=i<STATES:raise ValueError("button exceeds 206^3 visual states")
    q,r=divmod(i,NODES*NODES);g,b=divmod(r,NODES)
    return q,g,b
def id206(r,g,b):
    if max(r,g,b)>=NODES:raise ValueError("non-EIS channel")
    return r*NODES*NODES+g*NODES+b

class EIS:
    __slots__=("selected","changes","repeats","flips")
    def __init__(self):self.selected=-1;self.changes=self.repeats=self.flips=0
    def drive(self,x):
        if x==self.selected:self.repeats+=1
        else:self.flips+=1 if self.selected<0 else 2;self.selected=x;self.changes+=1
        return self.selected

class Painter:
    __slots__=("buf","pixels")
    def __init__(self):self.buf=bytearray();self.pixels=0
    def put(self,tid,eis):
        for d in rgb206(tid):self.buf.append(eis.drive(d))
        self.pixels+=1
    def full(self):return self.pixels==PIXELS
    def snapshot(self):return bytes(self.buf)
    def clear(self):self.buf.clear();self.pixels=0

class Reader:
    def __init__(self,dictionary):self.dictionary=dictionary;self.ids=[];self.bytes_read=0;self.frames=0
    def consume(self,snap):
        if len(snap)%3:raise ValueError("bad RGB206 snapshot")
        for p in range(0,len(snap),3):
            tid=id206(snap[p],snap[p+1],snap[p+2])
            if tid>=len(self.dictionary):raise ValueError("pixel outside dictionary")
            self.ids.append(tid)
        self.bytes_read+=len(snap);self.frames+=1
    def source(self):return render(self.ids,self.dictionary)

def physics(ids,dictionary,mode):
    if mode not in ("stream","buffer"):raise ValueError("bad mode")
    eis=EIS();paint=Painter();reader=Reader(dictionary);saved=[];peak=0;cur=0
    def finish():
        nonlocal peak,cur
        snap=paint.snapshot()
        if mode=="stream":reader.consume(snap);peak=max(peak,len(snap))
        else:saved.append(snap);cur+=len(snap);peak=max(peak,cur)
        paint.clear()
    for tid in ids:
        paint.put(tid,eis)
        if paint.full():finish()
    if paint.pixels:finish()
    if mode=="buffer":
        for s in saved:reader.consume(s)
        saved.clear()
    return reader.source(),{"frames_snapshotted":reader.frames,"snapshot_bytes_read":reader.bytes_read,
        "snapshot_peak_buffer_bytes":peak,"snapshot_bytes_retained_in_artifact":0,
        "pixel_state_bytes":3,"visual_pixel_capacity":STATES,"physical_visual_pixels":PIXELS,
        "eis_nodes":NODES,"eis_state_changes":eis.changes,"eis_repeats":eis.repeats,"eis_node_flips":eis.flips,"mode":mode}

def comp(b,n):
    return b if n=="raw" else zlib.compress(b,9) if n=="zlib" else lzma.compress(b,preset=9)
def decomp(b,i):
    n=BACKEND_NAMES[i];return b if n=="raw" else zlib.decompress(b) if n=="zlib" else lzma.decompress(b)

def hdr(sl,ec,uc,dr,db,fr,fb,backend,h):
    b=bytearray(HEADER_BYTES)
    struct.pack_into("<8sHHBBBBIIQQQQQQQ",b,0,MAGIC,VERSION,HEADER_BYTES,BACKENDS[backend],MAX_ORDER,3,0,WIDTH,HEIGHT,sl,ec,uc,dr,db,fr,fb)
    b[80:112]=h;struct.pack_into("<I",b,156,zlib.crc32(b[:156])&0xffffffff);return bytes(b)
def ph(b):
    v=struct.unpack_from("<8sHHBBBBIIQQQQQQQ",b,0)
    magic,ver,hb,bid,mo,digits,_r,w,ht,sl,ec,uc,dr,db,fr,fb=v
    if magic!=MAGIC or ver!=VERSION or hb!=HEADER_BYTES or bid not in BACKEND_NAMES or mo!=2 or digits!=3 or w!=WIDTH or ht!=HEIGHT:raise ValueError("bad header")
    if zlib.crc32(b[:156])&0xffffffff!=struct.unpack_from("<I",b,156)[0]:raise ValueError("CRC")
    return dict(bid=bid,sl=sl,ec=ec,uc=uc,dr=dr,db=db,fr=fr,fb=fb,sha=b[80:112])

def encode(src,backend):
    toks=tokenize(src);dictionary,idsmap,draw=make_dictionary(toks)
    if len(dictionary)>STATES:raise ValueError("lexicon exceeds RGB206 capacity")
    ids=[idsmap[t] for t in toks];fraw,fm=forcing_encode(ids)
    dbody=lzma.compress(draw,preset=9);fbody=comp(fraw,backend)
    art=hdr(len(src),len(ids),len(dictionary),len(draw),len(dbody),len(fraw),len(fbody),backend,sha(src))+dbody+fbody
    s,sm=physics(ids,dictionary,"stream");b,bm=physics(ids,dictionary,"buffer")
    if s!=src or b!=src:raise AssertionError("physics exactness failure")
    ordinary=sum(1 for k,_ in toks if k==WORD);controls=len(toks)-ordinary
    return art,{**fm,"source_bytes":len(src),"button_presses":len(ids),"ordinary_word_buttons":ordinary,"whitespace_override_buttons":controls,
        "unique_buttons":len(dictionary),"dictionary_lzma_bytes":len(dbody),"forcing_backend":backend,"forcing_compressed_bytes":len(fbody),
        "artifact_bytes":len(art),"artifact_ratio_vs_source":len(art)/len(src),"direct_zlib_bytes":len(zlib.compress(src,9)),
        "direct_lzma_bytes":len(lzma.compress(src,preset=9)),"frames_snapshotted":sm["frames_snapshotted"],
        "stream_peak_snapshot_bytes":sm["snapshot_peak_buffer_bytes"],"buffer_peak_snapshot_bytes":bm["snapshot_peak_buffer_bytes"],
        "snapshot_bytes_retained_in_artifact":0,"physics_exact_stream":True,"physics_exact_buffer":True}

def decode(art,mode):
    h=ph(art[:HEADER_BYTES]);d0=HEADER_BYTES;d1=d0+h["db"];f1=d1+h["fb"]
    if f1!=len(art):raise ValueError("length mismatch")
    draw=lzma.decompress(art[d0:d1])
    if len(draw)!=h["dr"]:raise ValueError("dictionary length")
    dictionary=parse_dictionary(draw)
    if len(dictionary)!=h["uc"]:raise ValueError("dictionary count")
    fraw=decomp(art[d1:f1],h["bid"])
    if len(fraw)!=h["fr"]:raise ValueError("forcing length")
    ids=forcing_decode(fraw,h["ec"],h["uc"]);src,m=physics(ids,dictionary,mode)
    if len(src)!=h["sl"] or sha(src)!=h["sha"]:raise ValueError("cold reconstruction mismatch")
    return src,m

def probe(path,prefix):
    with open(path,"rb") as f:src=f.read(prefix) if prefix else f.read()
    r={"source":str(path),"prefix":prefix,"codec_script_bytes":Path(__file__).stat().st_size,"backends":{}}
    for be in BACKENDS:
        art,m=encode(src,be);a,sa=decode(art,"stream");b,sb=decode(art,"buffer")
        if a!=src or b!=src:raise AssertionError
        m["cold_stream_exact"]=m["cold_buffer_exact"]=True;r["backends"][be]=m
    return r

def main():
    a=argparse.ArgumentParser();s=a.add_subparsers(dest="cmd",required=True)
    p=s.add_parser("encode");p.add_argument("source");p.add_argument("artifact");p.add_argument("--backend",choices=BACKENDS,default="lzma")
    p=s.add_parser("decode");p.add_argument("artifact");p.add_argument("output");p.add_argument("--mode",choices=["stream","buffer"],default="stream")
    p=s.add_parser("probe");p.add_argument("source");p.add_argument("--prefix",type=int,default=0)
    x=a.parse_args()
    if x.cmd=="encode":
        art,m=encode(Path(x.source).read_bytes(),x.backend);Path(x.artifact).write_bytes(art);print(json.dumps(m,indent=2,sort_keys=True))
    elif x.cmd=="decode":
        src,m=decode(Path(x.artifact).read_bytes(),x.mode);Path(x.output).write_bytes(src);print(json.dumps({"exact":True,"decoded_bytes":len(src),"sha256":sha(src).hex(),**m},indent=2,sort_keys=True))
    else:print(json.dumps(probe(x.source,x.prefix),indent=2,sort_keys=True))
if __name__=="__main__":main()
