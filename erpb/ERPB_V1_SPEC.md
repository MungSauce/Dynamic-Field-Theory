# ERPB-v1 — DFT-inspired Reversible Projector Benchmark

Purpose: compare multiple reversible structural projections on the same corpus without changing the source or hiding source-dependent state.

Canonical corpus:
- enwik9
- bytes: 1,000,000,000
- SHA-256: 159b85351e5f76e60cbe32e04c677847a9ecba3adc79addab6f4c6c7aa3744bc

Projector channels:
1. repetition — recursive grammar candidate
2. transition — first-order reversible byte delta
3. context — APC order-0/order-1 page model
4. periodic — reversible lag-delta and lag-XOR families
5. residual — raw source with the same outer entropy coder

Accounting:
complete_bits = 8 * (carrier_bytes + custom_decoder_bytes)
All source-derived parameters are stored in the carrier. Generic system compressors are reported separately as external controls.

Exactness gate:
A candidate is valid only when cold reconstruction produces exactly 1,000,000,000 bytes and the canonical SHA-256.

Selection:
- evaluate channels independently on the same source;
- rank only exact candidates by complete bits;
- within parameterized channel families, expand around improving candidates;
- stop a family after its tested neighbors fail to improve;
- never claim a global optimum from a tested plateau.

DFT inheritance:
The benchmark borrows only the representation idea of a common substrate observed through different projectors. No physical claim from DFT is assumed by the codec.
