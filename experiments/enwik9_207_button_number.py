#!/usr/bin/env python3
import hashlib, zipfile, urllib.request, json, os
URL="https://mattmahoney.net/dc/enwik9.zip"
ZIP="enwik9.zip"; RAW="enwik9"
EXPECTED_SHA256="159b85351e5f76e60cbe32e04c677847a9ecba3adc79addab6f4c6c7aa3744bc"
EXPECTED_MD5="e206c3450ac99950df65bf70ef61a12d"
if not os.path.exists(RAW):
    urllib.request.urlretrieve(URL,ZIP)
    with zipfile.ZipFile(ZIP) as z, z.open("enwik9") as src, open(RAW,"wb") as out:
        while True:
            x=src.read(8<<20)
            if not x: break
            out.write(x)
data=open(RAW,"rb").read()
assert len(data)==1_000_000_000
assert hashlib.sha256(data).hexdigest()==EXPECTED_SHA256
assert hashlib.md5(data).hexdigest()==EXPECTED_MD5
vals=sorted(set(data))
assert len(vals)==206, len(vals)
unused=[x for x in range(256) if x not in vals]
sep_byte=unused[0]
button={v:i+1 for i,v in enumerate(vals)}
# Exact token target: b(c1),207,b(c2),207,...,b(c_n),207.
# Exact single integer target is the same token sequence interpreted in base 208.
# N = fold(N*208 + token).  No source character is a base-208 digit; only button labels are.
h=hashlib.sha256(); n_tokens=0; prefix=[]; suffix=[]
N_mod_2_256=0
MOD=1<<256
for c in data:
    for t in (button[c],207):
        bs=t.to_bytes(2,"big")
        h.update(bs); n_tokens+=1
        if len(prefix)<40: prefix.append(t)
        suffix.append(t)
        if len(suffix)>40: suffix.pop(0)
        N_mod_2_256=(N_mod_2_256*208+t)%MOD
report={
 "source_bytes":len(data),"source_sha256":EXPECTED_SHA256,"source_md5":EXPECTED_MD5,
 "distinct_source_characters":len(vals),"button_count":207,
 "separator_button":207,"separator_character_byte":sep_byte,
 "mapping_byte_to_button":{str(v):button[v] for v in vals},
 "token_count":n_tokens,"token_stream_sha256_u16be":h.hexdigest(),
 "base":208,"integer_definition":"N_0=0; N_(k+1)=208*N_k+t_(k+1); target=N_(2,000,000,000)",
 "prefix_tokens":prefix,"suffix_tokens":suffix,
 "target_mod_2^256":str(N_mod_2_256)
}
open("enwik9_button_number_report.json","w").write(json.dumps(report,indent=2)+"\n")
print(json.dumps(report,indent=2))

# trigger after workflow activation on default branch
