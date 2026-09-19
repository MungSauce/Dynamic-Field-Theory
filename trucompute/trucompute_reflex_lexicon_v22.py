#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import lzma
import math
import re
import struct
import zlib
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path

MAGIC = b"TCRFLX22"
VERSION = 22
HEADER_BYTES = 128
MAX_ORDER = 2
TOKENIZER_ID = 1
BACKENDS = {"raw": 0, "zlib": 1, "lzma": 2}
BACKEND_NAMES = {v: k for k, v in BACKENDS.items()}


def sha256(data: bytes) -> bytes:
    return hashlib.sha256(data).digest()


def varint_encode(n: int) -> bytes:
    if n < 0:
        raise ValueError("negative varint")
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        if n:
            out.append(b | 0x80)
        else:
            out.append(b)
            return bytes(out)


def varint_decode(data: bytes, pos: int) -> tuple[int, int]:
    value = 0
    shift = 0
    while True:
        if pos >= len(data) or shift > 63:
            raise ValueError("invalid/truncated varint")
        b = data[pos]
        pos += 1
        value |= (b & 0x7F) << shift
        if not (b & 0x80):
            return value, pos
        shift += 7


def tokenize_exact(data: bytes) -> list[bytes]:
    return re.findall(rb"\s+|\S+", data)


def dictionary_for(tokens: list[bytes]) -> tuple[list[bytes], dict[bytes, int], bytes]:
    freq = Counter(tokens)
    words = sorted(freq, key=lambda w: (-freq[w], w))
    ids = {w: i for i, w in enumerate(words)}
    raw = bytearray(varint_encode(len(words)))
    for w in words:
        raw.extend(varint_encode(len(w)))
        raw.extend(w)
    return words, ids, bytes(raw)


def parse_dictionary(raw: bytes) -> list[bytes]:
    n, pos = varint_decode(raw, 0)
    words = []
    for _ in range(n):
        ln, pos = varint_decode(raw, pos)
        end = pos + ln
        if end > len(raw):
            raise ValueError("truncated dictionary token")
        words.append(raw[pos:end])
        pos = end
    if pos != len(raw):
        raise ValueError("trailing dictionary bytes")
    if len(set(words)) != len(words):
        raise ValueError("duplicate dictionary token")
    return words


@dataclass
class CandidateStat:
    count: int = 0
    last_seen: int = -1


def candidate_order(model: dict[int, CandidateStat]) -> list[int]:
    return sorted(model, key=lambda token_id: (
        -model[token_id].count,
        -model[token_id].last_seen,
        token_id,
    ))


def update_models(models, history: list[int], token_id: int, step: int) -> None:
    for order in range(0, min(MAX_ORDER, len(history)) + 1):
        ctx = tuple(history[-order:]) if order else ()
        s = models[order][ctx].get(token_id)
        if s is None:
            s = CandidateStat()
            models[order][ctx][token_id] = s
        s.count += 1
        s.last_seen = step


def encode_events(token_ids: list[int]) -> tuple[bytes, dict]:
    models = [defaultdict(dict) for _ in range(MAX_ORDER + 1)]
    history: list[int] = []
    raw = bytearray()
    level_counts = Counter()
    rank_counts = Counter()
    pairs = Counter()

    for step, target in enumerate(token_ids):
        selected_order = -1
        rank = target

        for order in range(min(MAX_ORDER, len(history)), -1, -1):
            ctx = tuple(history[-order:]) if order else ()
            model = models[order].get(ctx)
            if model and target in model:
                ordered = candidate_order(model)
                rank = ordered.index(target)
                selected_order = order
                break

        kind = 0 if selected_order < 0 else selected_order + 1
        packed = (rank << 2) | kind
        raw.extend(varint_encode(packed))
        level_counts[kind] += 1
        rank_counts[rank] += 1
        pairs[(kind, rank)] += 1

        update_models(models, history, target, step)
        history.append(target)

    n = len(token_ids)
    entropy = 0.0
    if n:
        for count in pairs.values():
            p = count / n
            entropy -= p * math.log2(p)

    metrics = {
        "event_raw_bytes": len(raw),
        "rank0_events": rank_counts[0],
        "rank0_fraction": rank_counts[0] / n if n else 0.0,
        "fallback_events": level_counts[0],
        "fallback_fraction": level_counts[0] / n if n else 0.0,
        "order0_events": level_counts[1],
        "order1_events": level_counts[2],
        "order2_events": level_counts[3],
        "empirical_event_entropy_bits_per_token": entropy,
        "empirical_event_entropy_total_bytes": entropy * n / 8.0,
    }
    return bytes(raw), metrics


