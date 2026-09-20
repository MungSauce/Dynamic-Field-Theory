# Qompression Seed Profiler

Purpose: measure the portable information cost of the current Qompression grammar before the full TruQ substrate is implemented.

Current model:
- 206-primitive lossless fallback (205 direct byte values + ESC for the remaining 51).
- Space-delimited reusable buttons with trailing spaces preserved.
- Dictionary-definition cost counted explicitly.
- Document button-activation cost counted explicitly.
- Reports both:
  - uniform legal-branch seed cost (the current conservative grammar cost), and
  - an optimistic enumerative diagnostic conditioned on the observed token histogram.
- Converts seed information to ideal 19-state Q-cell count and sparse Node3 count.

Important: whole-field RAM/state expansion is not counted as compressed data. Only persistent information required after reset belongs in the seed accounting.

Canonical enwik9:
- size: 1,000,000,000 bytes
- MD5: e206c3450ac99950df65bf70ef61a12d
- SHA-1: 2996e86fb978f93cca8f566cc56998923e7fe581

Run:

```bash
python qompress/profiler/qseed_profiler.py /path/to/enwik9 --json qseed-report.json
pytest -q qompress/profiler/test_qseed_profiler.py
```

The final Qompression ratio is the serialized portable seed plus any persistent document-specific machinery required to reproduce the source. This profiler does not claim a final ratio until run against the exact source and the final grammar is frozen.
