#!/usr/bin/env python3
from __future__ import annotations
import hashlib, json, math, os, struct, sys, tempfile, time, zlib
from pathlib import Path

MAGIC=b'MSFCMP01'
VERSION=1
HEADER_SIZE=160
INSTRUMENTS=32
DECLARED_PITCHES=412
MAX_TERMINALS=206
PAGE_SIZE=1_000_000
BLOCK_FRAMES=64
HEADER=struct.Struct('<8sHHIHHHIIQQQQ32s32sI24s')
assert HEADER.size==HEADER_SIZE

def sha256_file(path,chunk=8<<20):
    h=hashlib.sha256()
    with open(path,'rb') as f:
        for b in iter(lambda:f.read(chunk),b''): h.update(b)
    return h.digest()

def scan(path):
    seen=bytearray(256); h=hashlib.sha256(); n=0
    with open(path,'rb') as f:
        for c in iter(lambda:f.read(8<<20),b''):
            n+=len(c); h.update(c)
            for b in set(c): seen[b]=1
    alphabet=bytes(i for i,v in enumerate(seen) if v)
    if len(alphabet)>MAX_TERMINALS:
        raise ValueError(f'alphabet {len(alphabet)} > {MAX_TERMINALS}')
    return n,alphabet,h.digest()

def frame_count_for(n):
    total=0
    while n:
        L=min(PAGE_SIZE,n)
        total+=(L+63)//64
        n-=L
    return total