def decode_events(raw: bytes, token_count: int, words: list[bytes]) -> list[int]:
    models = [defaultdict(dict) for _ in range(MAX_ORDER + 1)]
    history: list[int] = []
    out: list[int] = []
    pos = 0

    for step in range(token_count):
        packed, pos = varint_decode(raw, pos)
        kind = packed & 0x3
        rank = packed >> 2

        if kind == 0:
            if rank >= len(words):
                raise ValueError("global button rank outside dictionary")
            token_id = rank
        else:
            order = kind - 1
            if order > MAX_ORDER or order > len(history):
                raise ValueError("invalid contextual reflex order")
            ctx = tuple(history[-order:]) if order else ()
            model = models[order].get(ctx)
            if not model:
                raise ValueError("missing reflex field")
            ordered = candidate_order(model)
            if rank >= len(ordered):
                raise ValueError("TRUE delay exceeds reflex field")
            token_id = ordered[rank]

        out.append(token_id)
        update_models(models, history, token_id, step)
        history.append(token_id)

    if pos != len(raw):
        raise ValueError("trailing event bytes")
    return out


def compress(data: bytes, backend: str) -> bytes:
    if backend == "raw":
        return data
    if backend == "zlib":
        return zlib.compress(data, 9)
    if backend == "lzma":
        return lzma.compress(data, preset=9)
    raise ValueError("unknown backend")


def decompress(data: bytes, backend_id: int) -> bytes:
    backend = BACKEND_NAMES.get(backend_id)
    if backend == "raw":
        return data
    if backend == "zlib":
        return zlib.decompress(data)
    if backend == "lzma":
        return lzma.decompress(data)
    raise ValueError("unknown backend id")


def build_header(source_len: int, token_count: int, unique_count: int,
                 dict_raw_len: int, dict_body_len: int, event_raw_len: int,
                 event_body_len: int, backend: str, source_hash: bytes) -> bytes:
    b = bytearray(HEADER_BYTES)
    struct.pack_into(
        "<8sHHBBBBQQQQQQQ",
        b, 0,
        MAGIC, VERSION, HEADER_BYTES,
        TOKENIZER_ID, MAX_ORDER, BACKENDS[backend], 0,
        source_len, token_count, unique_count,
        dict_raw_len, dict_body_len, event_raw_len, event_body_len,
    )
    b[72:104] = source_hash
    crc = zlib.crc32(b[:124]) & 0xFFFFFFFF
    struct.pack_into("<I", b, 124, crc)
    return bytes(b)


def parse_header(b: bytes) -> dict:
    if len(b) != HEADER_BYTES:
        raise ValueError("bad header length")
    vals = struct.unpack_from("<8sHHBBBBQQQQQQQ", b, 0)
    magic, version, hbytes, tokenizer, max_order, backend, _reserved, source_len, token_count, unique_count, dict_raw_len, dict_body_len, event_raw_len, event_body_len = vals
    if magic != MAGIC or version != VERSION or hbytes != HEADER_BYTES:
        raise ValueError("unsupported reflex lexicon artifact")
    if tokenizer != TOKENIZER_ID or max_order != MAX_ORDER:
        raise ValueError("unsupported tokenizer/reflex order")
    if backend not in BACKEND_NAMES:
        raise ValueError("unsupported backend")
    if (zlib.crc32(b[:124]) & 0xFFFFFFFF) != struct.unpack_from("<I", b, 124)[0]:
        raise ValueError("header CRC mismatch")
    return {
        "backend_id": backend,
        "source_len": source_len,
        "token_count": token_count,
        "unique_count": unique_count,
        "dict_raw_len": dict_raw_len,
        "dict_body_len": dict_body_len,
        "event_raw_len": event_raw_len,
        "event_body_len": event_body_len,
        "source_sha256": b[72:104],
    }


