import struct
from dataclasses import dataclass
from .chronology_v2 import ChronologyCounter, PRIMED_ZERO_NODE
from .reflex_v2 import ReflexField, ReflexDelta
from .varint import encode_uvarint, decode_uvarint

MAGIC=b"QRF2"; VERSION=2
HEADER=struct.Struct("<4sBIIQ")
RECORD_HEADER=struct.Struct("<BI")
MODE_SPARSE=0; MODE_DENSE=1

@dataclass(frozen=True)
class PageSeed:
    logical_length:int
    deltas:tuple
    mode:int

@dataclass(frozen=True)
class ParsedSeed:
    page_size:int
    chrono_nodes:int
    original_size:int
    pages:tuple

def _pad(chunk,page_size):
    return bytes(chunk)+bytes([PRIMED_ZERO_NODE])*(page_size-len(chunk))

def _sparse_payload(deltas):
    out=bytearray(encode_uvarint(len(deltas))); prev=-1
    for d in deltas:
        out+=encode_uvarint(d.position-prev-1); out.append(d.xor_delta); prev=d.position
    return bytes(out)

def _dense_payload(deltas,width):
    bitmap=bytearray((width+7)//8); values=bytearray()
    for d in deltas:
        bitmap[d.position>>3]|=1<<(d.position&7); values.append(d.xor_delta)
    return bytes(bitmap+values)

def _encode_page_seed(page_seed,page_size):
    sparse=_sparse_payload(page_seed.deltas); dense=_dense_payload(page_seed.deltas,page_size)
    mode,payload=(MODE_SPARSE,sparse) if len(sparse)<=len(dense) else (MODE_DENSE,dense)
    return RECORD_HEADER.pack(mode,page_seed.logical_length)+encode_uvarint(len(payload))+payload

def encode_reflex(data,page_size=4096,chrono_nodes=8):
    data=bytes(data)
    if not 1<=page_size<=0xFFFFFFFF or not 1<=chrono_nodes<=0xFFFFFFFF:
        raise ValueError("invalid dimensions")
    field=ReflexField(page_size); clock=ChronologyCounter(chrono_nodes)
    out=bytearray(HEADER.pack(MAGIC,VERSION,page_size,chrono_nodes,len(data)))
    for off in range(0,len(data),page_size):
        chunk=data[off:off+page_size]; page=_pad(chunk,page_size)
        deltas=tuple(field.ingest(page))
        out+=_encode_page_seed(PageSeed(len(chunk),deltas,MODE_SPARSE),page_size)
        clock.advance()
    return bytes(out)

def _parse_sparse(payload,width):
    count,offset=decode_uvarint(payload,0); deltas=[]; pos=-1
    for _ in range(count):
        gap,offset=decode_uvarint(payload,offset); pos+=gap+1
        if pos>=width or offset>=len(payload): raise ValueError("bad sparse payload")
        d=payload[offset]; offset+=1
        if d==0: raise ValueError("zero delta")
        deltas.append(ReflexDelta(pos,d))
    if offset!=len(payload): raise ValueError("trailing sparse payload")
    return tuple(deltas)

def _parse_dense(payload,width):
    bmlen=(width+7)//8
    if len(payload)<bmlen: raise ValueError("truncated dense bitmap")
    bitmap=payload[:bmlen]; values=payload[bmlen:]; positions=[]
    for pos in range(width):
        if bitmap[pos>>3]&(1<<(pos&7)): positions.append(pos)
    if len(values)!=len(positions) or any(v==0 for v in values):
        raise ValueError("bad dense payload")
    return tuple(ReflexDelta(p,v) for p,v in zip(positions,values))

def parse_seed(blob):
    blob=bytes(blob)
    if len(blob)<HEADER.size: raise ValueError("truncated header")
    magic,version,page_size,chrono_nodes,original_size=HEADER.unpack_from(blob,0)
    if magic!=MAGIC or version!=VERSION: raise ValueError("unsupported reflex seed")
    if page_size==0 or chrono_nodes==0: raise ValueError("invalid dimensions")
    pages=[]; offset=HEADER.size; emitted=0
    while offset<len(blob):
        if offset+RECORD_HEADER.size>len(blob): raise ValueError("truncated record header")
        mode,logical_length=RECORD_HEADER.unpack_from(blob,offset); offset+=RECORD_HEADER.size
        if logical_length==0 or logical_length>page_size: raise ValueError("invalid logical length")
        payload_len,offset=decode_uvarint(blob,offset); end=offset+payload_len
        if end>len(blob): raise ValueError("truncated record payload")
        payload=blob[offset:end]; offset=end
        if mode==MODE_SPARSE: deltas=_parse_sparse(payload,page_size)
        elif mode==MODE_DENSE: deltas=_parse_dense(payload,page_size)
        else: raise ValueError("unknown page mode")
        pages.append(PageSeed(logical_length,deltas,mode)); emitted+=logical_length
    if emitted!=original_size and not (original_size==0 and emitted==0):
        raise ValueError("original size mismatch")
    if len(pages)>=256**chrono_nodes: raise ValueError("chronology capacity exceeded")
    return ParsedSeed(page_size,chrono_nodes,original_size,tuple(pages))

def decode_reflex(blob):
    parsed=parse_seed(blob); field=ReflexField(parsed.page_size); clock=ChronologyCounter(parsed.chrono_nodes); out=bytearray()
    for rec in parsed.pages:
        field.apply(rec.deltas); out+=field.snapshot()[:rec.logical_length]; clock.advance()
    if len(out)!=parsed.original_size: raise ValueError("decoded length mismatch")
    return bytes(out)

def rollback_to_primed_zero(blob):
    parsed=parse_seed(blob); field=ReflexField(parsed.page_size); clock=ChronologyCounter(parsed.chrono_nodes)
    for rec in parsed.pages: field.apply(rec.deltas); clock.advance()
    for rec in reversed(parsed.pages): field.rollback(rec.deltas); clock.reverse()
    return field.is_primed_zero() and clock.is_primed_zero()

def seed_stats(blob):
    parsed=parse_seed(blob); changes=sum(len(p.deltas) for p in parsed.pages); slots=len(parsed.pages)*parsed.page_size
    return {"source_bytes":parsed.original_size,"seed_bytes":len(blob),"page_size":parsed.page_size,"pages":len(parsed.pages),"changed_nodes":changes,"total_node_observations":slots,"derived_unchanged_nodes":slots-changes,"change_density":changes/slots if slots else 0.0,"roundtrip_ratio":len(blob)/parsed.original_size if parsed.original_size else 0.0}

def compile_best_seed(data,candidates=(64,128,256,512,1024,2048,4096,8192,16384),chrono_nodes=8):
    trials=[]
    for ps in candidates:
        if ps>0:
            seed=encode_reflex(data,ps,chrono_nodes); trials.append((len(seed),ps,seed))
    if not trials: raise ValueError("no page-size candidates")
    trials.sort(key=lambda x:(x[0],x[1]))
    _,ps,seed=trials[0]
    return seed,ps,[(size,page) for size,page,_ in trials]
