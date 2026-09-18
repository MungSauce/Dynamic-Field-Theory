# Hutter-target compression program — 2026-09-17

## Engineering target

Canonical source:
- enwik9 bytes: 1,000,000,000
- SHA-256: 159b85351e5f76e60cbe32e04c677847a9ecba3adc79addab6f4c6c7aa3744bc
- MD5: e206c3450ac99950df65bf70ef61a12d

Primary engineering target:
- complete Hutter-style score < 99,312,424 bytes.

Why this target:
- the formally published prize page still lists the accepted 2024 record as 110,793,128 bytes;
- the 1% prize-improvement threshold over that accepted record is 109,685,197 bytes;
- current 2026 benchmark data reports zmix at 96,096,261 archive bytes plus 3,216,163 compressor bytes = 99,312,424 under Hutter-style compressor+archive accounting while committee testing is pending.

Therefore this branch aims below 99,312,424 so a result is not merely better than the stale accepted record.

## Accounting invariant

No decoder/model/tool byte may be hidden.

For every candidate:
score_estimate =
  source-dependent carrier
  + required custom decompression executable/source package
  + required model/dictionary/assets
  + any non-system recovery library that must ship.

Final Hutter eligibility still requires packaging and validation under the official resource/runtime rules.

## Track A — APC as a modeling transform, not post-whitening

Current verified engineering incumbent:
- sequenced_graph -> xz9e
- carrier: 211,836,640
- custom sequenced-graph decoder: 26,936
- earlier engineering complete: 211,863,576
- exact canonical replay: PASS

That result is not yet a Hutter-valid packaged score because the earlier harness did not charge the XZ recovery machinery.

APC must target the pre-entropy sequenced-graph carrier first.

Search dimensions:
- sequenced-graph geometry;
- APC page size;
- APC rule family/context order;
- raw vs APC carrier;
- entropy stage after APC;
- exact cold replay.

Acceptance:
APC is retained only if it reduces the complete exact chain.

## Track B — fixed-capacity ingest bot

Goal:
A causal compressor/predictor whose allocated adaptive state has a fixed maximum size independent of corpus length.

Allowed:
- mutate fixed parameter/state values during encoding/decoding;
- use already decoded history inside a fixed-size ring/window;
- fixed-size hash tables/context banks;
- fixed-size recurrent/mixer state;
- deterministic online learning reproduced by decoder.

Forbidden:
- growing a corpus-sized hidden dictionary/table;
- retaining source-dependent state without counting it;
- external files/network/secret side information during replay.

Important capacity statement:
A fixed N-bit persistent state cannot losslessly distinguish more than 2^N source states. Exact compression below N bits is possible only when the residual/model representation exploits source regularity. Runtime state that decoder deterministically regenerates from prior decoded bytes does not need to be shipped.

The useful architecture is therefore:

fixed generic bot + bounded regenerated adaptive state + compressed residual stream -> exact source.

## Stop/continue rule

Do not stop at an arbitrary target during research. Preserve every exact improvement.
For Hutter-oriented evaluation, flag:
- ACCEPTED_RECORD_BEAT when <110,793,128
- CURRENT_ACCEPTED_1PCT_BEAT when <109,685,197
- 2026_FRONTIER_BEAT when <99,312,424

No global-optimum claim.
