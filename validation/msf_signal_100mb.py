#!/usr/bin/env python3
from __future__ import annotations
import hashlib, json, math, os, struct, sys, time, zlib
from pathlib import Path

MAGIC=b'MSFSIG01'; VERSION=1; HEADER_SIZE=160
INSTRUMENTS=32; MAX_TERMINALS=206; DECLARED_PITCHES=412; PAGE_SIZE=1_000_000
HEADER=struct.Struct('<8sHHIHHH I Q Q Q I H Q 32s32s I 28s')
assert HEADER.size == HEADER_SIZE

def sha256_file(path, chunk=8<<20):
    h=hashlib.sha256()
    with open(path,'rb') as f:
        for b in iter(lambda:f.read(chunk),b''): h.update(b)
    return h.digest()

def next_prime(n):
    def prime(x):
        if x<2:return False
        if x%2==0:return x==2
        d=3; r=math.isqrt(x)
        while d<=r:
            if x%d==0:return False
            d+=2
        return True
    x=max(2,n+1)
    if x>2 and x%2==0:x+=1
    while not prime(x):x+=2
    return x

def had(v,q):
    a=list(v);h=1
    while h<len(a):
        for i in range(0,len(a),2*h):
            for j in range(i,i+h):
                x=a[j];y=a[j+h];a[j]=(x+y)%q;a[j+h]=(x-y)%q
        h*=2
    return a

def ihad(v,q):
    a=had(v,q); inv=pow(len(v),-1,q)
    return [(x*inv)%q for x in a]

def scan(path):
    seen=bytearray(256);h=hashlib.sha256();n=0
    with open(path,'rb') as f:
        for c in iter(lambda:f.read(8<<20),b''):
            n+=len(c);h.update(c)
            for b in set(c):seen[b]=1
    alphabet=bytes(i for i,v in enumerate(seen) if v)
    if len(alphabet)>MAX_TERMINALS:raise ValueError(f'alphabet {len(alphabet)} > {MAX_TERMINALS}')
    return n,alphabet,h.digest()

