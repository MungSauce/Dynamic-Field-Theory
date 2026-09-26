# MSF Modifier-Bank Page Orchestra

Status: experimental implementation for the current MSF revision.

Core invariants:
- one instrument owns one deterministic contiguous page;
- all instruments reuse one symbol lexicon;
- the directional lexicon is two roles over that shared symbol lexicon (forward/backward), up to 412 directional note identities for a 206-symbol alphabet;
- every instrument emits its forward and reverse page notes at the same logical timestamp;
- instrument identity is a source-independent procedural modifier ("same instrument lesson, different synth/timbre");
- all instrument states are mixed into one composite field before serialization;
- the Composer is disposable after the .msf exists;
- the Listener knows only the generic codec/modifier law plus what is contained in .msf;
- page boundaries are deterministic from source length and instrument count;
- no page table, per-position symbol stream, correction stream, or source copy is retained.

For N instruments, a full timestamp carries up to 2N source symbols. The longest page determines chronology, approximately L/(2N) timestamps.

The implementation intentionally measures the stored width of the composite field as N increases. Shorter chronology is not claimed as byte compression unless the complete .msf actually becomes smaller.

Run:
```
python composer.py input.txt output.msf --instruments 128
python listener.py output.msf recovered.txt
```
