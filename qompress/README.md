# Qompress v0.2 Reflex Reference

This branch adds the executable reflex/primed-zero layer without changing the proven Q-OS v0.1 baseline.

Run:

```bash
python qompress/tests/test_reflex_v2.py
```

Core invariant:

`blank primed-zero field + self-contained seed program -> exact bytes -> exact rollback to primed-zero`

The seed contains every corpus-specific byte required by the fixed decoder. Reversible chronology supplies causal position; it does not count as free arbitrary payload storage.
