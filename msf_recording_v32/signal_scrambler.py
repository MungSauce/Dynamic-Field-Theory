from __future__ import annotations
import math
import numpy as np

GAIN=17
SEED=np.uint64(0x9E3779B97F4A7C15)
MUL=np.uint64(2685821657736338717)

def _mask(start:int,count:int,signal_base:int)->np.ndarray:
    idx=np.arange(start,start+count,dtype=np.uint64)
    x=idx+SEED
    x ^= x >> np.uint64(12)
    x ^= x << np.uint64(25)
    x ^= x >> np.uint64(27)
    x *= MUL
    return (x % np.uint64(signal_base)).astype(np.int64)

def scramble_samples(samples:np.ndarray,start:int,signal_base:int)->np.ndarray:
    """Noise-like chip scrambling applied after all instruments are superposed.

    This sees composite signal amplitudes only. It is source-independent and is
    not encryption; its purpose is to make the retained recording a spread-
    spectrum-like signal that requires the Audience's generic despreading law.
    """
    if math.gcd(GAIN,signal_base)!=1:
        raise ValueError("scrambler gain not invertible")
    x=np.asarray(samples,dtype=np.int64).reshape(-1)
    return ((GAIN*x+_mask(start,len(x),signal_base))%signal_base).astype(np.uint16)

def descramble_samples(samples:np.ndarray,start:int,signal_base:int)->np.ndarray:
    if math.gcd(GAIN,signal_base)!=1:
        raise ValueError("scrambler gain not invertible")
    inv=pow(GAIN,-1,signal_base)
    x=np.asarray(samples,dtype=np.int64).reshape(-1)
    return (inv*(x-_mask(start,len(x),signal_base))%signal_base).astype(np.uint16)
