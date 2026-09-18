#!/usr/bin/env python3
import json, math, os, wave, tempfile
import numpy as np

C=206
FS=48000

def tone_bank(samples, bits):
    # Two-sample construction is optimized so sample 1 spans the quantizer
    # monotonically across all 206 carrier identities. Every cell is still
    # generated from a sinusoid with a fixed phase reset at the cell boundary.
    phase=-math.pi/2
    maxv=(1<<(bits-1))-1
    target=np.linspace(-0.985,0.985,C)
    omega=np.arccos(-target)
    freqs=omega*FS/(2*math.pi)
    n=np.arange(samples,dtype=np.float64)
    x=np.sin(phase + omega[:,None]*n[None,:])
    q=np.rint(x*maxv).astype(np.int32)
    return freqs,q

def uniqueness(q):
    seen={tuple(r.tolist()) for r in q}
    md=None
    for i in range(len(q)-1):
        d=np.max(np.abs(q[i+1:]-q[i]),axis=1)
        z=int(d.min())
        md=z if md is None else min(md,z)
    return len(seen),md

def trial(samples,bits,nsyms=200000,seed=12345):
    freqs,q=tone_bank(samples,bits)
    u,margin=uniqueness(q)
    rng=np.random.default_rng(seed)
    # Cover all 206 identities repeatedly plus random transitions.
    ids=np.concatenate([np.arange(C,dtype=np.int16),rng.integers(0,C,size=nsyms-C,dtype=np.int16)])
    pcm=q[ids].reshape(-1)
    # Exact digital listener: nearest known tone-cell template.
    rec=[]
    bs=8192
    for p in range(0,len(ids),bs):
        block=pcm[p*samples:min(len(ids),p+bs)*samples].reshape(-1,samples)
        # squared Euclidean nearest-template
        # chunk to avoid large temporary matrices
        for r in block:
            dist=np.sum((q-r)**2,axis=1,dtype=np.int64)
            rec.append(int(np.argmin(dist)))
    rec=np.asarray(rec,dtype=np.int16)
    exact=bool(np.array_equal(ids,rec))
    wav_bytes=44 + len(pcm)*(bits//8)
    source_equiv_bytes=len(ids) # one byte per original byte character
    return {
      "samples_per_position":samples,
      "sample_bits":bits,
      "distinct_tones":u,
      "min_quantized_template_separation":margin,
      "positions":int(len(ids)),
      "exact":exact,
      "wav_bytes":int(wav_bytes),
      "raw_source_bytes":int(source_equiv_bytes),
      "wav_bytes_per_position":wav_bytes/len(ids),
      "expansion_vs_raw":wav_bytes/source_equiv_bytes,
      "positions_per_second":FS/samples,
      "seconds_for_1e9_positions":1e9/(FS/samples),
      "projected_wav_bytes_for_1e9":int(44 + 1e9*samples*(bits//8)),
      "min_freq_hz":float(freqs.min()),
      "max_freq_hz":float(freqs.max()),
    }

def arbitrary_206bit_state(samples=412,bits=16,states=1000,seed=7):
    # Orthogonal FFT-bin control test: 206 independently ON/OFF carriers
    # in the same frame, quantized to PCM, then recovered independently.
    rng=np.random.default_rng(seed)
    maxv=(1<<(bits-1))-1
    errors=0
    for _ in range(states):
        b=rng.integers(0,2,size=C,dtype=np.uint8)
        H=np.zeros(samples//2+1,dtype=np.complex128)
        H[1:C+1]=b*0.8
        x=np.fft.irfft(H,n=samples)
        q=np.rint(np.clip(x,-1,1)*maxv).astype(np.int16)
        R=np.fft.rfft(q.astype(np.float64)/maxv)
        dec=(np.abs(R[1:C+1])>0.3).astype(np.uint8)
        errors += int(np.count_nonzero(dec!=b))
    return {
      "frame_samples":samples,"sample_bits":bits,"carriers":C,
      "states_tested":states,"bit_errors":errors,
      "stored_bits_per_frame":samples*bits,
      "logical_onoff_bits_per_frame":C,
      "stored_bits_per_logical_bit":samples*bits/C
    }

if __name__=="__main__":
    rows=[]
    for bits in (8,16):
        for s in range(1,9):
            rows.append(trial(s,bits,nsyms=30000))
    valid=[r for r in rows if r["exact"] and r["distinct_tones"]==C]
    print(json.dumps({
      "model":"206 concurrent character-page tones over one shared chronology",
      "note":"At each source position exactly one page is ON; all 206 page streams are mixed into the same waveform.",
      "sweep":rows,
      "best_exact_8bit":min((r for r in valid if r["sample_bits"]==8),key=lambda r:r["wav_bytes_per_position"],default=None),
      "best_exact_16bit":min((r for r in valid if r["sample_bits"]==16),key=lambda r:r["wav_bytes_per_position"],default=None),
      "independent_206_onoff_control":arbitrary_206bit_state()
    },indent=2))
