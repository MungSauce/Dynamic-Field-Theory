from __future__ import annotations
import struct,zlib
from signal_field import require_instruments
from signal_packing import PAIR_BITS,SAMPLES_PER_CODE

MAGIC=b"MSFR32A1"
VERSION=32
HEADER_SIZE=184
MAX_ALPHABET=206
HEADER=struct.Struct("<8sHHIHHHHIQQQQ32s32sI56s")
assert HEADER.size==HEADER_SIZE

def make_header(*,N,A,p,block_frames,n,frames,sample_count,payload_bytes,source_sha,payload_sha):
    require_instruments(N)
    vals=[MAGIC,VERSION,HEADER_SIZE,SAMPLES_PER_CODE,N,A,p,PAIR_BITS,block_frames,n,frames,
          sample_count,payload_bytes,source_sha,payload_sha,0,b"\0"*56]
    raw=HEADER.pack(*vals);vals[-2]=zlib.crc32(raw)&0xffffffff
    return HEADER.pack(*vals)

def parse_header(raw:bytes):
    if len(raw)!=HEADER_SIZE:raise ValueError("short header")
    v=list(HEADER.unpack(raw))
    magic,ver,hs,spc,N,A,p,pair_bits,bf,n,frames,samples,pbytes,ss,ph,crc,res=v
    if (magic,ver,hs,spc,pair_bits)!=(MAGIC,VERSION,HEADER_SIZE,SAMPLES_PER_CODE,PAIR_BITS):
        raise ValueError("format")
    require_instruments(N)
    if not (1<=A<=MAX_ALPHABET) or p<A or p*p>65536:raise ValueError("signal geometry")
    v[-2]=0
    if zlib.crc32(HEADER.pack(*v))&0xffffffff!=crc:raise ValueError("header crc")
    return dict(N=N,A=A,p=p,block_frames=bf,n=n,frames=frames,sample_count=samples,
                payload_bytes=pbytes,source_sha=ss,payload_sha=ph)
