#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import lzma
import struct
import tempfile
import zlib
from pathlib import Path

import numpy as np

from eis_k32.composer_encoder import compose
from eis_k32.eis_k32_format import (
    HEADER_BYTES,
    LANES,
    build_header,
    parse_header,
    read_file,
    write_file,
)
from eis_k32.listener_decoder import listen

MAGIC = b"TCEIS20\0"
OUTER_HEADER_BYTES = 184
BACKENDS = {"raw": 0, "zrle": 1, "zlib": 2, "lzma": 3}
BACKEND_NAMES = {v: k for k, v in BACKENDS.items()}


def sha256(data: bytes) -> bytes:
    return hashlib.sha256(data).digest()


def contextual_delta(source: bytes) -> bytes:
    """Fold each lane against its previous contextual zero.

    Each of the 32 EIS lanes has a previous byte value that acts as the local
    zero/context.  The emitted byte is the modulo-256 displacement from that
    old zero.  The transformed stream is lane-major so temporal history is
    contiguous for the outer coder.
    """
    pad = (-len(source)) % LANES
    arr = np.frombuffer(source + bytes(pad), dtype=np.uint8).reshape(-1, LANES)
    out = np.empty_like(arr)
    prev = np.zeros(LANES, dtype=np.uint8)
    for t, row in enumerate(arr):
        out[t] = (
            row.astype(np.int16) - prev.astype(np.int16)
        ) & 255
        prev = row.copy()
    return out.T.copy().reshape(-1).tobytes()


def contextual_inverse(delta: bytes, source_length: int) -> bytes:
    frames = (source_length + LANES - 1) // LANES
    expected = frames * LANES
    if len(delta) != expected:
        raise ValueError("contextual stream length mismatch")
    arr = np.frombuffer(delta, dtype=np.uint8).reshape(LANES, frames).T.copy()
    out = np.empty_like(arr)
    prev = np.zeros(LANES, dtype=np.uint8)
    for t, row in enumerate(arr):
        value = (
            prev.astype(np.uint16) + row.astype(np.uint16)
        ) & 255
        value = value.astype(np.uint8)
        out[t] = value
        prev = value
    return out.reshape(-1).tobytes()[:source_length]


def zrle_encode(data: bytes) -> bytes:
    """Tiny source-independent zero-run/literal codec.

    High-bit token: zero run of 1..128 bytes.
    Low-bit token: literal run of 1..128 bytes, followed by literals.
    """
    out = bytearray()
    i = 0
    n = len(data)
    while i < n:
        if data[i] == 0:
            j = i
            while j < n and data[j] == 0 and j - i < 128:
                j += 1
            out.append(0x80 | (j - i - 1))
            i = j
        else:
            j = i
            while j < n and data[j] != 0 and j - i < 128:
                j += 1
            out.append(j - i - 1)
            out.extend(data[i:j])
            i = j
    return bytes(out)


def zrle_decode(data: bytes) -> bytes:
    out = bytearray()
    i = 0
    while i < len(data):
        token = data[i]
        i += 1
        count = (token & 0x7F) + 1
        if token & 0x80:
            out.extend(bytes(count))
        else:
            if i + count > len(data):
                raise ValueError("truncated literal run")
            out.extend(data[i:i + count])
            i += count
    return bytes(out)


def compress_body(data: bytes, backend: str) -> bytes:
    if backend == "raw":
        return data
    if backend == "zrle":
        return zrle_encode(data)
    if backend == "zlib":
        return zlib.compress(data, 9)
    if backend == "lzma":
        return lzma.compress(data, preset=9)
    raise ValueError("unknown backend")


def decompress_body(data: bytes, backend_id: int) -> bytes:
    backend = BACKEND_NAMES.get(backend_id)
    if backend == "raw":
        return data
    if backend == "zrle":
        return zrle_decode(data)
    if backend == "zlib":
        return zlib.decompress(data)
    if backend == "lzma":
        return lzma.decompress(data)
    raise ValueError("unknown backend id")


def build_outer_header(backend: str, eis_sha: bytes, eis_header: bytes, body_len: int) -> bytes:
    if len(eis_sha) != 32 or len(eis_header) != HEADER_BYTES:
        raise ValueError("invalid header material")
    b = bytearray(OUTER_HEADER_BYTES)
    b[:8] = MAGIC
    b[8] = BACKENDS[backend]
    b[16:48] = eis_sha
    b[48:176] = eis_header
    struct.pack_into("<Q", b, 176, body_len)
    return bytes(b)


def parse_outer_header(raw: bytes):
    if len(raw) != OUTER_HEADER_BYTES or raw[:8] != MAGIC:
        raise ValueError("bad TruCompute-EIS header")
    backend_id = raw[8]
    eis_sha = raw[16:48]
    eis_header = raw[48:176]
    body_len = struct.unpack_from("<Q", raw, 176)[0]
    return backend_id, eis_sha, eis_header, body_len


