from __future__ import annotations
import hashlib, os, struct, zlib
from pathlib import Path
from signal_field import require_instruments

MAGIC=b"MSFREC03"
VERSION=3
HEADER_SIZE=184
MAX_ALPHABET=206
SAMPLE_BITS=16
# magic,ver,hdr,flags,N,A,p,bits,block_frames,n,frames,samples,payload,
# source_sha,payload_sha,crc,reserved
HEADER=struct.Struct("<8sHHIHHHHIQQQQ32s32sI56s")
assert HEADER.size==HEADER_SIZE

def make_header(*,N,A,p,block_frames,n,frames,sample_count,payload_bytes,source_sha,payload_sha):
    require_instruments(N)
    vals=[MAGIC,VERSION,HEADER_SIZE,1,N,A,p,SAMPLE_BITS,block_frames,n,frames,
          sample_count,payload_bytes,source_sha,payload_sha,0,b"\0"*56]
    raw=HEADER.pack(*vals)
    vals[-2]=zlib.crc32(raw)&0xffffffff
    return HEADER.pack(*vals)

def parse_header(raw:bytes):
    if len(raw)!=HEADER_SIZE: raise ValueError("short header")
    v=list(HEADER.unpack(raw))
    magic,ver,hs,flags,N,A,p,bits,bf,n,frames,samples,pbytes,ss,ph,crc,res=v
    if (magic,ver,hs,bits)!=(MAGIC,VERSION,HEADER_SIZE,SAMPLE_BITS):
        raise ValueError("unsupported format")
    require_instruments(N)
    if not (1<=A<=MAX_ALPHABET): raise ValueError("alphabet geometry")
    if p<A or p*p>65536: raise ValueError("signal modulus")
    v[-2]=0
    if zlib.crc32(HEADER.pack(*v))&0xffffffff!=crc: raise ValueError("header crc")
    return dict(N=N,A=A,p=p,block_frames=bf,n=n,frames=frames,
                sample_count=samples,payload_bytes=pbytes,
                source_sha=ss,payload_sha=ph)

def hash_file(path,chunk=8<<20):
    h=hashlib.sha256()
    with open(path,"rb") as f:
        for b in iter(lambda:f.read(chunk),b""):h.update(b)
    return h.digest()
