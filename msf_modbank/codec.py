#!/usr/bin/env python3
"""MSF modifier-bank page-orchestra codec.

One instrument owns one deterministic contiguous page. Every instrument uses the
same symbol lexicon. At logical time t, each instrument emits up to two notes:
one read from the front of its page and one from the back. Instrument identity
is procedural: a source-independent modifier plus an invertible shared mixing
field. Only the mixed field is retained in .msf.
"""
from __future__ import annotations
import hashlib, math, mmap, os, struct, zlib
from pathlib import Path

MAGIC=b"MSFMOD01"
VERSION=1
MAX_ALPHABET=206
MAX_DIRECTIONAL_NOTES=412
BLOCK_FRAMES=32
HEADER_SIZE=168
# magic,ver,hdr,flags,N,A,dir_notes,block_frames,n,frames,payload,
# source_sha,payload_sha,crc,reserved
HEADER=struct.Struct("<8sHHIHHHHQQQ32s32sI52s")
assert HEADER.size==HEADER_SIZE

def _sha256_file(path, chunk=8<<20):
    h=hashlib.sha256()
    with open(path,"rb") as f:
        for b in iter(lambda:f.read(chunk),b""): h.update(b)
    return h.digest()

def _scan(path):
    seen=bytearray(256); h=hashlib.sha256(); n=0
    with open(path,"rb") as f:
        for c in iter(lambda:f.read(8<<20),b""):
            n+=len(c); h.update(c)
            for b in set(c): seen[b]=1
    alphabet=bytes(i for i,v in enumerate(seen) if v)
    if not alphabet: raise ValueError("empty source")
    if len(alphabet)>MAX_ALPHABET:
        raise ValueError(f"alphabet {len(alphabet)} exceeds {MAX_ALPHABET}")
    return n,alphabet,h.digest()

def _require_instruments(N):
    if N<1 or N>4096 or N&(N-1):
        raise ValueError("instrument_count must be a power of two in [1,4096]")

def page_bounds(n,N,i):
    return (i*n)//N, ((i+1)*n)//N

