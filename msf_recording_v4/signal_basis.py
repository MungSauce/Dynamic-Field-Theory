from __future__ import annotations
import math

def page_bounds(n,N,i):
    return (i*n)//N,((i+1)*n)//N

def frame_count(n,N):
    return max((page_bounds(n,N,i)[1]-page_bounds(n,N,i)[0]+1)//2 for i in range(N))

def bit_reverse(i,bits):
    r=0
    for _ in range(bits):
        r=(r<<1)|(i&1);i>>=1
    return r

def instrument_slot(i,N):
    # Procedural basis/timbre placement; no stored table.
    return bit_reverse(i,N.bit_length()-1)

def modifier_offset(i,B):
    # Source-independent modifier of the shared note-pair lexicon.
    return ((i+1)*(i+7)*17 + 29*i + 11) % B

def apply_modifier(x,i,B):
    return (x+modifier_offset(i,B))%B

def remove_modifier(y,i,B):
    return (y-modifier_offset(i,B))%B

def sample_bytes_for(B,N):
    # Exact serialized width of one scalar composite sample.
    return ((pow(B,N)-1).bit_length()+7)//8


def mix_field(v,B):
    """Reversible IIR/filter-bank style orchestra mix.

    Each output after the first depends recursively on the previous mixed
    coefficient, so the recording does not preserve page-aligned states.
    """
    y=[0]*len(v);acc=0
    for i,x in enumerate(v):
        alpha=(2*i+3)%B
        acc=(x + alpha*acc) % B
        y[i]=acc
    return y

def unmix_field(v,B):
    y=[0]*len(v);prev=0
    for i,w in enumerate(v):
        alpha=(2*i+3)%B
        y[i]=(w - alpha*prev) % B
        prev=w
    return y

def spectral_permute(v):
    N=len(v)
    return [v[instrument_slot(i,N)] for i in range(N)]

def spectral_unpermute(v):
    N=len(v);out=[0]*N
    for i in range(N):out[instrument_slot(i,N)]=v[i]
    return out
