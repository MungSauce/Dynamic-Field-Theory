from __future__ import annotations
import struct,zlib

MAGIC=b"MSFREC04"
VERSION=4
HEADER_SIZE=184
MAX_ALPHABET=206
# magic,ver,hdr,flags,N,A,sample_bytes,n,frames,payload,source_sha,payload_sha,crc,reserved
HEADER=struct.Struct("<8sHHIHHIQQQ32s32sI68s")
assert HEADER.size==HEADER_SIZE

def make_header(N,A,sample_bytes,n,frames,payload,source_sha,payload_sha):
    vals=[MAGIC,VERSION,HEADER_SIZE,1,N,A,sample_bytes,n,frames,payload,
          source_sha,payload_sha,0,b"\0"*68]
    raw=HEADER.pack(*vals)
    vals[-2]=zlib.crc32(raw)&0xffffffff
    return HEADER.pack(*vals)

def parse_header(raw):
    if len(raw)!=HEADER_SIZE: raise ValueError("short header")
    v=list(HEADER.unpack(raw))
    magic,ver,hs,flags,N,A,sb,n,frames,payload,ss,ps,crc,res=v
    if (magic,ver,hs)!=(MAGIC,VERSION,HEADER_SIZE): raise ValueError("format")
    if N<1 or N>4096 or N&(N-1): raise ValueError("instrument count")
    if not 1<=A<=MAX_ALPHABET: raise ValueError("alphabet")
    v[-2]=0
    if zlib.crc32(HEADER.pack(*v))&0xffffffff!=crc: raise ValueError("header crc")
    return dict(N=N,A=A,sample_bytes=sb,n=n,frames=frames,payload_bytes=payload,
                source_sha=ss,payload_sha=ps)
