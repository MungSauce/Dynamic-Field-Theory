from __future__ import annotations
import math
import numpy as np

def is_prime(n:int)->bool:
    if n<2:return False
    if n%2==0:return n==2
    d=3
    while d*d<=n:
        if n%d==0:return False
        d+=2
    return True

def next_prime_at_least(n:int)->int:
    x=max(3,n)
    if x%2==0:x+=1
    while not is_prime(x):x+=2
    return x

def require_instruments(n:int)->None:
    if n<1 or n>4096 or (n & (n-1)):
        raise ValueError("instrument count must be a power of two in [1,4096]")

def fwht_rows(a:np.ndarray, modulus:int)->np.ndarray:
    """Walsh-Hadamard synthesis/correlation over a finite field.

    Rows are independent timestamps; columns are instruments/chips.
    The Walsh row is the instrument's procedural modifier/timbre.
    """
    if a.ndim!=2: raise ValueError("expected 2D array")
    n=a.shape[1]; require_instruments(n)
    y=np.asarray(a,dtype=np.int64).copy()
    h=1
    while h<n:
        view=y.reshape(y.shape[0],-1,2*h)
        left=view[:,:,:h].copy()
        right=view[:,:,h:].copy()
        view[:,:,:h]=(left+right)%modulus
        view[:,:,h:]=(left-right)%modulus
        h*=2
    return y

def inverse_fwht_rows(samples:np.ndarray, modulus:int)->np.ndarray:
    y=fwht_rows(samples,modulus)
    inv_n=pow(samples.shape[1],-1,modulus)
    return (y*inv_n)%modulus
