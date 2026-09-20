#!/usr/bin/env python3
"""Qompression seed-cost profiler.

This does *not* claim to implement the final Qompression codec. It computes the
portable-state/seed information implied by the currently frozen grammar:
  source bytes -> primitive definitions -> space-delimited reusable buttons ->
  document button activations.

The purpose is to answer the right pre-code question: how much independent
information must the terminal seed distinguish under the declared grammar?
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from collections import Counter
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import BinaryIO, Iterator

LOG2_19 = math.log2(19.0)


def iter_space_tokens(fp: BinaryIO, chunk_size: int = 8 << 20) -> Iterator[bytes]:
    """Yield maximal non-space runs with trailing ASCII space attached.

    This exactly preserves bytes. A final run without a trailing space is emitted
    as-is. Empty runs caused by consecutive spaces are represented as b" " so
    runs of spaces round-trip without hidden assumptions.
    """
    carry = bytearray()
    while True:
        chunk = fp.read(chunk_size)
        if not chunk:
            break
        start = 0
        for i, b in enumerate(chunk):
            if b == 0x20:
                carry.extend(chunk[start:i + 1])
                yield bytes(carry)
                carry.clear()
                start = i + 1
        carry.extend(chunk[start:])
    if carry:
        yield bytes(carry)


def primitive_event_count(data: bytes, direct_count: int = 205) -> int:
    """206-primitive lossless byte fallback from theorem 30.

    205 byte values are one primitive. Remaining 51 use ESC + a designated
    primitive, hence two primitive events. The particular byte assignment does
    not affect event count if direct values are 0..204.
    """
    if not (1 <= direct_count <= 255):
        raise ValueError("direct_count must be in [1,255]")
    return sum(1 if b < direct_count else 2 for b in data)


def log2_multinomial(counts: Counter[bytes]) -> float:
    """Ideal enumerative bits for an ordered token sequence given its histogram."""
    n = sum(counts.values())
    x = math.lgamma(n + 1.0)
    for c in counts.values():
        x -= math.lgamma(c + 1.0)
    return x / math.log(2.0)


def unsigned_varint_bits(n: int) -> int:
    if n < 0:
        raise ValueError(n)
    bytes_ = 1
    while n >= 0x80:
        n >>= 7
        bytes_ += 1
    return bytes_ * 8


@dataclass
class Report:
    source_path: str
    source_bytes: int
    sha256: str
    token_count: int
    unique_buttons: int
    unique_button_bytes: int
    primitive_definition_events: int
    raw_primitive_events: int
    primitive_alphabet: int
    independent_sign_choice: bool
    primitive_event_classes: int
    word_event_classes: int
    definition_uniform_bits: float
    document_uniform_bits: float
    seed_uniform_bits: float
    ratio_uniform: float
    reduction_uniform_percent: float
    document_enumerative_bits_given_histogram: float
    histogram_metadata_bits_varint: int
    dictionary_lengths_bits_varint: int
    optimistic_model_bits: float
    ratio_optimistic_model: float
    qcells_uniform: int
    node3_blocks_uniform: int
    notes: list[str]


def profile(path: Path, independent_sign_choice: bool = False,
            primitive_alphabet: int = 206) -> Report:
    source_bytes = path.stat().st_size
    sha = hashlib.sha256()
    counts: Counter[bytes] = Counter()
    first_seen: list[bytes] = []
    seen: set[bytes] = set()
    token_count = 0

    with path.open('rb') as fp:
        while True:
            c = fp.read(8 << 20)
            if not c:
                break
            sha.update(c)

    with path.open('rb') as fp:
        for tok in iter_space_tokens(fp):
            token_count += 1
            counts[tok] += 1
            if tok not in seen:
                seen.add(tok)
                first_seen.append(tok)

    unique_button_bytes = sum(len(t) for t in first_seen)
    primitive_defs = sum(primitive_event_count(t) for t in first_seen)

    raw_primitive_events = 0
    with path.open('rb') as fp:
        while True:
            c = fp.read(8 << 20)
            if not c:
                break
            raw_primitive_events += primitive_event_count(c)

    sign_factor = 2 if independent_sign_choice else 1
    primitive_classes = primitive_alphabet * sign_factor + 1
    word_classes = max(1, len(first_seen) * sign_factor + 1)

    definition_uniform_bits = primitive_defs * math.log2(primitive_classes)
    document_uniform_bits = token_count * math.log2(word_classes)
    seed_uniform_bits = definition_uniform_bits + document_uniform_bits
    source_bits = source_bytes * 8

    enum_bits = log2_multinomial(counts)
    hist_meta = sum(unsigned_varint_bits(c) for c in counts.values())
    len_meta = sum(unsigned_varint_bits(len(t)) for t in first_seen)
    optimistic_model_bits = definition_uniform_bits + enum_bits + hist_meta + len_meta

    qcells = math.ceil(seed_uniform_bits / LOG2_19) if seed_uniform_bits else 0
    node3_blocks = math.ceil(qcells / 16384) if qcells else 0

    return Report(
        source_path=str(path),
        source_bytes=source_bytes,
        sha256=sha.hexdigest(),
        token_count=token_count,
        unique_buttons=len(first_seen),
        unique_button_bytes=unique_button_bytes,
        primitive_definition_events=primitive_defs,
        raw_primitive_events=raw_primitive_events,
        primitive_alphabet=primitive_alphabet,
        independent_sign_choice=independent_sign_choice,
        primitive_event_classes=primitive_classes,
        word_event_classes=word_classes,
        definition_uniform_bits=definition_uniform_bits,
        document_uniform_bits=document_uniform_bits,
        seed_uniform_bits=seed_uniform_bits,
        ratio_uniform=(seed_uniform_bits / source_bits) if source_bits else 0.0,
        reduction_uniform_percent=(100.0 * (1.0 - seed_uniform_bits/source_bits)) if source_bits else 0.0,
        document_enumerative_bits_given_histogram=enum_bits,
        histogram_metadata_bits_varint=hist_meta,
        dictionary_lengths_bits_varint=len_meta,
        optimistic_model_bits=optimistic_model_bits,
        ratio_optimistic_model=(optimistic_model_bits/source_bits) if source_bits else 0.0,
        qcells_uniform=qcells,
        node3_blocks_uniform=node3_blocks,
        notes=[
            "Uniform seed cost is the current grammar cost if every reconstructed button is a legal branch at each placement.",
            "The optimistic model uses exact multinomial sequence rank given the histogram; it is diagnostic until that histogram/model is itself generated canonically from the seed law.",
            "Whole-field conceptual state size is not counted separately; only portable distinguishability/serialization is relevant.",
            "A source bit may influence the entire field but cannot contribute more independent information than the source contains without pre-existing machine structure.",
        ],
    )


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument('input', type=Path)
    ap.add_argument('--independent-sign-choice', action='store_true',
                    help='Count +/- as independent event alternatives. Default treats sign as determined by orientation/mode.')
    ap.add_argument('--json', type=Path, default=None)
    args = ap.parse_args()
    rep = profile(args.input, args.independent_sign_choice)
    payload = asdict(rep)
    s = json.dumps(payload, indent=2, sort_keys=True)
    print(s)
    if args.json:
        args.json.write_text(s + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
