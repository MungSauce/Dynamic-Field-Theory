#!/usr/bin/env python3
import random,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"reference"))
from qompress_ref.qstate import unpack_node
from qompress_ref.chronology_v2 import ChronologyCounter,PRIMED_ZERO_NODE
from qompress_ref.reflex_v2 import ReflexField
from qompress_ref.seed_v2 import encode_reflex,decode_reflex,rollback_to_primed_zero,compile_best_seed,seed_stats

def check(c,m):
    if not c: raise AssertionError(m)

check(PRIMED_ZERO_NODE==0x55,"primed zero byte")
check(tuple(int(x) for x in unpack_node(PRIMED_ZERO_NODE))==(1,1,1,1),"primed zero q-cells")
print("R1_PRIMED_ZERO=PASS")

clock=ChronologyCounter(2)
for i in range(1,10000):
    clock.advance(); check(clock.index()==i,"chronology index")
for i in range(9999,0,-1):
    check(clock.index()==i,"reverse index"); clock.reverse()
check(clock.is_primed_zero(),"chronology origin")
print("R2_CHRONOLOGY_REVERSAL=PASS")

f=ReflexField(11); page=bytes(range(11)); d=f.ingest(page)
for i,x in enumerate(page):
    check(f.button(i,x),"true button")
    check(sum(1 for c in range(256) if f.button(i,c))==1,"one true button")
f.rollback(d); check(f.is_primed_zero(),"field origin")
print("R3_BUTTON_RELATION=PASS")

rng=random.Random(90210)
for n in [0,1,31,257,4096,10000]:
    raw=bytes(rng.randrange(256) for _ in range(n)); seed=encode_reflex(raw,257,4)
    check(decode_reflex(seed)==raw,f"roundtrip {n}"); check(rollback_to_primed_zero(seed),f"rollback {n}")
print("R4_ARBITRARY_EXACT_REVERSAL=PASS")

page=bytes((i*7)&255 for i in range(256)); raw=page*32; seed=encode_reflex(raw,256,2); st=seed_stats(seed)
check(st["changed_nodes"]<=256,"redundant deltas"); check(st["derived_unchanged_nodes"]>=31*256,"reuse")
print("R5_DERIVED_RELATIONAL_REUSE=PASS")

raw=(b"The same page repeats.\n"*40)*50
seed,ps,trials=compile_best_seed(raw,candidates=(64,128,256,512,1024),chrono_nodes=3)
check(len(seed)==min(s for s,_ in trials),"compiler"); check(decode_reflex(seed)==raw,"compiled roundtrip")
print("R6_SEED_COMPILER=PASS")

raw=b"abcdef"*100; seed=encode_reflex(raw,64,2)
for bad in [seed[:-1],b"BAD!"+seed[4:]]:
    try: decode_reflex(bad)
    except ValueError: pass
    else: raise AssertionError("malformed seed accepted")
print("R7_CORRUPTION_REJECTION=PASS")
print("QOMPRESS_REFLEX_V2_CONFORMANCE=PASS")
