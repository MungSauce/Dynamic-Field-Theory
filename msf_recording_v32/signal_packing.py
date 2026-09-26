from __future__ import annotations
import numpy as np

PAIR_BITS=31
SAMPLES_PER_CODE=2

def _validate_base(signal_base:int):
    if signal_base<2:raise ValueError("signal base")
    if signal_base*signal_base>(1<<PAIR_BITS):raise ValueError("two signal samples do not fit 31-bit code")

def packed_bytes_for_samples(sample_count:int)->int:
    return (((sample_count+1)//2)*PAIR_BITS+7)//8

def pack_samples(samples:np.ndarray,signal_base:int)->bytes:
    """Bit-pack literal recorded signal amplitudes; no source semantics here."""
    _validate_base(signal_base)
    x=np.asarray(samples,dtype=np.uint32).reshape(-1)
    if np.any(x>=signal_base):raise ValueError("signal sample outside alphabet")
    if len(x)&1:x=np.concatenate([x,np.zeros(1,dtype=np.uint32)])
    codes=x[0::2].astype(np.uint64)+signal_base*x[1::2].astype(np.uint64)
    if np.any(codes>=(1<<PAIR_BITS)):raise ValueError("pair code overflow")
    rows=codes.astype("<u4",copy=False).view(np.uint8).reshape(-1,4)
    bits=np.unpackbits(rows,axis=1,bitorder="little")[:,:PAIR_BITS]
    return np.packbits(bits.reshape(-1),bitorder="little").tobytes()

def unpack_samples(raw:bytes,signal_base:int,sample_count:int)->np.ndarray:
    _validate_base(signal_base)
    codes_n=(sample_count+1)//2
    bits=np.unpackbits(np.frombuffer(raw,dtype=np.uint8),bitorder="little")[:codes_n*PAIR_BITS]
    if bits.size!=codes_n*PAIR_BITS:raise ValueError("short packed signal")
    rows=bits.reshape(codes_n,PAIR_BITS)
    rows32=np.zeros((codes_n,32),dtype=np.uint8);rows32[:,:PAIR_BITS]=rows
    codes=np.packbits(rows32,axis=1,bitorder="little").reshape(-1).view("<u4")
    if np.any(codes.astype(np.uint64)>=signal_base*signal_base):raise ValueError("noncanonical packed signal")
    out=np.empty(codes_n*2,dtype=np.uint16)
    out[0::2]=codes%signal_base;out[1::2]=codes//signal_base
    return out[:sample_count]