def iter_pair_digits(src,alphabet):
    rank=[0]*256
    for i,b in enumerate(alphabet): rank[b]=i
    A=len(alphabet)
    with open(src,'rb') as f:
        while True:
            page=f.read(PAGE_SIZE)
            if not page: break
            L=len(page); hf=(L+1)//2; hr=L//2
            for t in range((L+63)//64):
                base=t*32
                for i in range(32):
                    k=base+i
                    fv=rank[page[k]] if k<hf else 0
                    rv=rank[page[L-1-k]] if k<hr else 0
                    yield fv + A*rv

def bytes_for_digits(base,count):
    if count<=0:return 0
    maxv=pow(base,count)-1
    return (maxv.bit_length()+7)//8

def pack_payload(src,alphabet,out):
    A=len(alphabet); B=A*A
    digits_per_block=BLOCK_FRAMES*INSTRUMENTS
    h=hashlib.sha256(); total=0; buf=[]
    for d in iter_pair_digits(src,alphabet):
        buf.append(d)
        if len(buf)==digits_per_block:
            acc=0
            for x in reversed(buf): acc=acc*B+x
            nbytes=bytes_for_digits(B,len(buf))
            raw=acc.to_bytes(nbytes,'little')
            out.write(raw);h.update(raw);total+=len(raw);buf.clear()
    if buf:
        acc=0
        for x in reversed(buf): acc=acc*B+x
        nbytes=bytes_for_digits(B,len(buf))
        raw=acc.to_bytes(nbytes,'little')
        out.write(raw);h.update(raw);total+=len(raw)
    return total,h.digest()

def header_pack(A,n,pages,frames,payload_bytes,source_sha,payload_sha):
    B=A*A
    vals=[MAGIC,VERSION,HEADER_SIZE,1,INSTRUMENTS,DECLARED_PITCHES,A,PAGE_SIZE,BLOCK_FRAMES,n,pages,frames,payload_bytes,source_sha,payload_sha,0,b'\0'*24]
    raw=HEADER.pack(*vals)
    vals[-2]=zlib.crc32(raw)&0xffffffff
    return HEADER.pack(*vals)

def header_parse(raw):
    v=list(HEADER.unpack(raw))
    magic,ver,hs,flags,inst,pitches,A,page_size,block_frames,n,pages,frames,pbytes,ss,ph,crc,res=v
    if (magic,ver,hs,inst,pitches,page_size,block_frames)!=(MAGIC,VERSION,HEADER_SIZE,INSTRUMENTS,DECLARED_PITCHES,PAGE_SIZE,BLOCK_FRAMES):
        raise ValueError('header geometry')
    if not (1<=A<=MAX_TERMINALS):raise ValueError('alphabet')
    v[-2]=0
    if zlib.crc32(HEADER.pack(*v))&0xffffffff!=crc:raise ValueError('header crc')
    return dict(A=A,n=n,pages=pages,frames=frames,pbytes=pbytes,ss=ss,ph=ph)

def compose(src,msf):
    n,alphabet,ss=scan(src)
    A=len(alphabet);frames=frame_count_for(n);pages=(n+PAGE_SIZE-1)//PAGE_SIZE
    tmp=Path(str(msf)+'.payload.tmp')
    with open(tmp,'wb') as pf:pbytes,ph=pack_payload(src,alphabet,pf)
    with open(msf,'wb') as out,open(tmp,'rb') as pf:
        out.write(header_pack(A,n,pages,frames,pbytes,ss,ph));out.write(alphabet)
        for c in iter(lambda:pf.read(8<<20),b''):out.write(c)
    tmp.unlink()
    theoretical=frames*INSTRUMENTS*math.log2(A*A)/8
    return dict(source_bytes=n,alphabet_size=A,instruments=INSTRUMENTS,pitch_capacity=DECLARED_PITCHES,
                page_count=pages,frame_count=frames,payload_bytes=pbytes,complete_msf_bytes=os.path.getsize(msf),
                theoretical_field_bytes=theoretical,source_sha256=ss.hex(),payload_sha256=ph.hex())

def digit_blocks(payload,A,frames):
    B=A*A; total_digits=frames*INSTRUMENTS; block_digits=BLOCK_FRAMES*INSTRUMENTS
    off=0;remaining=total_digits
    while remaining:
        count=min(block_digits,remaining); nbytes=bytes_for_digits(B,count)
        raw=payload[off:off+nbytes]
        if len(raw)!=nbytes:raise ValueError('short payload')
        off+=nbytes;acc=int.from_bytes(raw,'little');vals=[]
        for _ in range(count):
            acc,r=divmod(acc,B);vals.append(r)
        if acc:raise ValueError('noncanonical composite block')
        yield vals
        remaining-=count
    if off!=len(payload):raise ValueError('trailing payload')

def listen(msf,outpath):
    with open(msf,'rb') as f:
        h=header_parse(f.read(HEADER_SIZE));alphabet=f.read(h['A']);payload=f.read(h['pbytes'])
        if len(alphabet)!=h['A'] or len(set(alphabet))!=len(alphabet):raise ValueError('alphabet map')
        if f.read(1):raise ValueError('trailing file data')
    if hashlib.sha256(payload).digest()!=h['ph']:raise ValueError('payload hash')
    A=h['A']; out=bytearray(h['n']);block_iter=digit_blocks(payload,A,h['frames']);current=[];idx=0
    def next_digit():
        nonlocal current,idx
        if idx>=len(current):
            current=next(block_iter);idx=0
        d=current[idx];idx+=1;return d
    off=0;left=h['n']
    while left:
        L=min(PAGE_SIZE,left);hf=(L+1)//2;hr=L//2
        for t in range((L+63)//64):
            base=t*32
            for i in range(32):
                x=next_digit();fv=x%A;rv=x//A;k=base+i
                if k<hf:out[off+k]=alphabet[fv]
                if k<hr:out[off+L-1-k]=alphabet[rv]
        off+=L;left-=L
    with open(outpath,'wb') as f:f.write(out)
    got=hashlib.sha256(out).digest()
    if got!=h['ss']:raise ValueError('recovered sha mismatch')
    return got.hex()

def main():
    src=Path(sys.argv[1] if len(sys.argv)>1 else 'enwik100m')
    msf=Path('enwik100m_refined.msf');rec=Path('recovered_enwik100m_refined')
    t=time.perf_counter();r=compose(src,msf);t1=time.perf_counter()
    source_sha=r['source_sha256'];os.remove(src)
    recovered=listen(msf,rec);t2=time.perf_counter()
    r.update(recovered_sha256=recovered,source_removed_before_decode=True,exact_reconstruction=(source_sha==recovered),
             complete_ratio=r['complete_msf_bytes']/r['source_bytes'],
             compression_percent=100*(1-r['complete_msf_bytes']/r['source_bytes']),
             bytes_per_frame=r['payload_bytes']/r['frame_count'],
             source_bytes_per_frame=r['source_bytes']/r['frame_count'],
             compose_seconds=t1-t,listen_seconds=t2-t1,
             status='PASS' if source_sha==recovered else 'FAIL')
    Path('msf_100mb_refined_report.json').write_text(json.dumps(r,indent=2)+'\n')
    print(json.dumps(r,indent=2))
if __name__=='__main__':main()