def frame_count(n,N):
    return max(((page_bounds(n,N,i)[1]-page_bounds(n,N,i)[0]+1)//2) for i in range(N))

def _mix(v,m):
    """O(N) reversible cumulative field mixer.

    Each stored coordinate after the first depends on all preceding modified
    instrument states, so the payload is not laid out as recognizable lanes.
    """
    y=[]; acc=0
    for x in v:
        acc=(acc+x)%m
        y.append(acc)
    return y

def _unmix(v,m):
    y=[0]*len(v); prev=0
    for i,x in enumerate(v):
        y[i]=(x-prev)%m
        prev=x
    return y

def _modifier(i,m):
    # Generic timbre identity: no per-source table, same law for every file.
    return ((i+1)*(i+7)*17 + 29*i) % m

def _apply_modifier(x,i,m): return (x+_modifier(i,m))%m
def _remove_modifier(x,i,m): return (x-_modifier(i,m))%m

def _bytes_for_digits(base,count):
    return ((pow(base,count)-1).bit_length()+7)//8 if count else 0

def _pack_block(digits,base):
    acc=0
    for x in reversed(digits): acc=acc*base+x
    return acc.to_bytes(_bytes_for_digits(base,len(digits)),"little")

def _unpack_block(raw,base,count):
    acc=int.from_bytes(raw,"little"); out=[]
    for _ in range(count):
        acc,r=divmod(acc,base); out.append(r)
    if acc: raise ValueError("noncanonical field block")
    return out

def _make_header(N,A,n,frames,payload_bytes,source_sha,payload_sha):
    vals=[MAGIC,VERSION,HEADER_SIZE,1,N,A,2*A,BLOCK_FRAMES,n,frames,payload_bytes,
          source_sha,payload_sha,0,b"\0"*52]
    raw=HEADER.pack(*vals); vals[-2]=zlib.crc32(raw)&0xffffffff
    return HEADER.pack(*vals)

def _parse_header(raw):
    v=list(HEADER.unpack(raw))
    magic,ver,hs,flags,N,A,dir_notes,bf,n,frames,pbytes,ss,ph,crc,res=v
    if magic!=MAGIC or ver!=VERSION or hs!=HEADER_SIZE: raise ValueError("format")
    _require_instruments(N)
    if not (1<=A<=MAX_ALPHABET) or dir_notes!=2*A or dir_notes>MAX_DIRECTIONAL_NOTES:
        raise ValueError("lexicon geometry")
    if bf!=BLOCK_FRAMES: raise ValueError("block geometry")
    v[-2]=0
    if zlib.crc32(HEADER.pack(*v))&0xffffffff!=crc: raise ValueError("header crc")
    return dict(N=N,A=A,n=n,frames=frames,pbytes=pbytes,ss=ss,ph=ph)

def compose(source,msf,instrument_count=128):
    _require_instruments(instrument_count)
    source=Path(source); msf=Path(msf)
    n,alphabet,source_sha=_scan(source); A=len(alphabet); B=A*A
    ranks=[0]*256
    for j,b in enumerate(alphabet): ranks[b]=j
    frames=frame_count(n,instrument_count)
    payload_tmp=Path(str(msf)+".payload.tmp")
    ph=hashlib.sha256(); payload_bytes=0; block=[]
    with open(source,"rb") as sf, open(payload_tmp,"wb") as pf:
        mm=mmap.mmap(sf.fileno(),0,access=mmap.ACCESS_READ)
        try:
            for t in range(frames):
                states=[]
                for i in range(instrument_count):
                    s,e=page_bounds(n,instrument_count,i); L=e-s
                    # Page is read inward. Center byte of an odd page is emitted once.
                    if t < (L+1)//2: f=ranks[mm[s+t]]
                    else: f=0
                    if t < L//2: r=ranks[mm[e-1-t]]
                    else: r=0
                    pair=f+A*r
                    states.append(_apply_modifier(pair,i,B))
                block.extend(_mix(states,B))
                if len(block)==BLOCK_FRAMES*instrument_count:
                    raw=_pack_block(block,B); pf.write(raw); ph.update(raw)
                    payload_bytes+=len(raw); block.clear()
            if block:
                raw=_pack_block(block,B); pf.write(raw); ph.update(raw)
                payload_bytes+=len(raw)
        finally:
            mm.close()
    payload_sha=ph.digest()
    with open(msf,"wb") as out, open(payload_tmp,"rb") as pf:
        out.write(_make_header(instrument_count,A,n,frames,payload_bytes,source_sha,payload_sha))
        out.write(alphabet)
        for c in iter(lambda:pf.read(8<<20),b""): out.write(c)
    payload_tmp.unlink()
    return {
        "source_bytes":n,"instrument_count":instrument_count,"page_count":instrument_count,
        "alphabet_size":A,"directional_note_lexicon":2*A,
        "symbols_per_full_timestamp":2*instrument_count,"frame_count":frames,
        "payload_bytes":payload_bytes,"complete_msf_bytes":os.path.getsize(msf),
        "bytes_per_timestamp":payload_bytes/frames,
        "source_bytes_per_timestamp":n/frames,
        "source_sha256":source_sha.hex(),"payload_sha256":payload_sha.hex(),
        "modifier_rule":"procedural_affine_timbre_plus_invertible_butterfly",
        "block_frames":BLOCK_FRAMES,
    }

def listen(msf,output):
    msf=Path(msf); output=Path(output)
    with open(msf,"rb") as f:
        h=_parse_header(f.read(HEADER_SIZE)); alphabet=f.read(h["A"])
        payload=f.read(h["pbytes"])
        if f.read(1): raise ValueError("trailing bytes")
    if len(alphabet)!=h["A"] or len(set(alphabet))!=len(alphabet): raise ValueError("alphabet")
    if hashlib.sha256(payload).digest()!=h["ph"]: raise ValueError("payload hash")
    N,A,n,frames=h["N"],h["A"],h["n"],h["frames"]; B=A*A
    out=bytearray(n); total_digits=frames*N; per_block=BLOCK_FRAMES*N
    off=0; remaining=total_digits; frame_index=0
    while remaining:
        count=min(per_block,remaining); nbytes=_bytes_for_digits(B,count)
        raw=payload[off:off+nbytes]
        if len(raw)!=nbytes: raise ValueError("short payload")
        off+=nbytes; digits=_unpack_block(raw,B,count)
        for k in range(0,len(digits),N):
            mixed=digits[k:k+N]
            if len(mixed)!=N: raise ValueError("partial frame")
            states=_unmix(mixed,B); t=frame_index; frame_index+=1
            for i,x in enumerate(states):
                pair=_remove_modifier(x,i,B); f=pair%A; r=pair//A
                s,e=page_bounds(n,N,i); L=e-s
                if t < (L+1)//2: out[s+t]=alphabet[f]
                if t < L//2: out[e-1-t]=alphabet[r]
        remaining-=count
    if off!=len(payload): raise ValueError("trailing payload")
    with open(output,"wb") as f:f.write(out)
    got=hashlib.sha256(out).digest()
    if got!=h["ss"]: raise ValueError("source hash mismatch")
    return {"recovered_bytes":len(out),"recovered_sha256":got.hex(),"exact":True}

def inspect(msf):
    with open(msf,"rb") as f:
        h=_parse_header(f.read(HEADER_SIZE)); alphabet=f.read(h["A"])
    return {**h,"source_sha256":h["ss"].hex(),"payload_sha256":h["ph"].hex(),
            "complete_msf_bytes":os.path.getsize(msf),"alphabet":alphabet.hex()}