def encode_bytes(source: bytes, backend: str = "lzma") -> tuple[bytes, dict]:
    tokens = tokenize_exact(source)
    words, ids, dict_raw = dictionary_for(tokens)
    token_ids = [ids[t] for t in tokens]
    event_raw, metrics = encode_events(token_ids)

    dict_body = lzma.compress(dict_raw, preset=9)
    event_body = compress(event_raw, backend)
    header = build_header(
        len(source), len(tokens), len(words), len(dict_raw), len(dict_body),
        len(event_raw), len(event_body), backend, sha256(source),
    )
    artifact = header + dict_body + event_body

    fixed_bits = math.ceil(math.log2(max(1, len(words)))) if len(words) > 1 else 0
    metrics.update({
        "source_bytes": len(source),
        "token_count": len(tokens),
        "unique_buttons": len(words),
        "dictionary_raw_bytes": len(dict_raw),
        "dictionary_lzma_bytes": len(dict_body),
        "event_backend": backend,
        "event_compressed_bytes": len(event_body),
        "artifact_bytes": len(artifact),
        "artifact_ratio_vs_source": len(artifact) / len(source) if source else 0.0,
        "fixed_global_id_bits_per_token": fixed_bits,
        "fixed_global_id_sequence_bytes": fixed_bits * len(tokens) / 8.0,
        "direct_zlib_bytes": len(zlib.compress(source, 9)),
        "direct_lzma_bytes": len(lzma.compress(source, preset=9)),
    })
    return artifact, metrics


def decode_bytes(artifact: bytes) -> bytes:
    if len(artifact) < HEADER_BYTES:
        raise ValueError("short artifact")
    h = parse_header(artifact[:HEADER_BYTES])
    d0 = HEADER_BYTES
    d1 = d0 + h["dict_body_len"]
    e1 = d1 + h["event_body_len"]
    if e1 != len(artifact):
        raise ValueError("artifact length mismatch")
    dict_raw = lzma.decompress(artifact[d0:d1])
    if len(dict_raw) != h["dict_raw_len"]:
        raise ValueError("dictionary length mismatch")
    words = parse_dictionary(dict_raw)
    if len(words) != h["unique_count"]:
        raise ValueError("dictionary count mismatch")
    event_raw = decompress(artifact[d1:e1], h["backend_id"])
    if len(event_raw) != h["event_raw_len"]:
        raise ValueError("event length mismatch")
    token_ids = decode_events(event_raw, h["token_count"], words)
    source = b"".join(words[i] for i in token_ids)
    if len(source) != h["source_len"] or sha256(source) != h["source_sha256"]:
        raise ValueError("source reconstruction mismatch")
    return source


def probe(path: str | Path, prefix: int = 0) -> dict:
    with open(path, "rb") as f:
        source = f.read(prefix) if prefix > 0 else f.read()
    result = {"source": str(path), "prefix": prefix, "codec_script_bytes": Path(__file__).stat().st_size, "backends": {}}
    for backend in BACKENDS:
        artifact, metrics = encode_bytes(source, backend)
        recovered = decode_bytes(artifact)
        if recovered != source:
            raise AssertionError("exact reflex-button replay failed")
        metrics["exact"] = True
        result["backends"][backend] = metrics
    return result


def main() -> None:
    ap = argparse.ArgumentParser(description="TruCompute reflex lexicon button codec v22")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("encode")
    p.add_argument("source")
    p.add_argument("artifact")
    p.add_argument("--backend", choices=BACKENDS, default="lzma")

    p = sub.add_parser("decode")
    p.add_argument("artifact")
    p.add_argument("output")

    p = sub.add_parser("probe")
    p.add_argument("source")
    p.add_argument("--prefix", type=int, default=0)

    a = ap.parse_args()
    if a.cmd == "encode":
        source = Path(a.source).read_bytes()
        artifact, metrics = encode_bytes(source, a.backend)
        Path(a.artifact).write_bytes(artifact)
        print(json.dumps(metrics, indent=2, sort_keys=True))
    elif a.cmd == "decode":
        source = decode_bytes(Path(a.artifact).read_bytes())
        Path(a.output).write_bytes(source)
        print(json.dumps({"decoded_bytes": len(source), "sha256": sha256(source).hex(), "exact_container_decode": True}, indent=2))
    else:
        print(json.dumps(probe(a.source, a.prefix), indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
