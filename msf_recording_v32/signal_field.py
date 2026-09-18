from __future__ import annotations
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
    x=max(19,n)
    if x%2==0:x+=1
    while not is_prime(x):x+=2
    return x

def require_instruments(n:int)->None:
    if n<1 or n>4096 or (n & (n-1)):
        raise ValueError("instrument count must be a power of two in [1,4096]")

def fwht_rows(a:np.ndarray,modulus:int)->np.ndarray:
    """DSSS/CDMA-like orchestra synthesis using Walsh instrument signatures."""
    if a.ndim!=2:raise ValueError("expected 2D array")
    n=a.shape[1];require_instruments(n)
    y=np.asarray(a,dtype=np.int64).copy();h=1
    while h<n:
        v=y.reshape(y.shape[0],-1,2*h)
        left=v[:,:,:h].copy();right=v[:,:,h:].copy()
        v[:,:,:h]=(left+right)%modulus
        v[:,:,h:]=(left-right)%modulus
        h*=2
    return y

def inverse_fwht_rows(samples:np.ndarray,modulus:int)->np.ndarray:
    """Audience matched-filter/despread operation for the Walsh code bank."""
    y=fwht_rows(samples,modulus)
    return (y*pow(samples.shape[1],-1,modulus))%modulus
