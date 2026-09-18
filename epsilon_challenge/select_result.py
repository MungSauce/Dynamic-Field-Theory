#!/usr/bin/env python3
from __future__ import annotations
import csv, glob, hashlib, json, pathlib

MECH=pathlib.Path("epsilon_challenge/EPSILON_V7_COMPRESSION_MECHANICS.tsv")
EXPECTED_MECH_SHA="b185ad917e1287cc87c13623a826c0894a0d5a8b0fb2219eafae79a24f375736"
EXPECTED_ROWS=67

def sha256(p):
    h=hashlib.sha256()
    h.update(pathlib.Path(p).read_bytes())
    return h.hexdigest()

rows=[]
with MECH.open(encoding="utf-8") as f:
    for line in f:
        if line.rstrip("\n"):
            parts=line.rstrip("\n").split("\t")
            if len(parts)!=3: raise SystemExit("malformed mechanics projection")
            rows.append(tuple(parts))

if len(rows)!=EXPECTED_ROWS:
    raise SystemExit(f"mechanics row mismatch {len(rows)}")
if sha256(MECH)!=EXPECTED_MECH_SHA:
    raise SystemExit(f"mechanics hash mismatch {sha256(MECH)}")

relations=set(rows)
required={
 ("lossless compression candidate","requires","exact inverse verification"),
 ("lossless compression candidate","requires","complete child size accounting"),
 ("compression decision","should prefer","smallest verified exact candidate"),
 ("adaptive codec selection","means","measure multiple exact candidates and retain the smallest"),
 ("adaptive codec selection","must verify","selected candidate decodes exactly"),
 ("transform chain","requires","inverse transforms applied in reverse order"),
 ("lossless plateau","requires","testing materially applicable transform classes and orderings"),
 ("failed compression candidate","should preserve","negative evidence and failure reason"),
 ("sealed evaluation","must withhold","optimal transform chain"),
 ("sealed evaluation","must withhold","expected encoded bytes and expected size"),
 ("novel transfer","requires","exact reconstruction of unfamiliar source"),
}
missing=sorted(required-relations)
if missing:
    raise SystemExit("required Epsilon compression mechanics missing: "+repr(missing))

variants=[]
for p in sorted(glob.glob("candidate_*.json")):
    doc=json.loads(pathlib.Path(p).read_text())
    variants.extend(doc.get("variants",[]))

passing=[v for v in variants if v.get("exact") and isinstance(v.get("complete_bytes"),int)]
if not passing:
    raise SystemExit("no byte-exact candidate survived")

best=min(passing,key=lambda v:(v["complete_bytes"],v["carrier_bytes"],v["name"]))
failures=[v for v in variants if not v.get("exact")]

report={
 "subject":"Epsilon V7 compression-mechanics projection / EpsilonLCCPOrganV1 decision policy",
 "mechanics_rows":len(rows),
 "mechanics_tsv_sha256":EXPECTED_MECH_SHA,
 "source":"canonical enwik9",
 "source_bytes":1_000_000_000,
 "source_sha256":"159b85351e5f76e60cbe32e04c677847a9ecba3adc79addab6f4c6c7aa3744bc",
 "decision_basis":[
   "exact inverse verification",
   "complete child size accounting",
   "adaptive candidate measurement",
   "reversible chain composition",
   "select smallest verified exact candidate",
   "preserve failed candidates as negative evidence"
 ],
 "all_variants":variants,
 "selected":best,
 "failed_variants":failures,
 "tested_plateau_only":True,
 "global_optimum_claim":False,
 "tool_authorship_boundary":"Candidate executables/workshop orchestration are external affordances. Selection/accept-reject policy is the preserved Epsilon compression-mechanics state. No claim that current Epsilon generated the tool source code."
}
pathlib.Path("EPSILON_V7_ENWIK9_OPEN_RESULT.json").write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
