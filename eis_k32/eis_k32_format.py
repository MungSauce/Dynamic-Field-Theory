#!/usr/bin/env python3
"""EIS-K32 v1 machine-signal container.

This is not a consumer audio format. It stores a quantized composite signal
whose 32 orthogonal carrier/listener lanes each encode one byte state per frame.
"""
from __future__ import annotations
import hashlib, struct, zlib
from dataclasses import dataclass
from pathlib import Path
import numpy as np

MAGIC=b"EISK32V1"
VERSION=1
HEADER_BYTES=128
LANES=32
SYMBOLS_PER_LANE=256
BASIS_ID_DFT32=1
MODULATION_ID_QAM16X16=1
FLAG_FINAL_PAD=1

@dataclass(frozen=True)
class Header:
    flags:int
    lanes:int
    symbols_per_lane:int
    frame_samples:int
    sample_bits:int
    basis_id:int
    modulation_id:int
    source_length:int
    frame_count:int
    payload_bytes:int
    source_sha256:bytes
    payload_sha256:bytes

def sha256_bytes(data:bytes)->bytes:
    return hashlib.sha256(data).digest()

def carrier_bins(frame_samples:int, lanes:int=LANES)->np.ndarray:
    hi=frame_samples//2-1
    if hi < lanes:
        raise ValueError("frame_samples too small for requested orthogonal real carriers")
    bins=np.rint(np.linspace(1,hi,lanes)).astype(np.int64)
    for i in range(1,len(bins)):
        if bins[i] <= bins[i-1]:
            bins[i]=bins[i-1]+1
    if bins[-1] > hi:
        raise ValueError("carrier-bin collision")
    return bins

def qam16x16_encode(values:np.ndarray)->tuple[np.ndarray,np.ndarray]:
    u=np.asarray(values,dtype=np.uint8)
    hi=(u>>4).astype(np.float64)
    lo=(u&15).astype(np.float64)
    return (2.0*hi/15.0)-1.0, (2.0*lo/15.0)-1.0

def qam16x16_decode(i:np.ndarray,q:np.ndarray)->np.ndarray:
    hi=np.clip(np.rint((np.asarray(i)+1.0)*15.0/2.0),0,15).astype(np.uint8)
    lo=np.clip(np.rint((np.asarray(q)+1.0)*15.0/2.0),0,15).astype(np.uint8)
    return (hi<<4)|lo

def carrier_scale(frame_samples:int, drive:float, lanes:int=LANES)->float:
    return drive*frame_samples/(2.0*lanes*np.sqrt(2.0))

def pack_samples(q:np.ndarray,bits:int)->bytes:
    q=np.asarray(q).reshape(-1)
    if bits==16:
        return q.astype("<i2",copy=False).tobytes()
    raise ValueError("EIS-K32 v1 reference payload currently supports 16-bit samples")

def unpack_samples(raw:bytes,bits:int)->np.ndarray:
    if bits==16:
        if len(raw)%2: raise ValueError("odd 16-bit payload length")
        return np.frombuffer(raw,dtype="<i2").astype(np.int32)
    raise ValueError("EIS-K32 v1 reference payload currently supports 16-bit samples")

def build_header(h:Header)->bytes:
    if len(h.source_sha256)!=32 or len(h.payload_sha256)!=32:
        raise ValueError("sha256 fields must be 32 bytes")
    b=bytearray(HEADER_BYTES)
    struct.pack_into("<8sHHIHHHHHHIQQQ",b,0,
        MAGIC,VERSION,HEADER_BYTES,h.flags,h.lanes,h.symbols_per_lane,
        h.frame_samples,h.sample_bits,h.basis_id,h.modulation_id,0,
        h.source_length,h.frame_count,h.payload_bytes)
    b[56:88]=h.source_sha256
    b[88:120]=h.payload_sha256
    crc=zlib.crc32(bytes(b[:120])) & 0xffffffff
    struct.pack_into("<I",b,120,crc)
    struct.pack_into("<I",b,124,0)
    return bytes(b)

def parse_header(raw:bytes)->Header:
    if len(raw)!=HEADER_BYTES: raise ValueError("short EIS-K32 header")
    magic,version,hbytes,flags,lanes,symbols,frame_samples,sample_bits,basis_id,modulation_id,_r0,source_length,frame_count,payload_bytes=struct.unpack_from("<8sHHIHHHHHHIQQQ",raw,0)
    if magic!=MAGIC or version!=VERSION or hbytes!=HEADER_BYTES:
        raise ValueError("unsupported EIS-K32 header")
    crc=struct.unpack_from("<I",raw,120)[0]
    if crc != (zlib.crc32(raw[:120]) & 0xffffffff):
        raise ValueError("header CRC mismatch")
    return Header(flags,lanes,symbols,frame_samples,sample_bits,basis_id,modulation_id,
                  source_length,frame_count,payload_bytes,raw[56:88],raw[88:120])

def write_file(path:str|Path,header:Header,payload:bytes)->None:
    if len(payload)!=header.payload_bytes:
        raise ValueError("payload length/header mismatch")
    Path(path).write_bytes(build_header(header)+payload)

def read_file(path:str|Path)->tuple[Header,bytes]:
    raw=Path(path).read_bytes()
    if len(raw)<HEADER_BYTES: raise ValueError("short EIS-K32 file")
    h=parse_header(raw[:HEADER_BYTES])
    payload=raw[HEADER_BYTES:]
    if len(payload)!=h.payload_bytes: raise ValueError("payload byte count mismatch")
    if sha256_bytes(payload)!=h.payload_sha256: raise ValueError("payload SHA-256 mismatch")
    return h,payload