def iter_frames(src,alphabet,q):
    rank=[0]*256
    for i,b in enumerate(alphabet):rank[b]=i
    A=len(alphabet)
    with open(src,'rb') as f:
        while True:
            page=f.read(PAGE_SIZE)
            if not page:break
            L=len(page);hf=(L+1)//2;hr=L//2
            for t in range((L+63)//64):
                base=t*32; coeff=[]
                for i in range(32):
                    k=base+i
                    fv=rank[page[k]] if k<hf else 0
                    rv=rank[page[L-1-k]] if k<hr else 0
                    # instrument i simultaneously carries forward and reverse notes.
                    # Pair identity is source state; Hadamard output is the stored mixed field.
                    coeff.append(fv+A*rv)
                yield had(coeff,q)

def pack_frames(frames,q,out):
    one=math.ceil(32*math.log2(q)/8); two=math.ceil(64*math.log2(q)/8)
    pending=None;h=hashlib.sha256();total=0
    for fr in frames:
        if pending is None:pending=fr;continue
        acc=0
        for x in reversed(pending+fr):acc=acc*q+x
        b=acc.to_bytes(two,'little');out.write(b);h.update(b);total+=len(b);pending=None
    if pending is not None:
        acc=0
        for x in reversed(pending):acc=acc*q+x
        b=acc.to_bytes(one,'little');out.write(b);h.update(b);total+=len(b)
    return total,h.digest()

def unpack_frames(payload,q,count):
    one=math.ceil(32*math.log2(q)/8); two=math.ceil(64*math.log2(q)/8)
    off=0;done=0
    while done+2<=count:
        acc=int.from_bytes(payload[off:off+two],'little');off+=two;vals=[]
        for _ in range(64):acc,r=divmod(acc,q);vals.append(r)
        if acc:raise ValueError('noncanonical field block')
        yield vals[:32];yield vals[32:];done+=2
    if done<count:
        acc=int.from_bytes(payload[off:off+one],'little');off+=one;vals=[]
        for _ in range(32):acc,r=divmod(acc,q);vals.append(r)
        if acc:raise ValueError('noncanonical final block')
        yield vals;done+=1
    if off!=len(payload):raise ValueError('trailing payload')

def make_header(A,n,pages,frames,q,pbytes,ss,ph):
    vals=[MAGIC,VERSION,HEADER_SIZE,1,INSTRUMENTS,DECLARED_PITCHES,A,PAGE_SIZE,n,pages,frames,q,2,pbytes,ss,ph,0,b'\0'*28]
    raw=HEADER.pack(*vals);vals[-2]=zlib.crc32(raw)&0xffffffff
    return HEADER.pack(*vals)

def parse_header(raw):
    v=list(HEADER.unpack(raw))
    magic,ver,hs,flags,inst,pitches,A,ps,n,pages,frames,q,fpp,pbytes,ss,ph,crc,res=v
    if (magic,ver,hs,inst,pitches,ps,fpp)!=(MAGIC,VERSION,HEADER_SIZE,INSTRUMENTS,DECLARED_PITCHES,PAGE_SIZE,2):raise ValueError('geometry')
    v[-2]=0
    if zlib.crc32(HEADER.pack(*v))&0xffffffff!=crc:raise ValueError('header crc')
    return dict(A=A,n=n,pages=pages,frames=frames,q=q,pbytes=pbytes,ss=ss,ph=ph)

def compose(src,msf):
    n,alphabet,ss=scan(src);A=len(alphabet);q=next_prime(A*A-1)
    pages=(n+PAGE_SIZE-1)//PAGE_SIZE;left=n;frames=0
    while left:
        L=min(PAGE_SIZE,left);frames+=(L+63)//64;left-=L
    tmp=Path(str(msf)+'.payload.tmp')
    with open(tmp,'wb') as pf:pbytes,ph=pack_frames(iter_frames(src,alphabet,q),q,pf)
    with open(msf,'wb') as out,open(tmp,'rb') as pf:
        out.write(make_header(A,n,pages,frames,q,pbytes,ss,ph));out.write(alphabet)
        for c in iter(lambda:pf.read(8<<20),b''):out.write(c)
    tmp.unlink()
    return dict(source_bytes=n,alphabet_size=A,instruments=32,pitch_capacity=412,page_count=pages,frame_count=frames,field_modulus=q,payload_bytes=pbytes,complete_msf_bytes=os.path.getsize(msf),source_sha256=ss.hex(),payload_sha256=ph.hex())

def listen(msf,outpath):
    with open(msf,'rb') as f:
        h=parse_header(f.read(HEADER_SIZE));alphabet=f.read(h['A']);payload=f.read(h['pbytes'])
        if f.read(1):raise ValueError('trailing file data')
    if hashlib.sha256(payload).digest()!=h['ph']:raise ValueError('payload hash')
    frames=unpack_frames(payload,h['q'],h['frames']);A=h['A'];out=bytearray(h['n'])
    off=0;left=h['n']
    while left:
        L=min(PAGE_SIZE,left);hf=(L+1)//2;hr=L//2
        for t in range((L+63)//64):
            coeff=ihad(next(frames),h['q']);base=t*32
            for i,x in enumerate(coeff):
                if x>=A*A:raise ValueError('field inverse outside note-pair state')
                fv=x%A;rv=x//A;k=base+i
                if k<hf:out[off+k]=alphabet[fv]
                if k<hr:out[off+L-1-k]=alphabet[rv]
        off+=L;left-=L
    with open(outpath,'wb') as f:f.write(out)
    got=hashlib.sha256(out).digest()
    if got!=h['ss']:raise ValueError('recovered sha mismatch')
    return got.hex()

def main():
    src=Path(sys.argv[1] if len(sys.argv)>1 else 'enwik100m')
    msf=Path('enwik100m.msf');rec=Path('recovered_enwik100m')
    t=time.perf_counter();r=compose(src,msf);t1=time.perf_counter()
    source_sha=r['source_sha256'];os.remove(src)
    recovered=listen(msf,rec);t2=time.perf_counter()
    r.update(recovered_sha256=recovered,source_removed_before_decode=True,exact_reconstruction=(source_sha==recovered),complete_ratio=r['complete_msf_bytes']/r['source_bytes'],compression_percent=100*(1-r['complete_msf_bytes']/r['source_bytes']),compose_seconds=t1-t,listen_seconds=t2-t1,status='PASS' if source_sha==recovered else 'FAIL')
    Path('msf_100mb_report.json').write_text(json.dumps(r,indent=2)+'\n')
    print(json.dumps(r,indent=2))
if __name__=='__main__':main()