def pack_eis(eis_path: str | Path, output_path: str | Path, backend: str) -> dict:
    eis_bytes = Path(eis_path).read_bytes()
    if len(eis_bytes) < HEADER_BYTES:
        raise ValueError("short EIS file")
    h, _payload = read_file(eis_path)
    if h.lanes != LANES or h.frame_samples != 96 or h.sample_bits != 16:
        raise ValueError("v20 probe requires canonical K32/96/16 geometry")
    source = listen(str(eis_path), drive=1.0)
    delta = contextual_delta(source)
    body = compress_body(delta, backend)
    outer = build_outer_header(backend, sha256(eis_bytes), eis_bytes[:HEADER_BYTES], len(body))
    artifact = outer + body
    Path(output_path).write_bytes(artifact)
    return {
        "backend": backend,
        "source_bytes": len(source),
        "eis_bytes": len(eis_bytes),
        "delta_bytes": len(delta),
        "artifact_bytes": len(artifact),
        "artifact_vs_eis": len(artifact) / len(eis_bytes),
        "artifact_vs_source": len(artifact) / len(source),
    }


def unpack_eis(artifact_path: str | Path, eis_output: str | Path, source_output: str | Path) -> dict:
    raw = Path(artifact_path).read_bytes()
    if len(raw) < OUTER_HEADER_BYTES:
        raise ValueError("short TruCompute-EIS artifact")
    backend_id, expected_eis_sha, original_header, body_len = parse_outer_header(
        raw[:OUTER_HEADER_BYTES]
    )
    body = raw[OUTER_HEADER_BYTES:]
    if len(body) != body_len:
        raise ValueError("artifact body length mismatch")
    h = parse_header(original_header)
    delta = decompress_body(body, backend_id)
    source = contextual_inverse(delta, h.source_length)
    if sha256(source) != h.source_sha256:
        raise ValueError("source hash mismatch after contextual inverse")

    regen_h, regen_payload, _meta = compose(
        source, frame_samples=h.frame_samples, sample_bits=h.sample_bits, drive=1.0
    )
    regenerated = build_header(regen_h) + regen_payload
    if sha256(regenerated) != expected_eis_sha:
        raise ValueError("exact EIS signal regeneration failed")

    Path(eis_output).write_bytes(regenerated)
    Path(source_output).write_bytes(source)
    return {
        "backend": BACKEND_NAMES[backend_id],
        "source_bytes": len(source),
        "eis_bytes": len(regenerated),
        "source_sha256": sha256(source).hex(),
        "eis_sha256": sha256(regenerated).hex(),
        "exact": True,
    }


def probe(source_path: str | Path) -> dict:
    source = Path(source_path).read_bytes()
    if not source:
        raise ValueError("empty source")
    with tempfile.TemporaryDirectory() as td:
        td = Path(td)
        eis_path = td / "source.eis"
        h, payload, meta = compose(source, frame_samples=96, sample_bits=16, drive=1.0)
        write_file(eis_path, h, payload)
        eis_bytes = eis_path.read_bytes()
        assert listen(str(eis_path), drive=1.0) == source

        results = {
            "source": str(source_path),
            "source_bytes": len(source),
            "eis_bytes": len(eis_bytes),
            "eis_ratio_vs_source": len(eis_bytes) / len(source),
            "source_zlib_bytes": len(zlib.compress(source, 9)),
            "source_lzma_bytes": len(lzma.compress(source, preset=9)),
            "eis_zlib_bytes": len(zlib.compress(eis_bytes, 9)),
            "prequant_peak": meta["prequant_peak"],
            "backends": {},
        }

        for backend in ("raw", "zrle", "zlib", "lzma"):
            artifact = td / f"source.{backend}.tceis"
            regen_eis = td / f"regen.{backend}.eis"
            regen_source = td / f"regen.{backend}.bin"
            enc = pack_eis(eis_path, artifact, backend)
            dec = unpack_eis(artifact, regen_eis, regen_source)
            if regen_source.read_bytes() != source:
                raise AssertionError("source cold replay failed")
            if regen_eis.read_bytes() != eis_bytes:
                raise AssertionError("signal cold replay failed")
            results["backends"][backend] = {**enc, "exact": dec["exact"]}

        return results


def main():
    ap = argparse.ArgumentParser(description="TruCompute-EIS contextual-zero signal probe v20")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("probe")
    p.add_argument("source")

    p = sub.add_parser("pack")
    p.add_argument("eis")
    p.add_argument("output")
    p.add_argument("--backend", choices=BACKENDS, default="zrle")

    p = sub.add_parser("unpack")
    p.add_argument("artifact")
    p.add_argument("eis_output")
    p.add_argument("source_output")

    a = ap.parse_args()
    if a.cmd == "probe":
        print(json.dumps(probe(a.source), indent=2, sort_keys=True))
    elif a.cmd == "pack":
        print(json.dumps(pack_eis(a.eis, a.output, a.backend), indent=2, sort_keys=True))
    elif a.cmd == "unpack":
        print(json.dumps(unpack_eis(a.artifact, a.eis_output, a.source_output), indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
