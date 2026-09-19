#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import lzma
import os
import struct
import tempfile
import zlib
from dataclasses import dataclass
from pathlib import Path

import numpy as np

NODES = 206
FRAME_SAMPLES = 512
SAMPLE_BITS = 16
AMP = 0.80

SIGNAL_MAGIC = b"EIS206M1"
SIGNAL_VERSION = 1
SIGNAL_HEADER_BYTES = 320

TC_MAGIC = b"TCE20621"
TC_VERSION = 1
TC_HEADER_BYTES = 416

BACKENDS = {"raw": 0, "tc_rle": 1, "zlib": 2, "lzma": 3}
BACKEND_NAMES = {v: k for k, v in BACKENDS.items()}


def sha256(data: bytes) -> bytes:
    return hashlib.sha256(data).digest()


def _varint_encode(n: int) -> bytes:
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


def _varint_decode(data: bytes, pos: int) -> tuple[int, int]:
    shift = 0
    value = 0
    while True:
        if pos >= len(data) or shift > 63:
            raise ValueError("bad varint")
        b = data[pos]
        pos += 1
        value |= (b & 0x7F) << shift
        if not (b & 0x80):
            return value, pos
        shift += 7


def tc_rle_encode(events: bytes) -> bytes:
    """Run-code only long no-change runs.

    Event values 0..206 are literals. 207 is reserved as ZERO_RUN marker.
    A run shorter than 3 stays literal so the transform never expands it.
    """
    out = bytearray()
    i = 0
    while i < len(events):
        if events[i] == 0:
            j = i + 1
            while j < len(events) and events[j] == 0:
                j += 1
            run = j - i
            if run >= 3:
                out.append(207)
                out.extend(_varint_encode(run))
            else:
                out.extend(b"\x00" * run)
            i = j
        else:
            out.append(events[i])
            i += 1
    return bytes(out)


def tc_rle_decode(data: bytes) -> bytes:
    out = bytearray()
    i = 0
    while i < len(data):
        b = data[i]
        i += 1
        if b == 207:
            run, i = _varint_decode(data, i)
            if run < 3:
                raise ValueError("noncanonical zero run")
            out.extend(b"\x00" * run)
        elif b <= 206:
            out.append(b)
        else:
            raise ValueError("reserved TC-RLE token")
    return bytes(out)


@dataclass(frozen=True)
class SignalHeader:
    source_length: int
    payload_bytes: int
    alphabet: bytes
    source_sha256: bytes
    payload_sha256: bytes


def build_signal_header(h: SignalHeader) -> bytes:
    if not (1 <= len(h.alphabet) <= NODES):
        raise ValueError("alphabet must contain 1..206 symbols")
    if len(h.source_sha256) != 32 or len(h.payload_sha256) != 32:
        raise ValueError("bad sha fields")
    b = bytearray(SIGNAL_HEADER_BYTES)
    struct.pack_into(
        "<8sHHHHHHQQ",
        b,
        0,
        SIGNAL_MAGIC,
        SIGNAL_VERSION,
        SIGNAL_HEADER_BYTES,
        NODES,
        FRAME_SAMPLES,
        SAMPLE_BITS,
        len(h.alphabet),
        h.source_length,
        h.payload_bytes,
    )
    b[36:68] = h.source_sha256
    b[68:100] = h.payload_sha256
    b[100:100 + len(h.alphabet)] = h.alphabet
    crc = zlib.crc32(b[:316]) & 0xFFFFFFFF
    struct.pack_into("<I", b, 316, crc)
    return bytes(b)


def parse_signal_header(raw: bytes) -> SignalHeader:
    if len(raw) != SIGNAL_HEADER_BYTES:
        raise ValueError("bad signal header size")
    magic, version, hbytes, nodes, samples, bits, alpha_len, source_len, payload_len = struct.unpack_from(
        "<8sHHHHHHQQ", raw, 0
    )
    if magic != SIGNAL_MAGIC or version != SIGNAL_VERSION or hbytes != SIGNAL_HEADER_BYTES:
        raise ValueError("unsupported EIS206 mixed signal")
    if nodes != NODES or samples != FRAME_SAMPLES or bits != SAMPLE_BITS:
        raise ValueError("unsupported signal geometry")
    if not (1 <= alpha_len <= NODES):
        raise ValueError("bad alphabet length")
    if (zlib.crc32(raw[:316]) & 0xFFFFFFFF) != struct.unpack_from("<I", raw, 316)[0]:
        raise ValueError("signal header CRC mismatch")
    return SignalHeader(
        source_length=source_len,
        payload_bytes=payload_len,
        alphabet=bytes(raw[100:100 + alpha_len]),
        source_sha256=bytes(raw[36:68]),
        payload_sha256=bytes(raw[68:100]),
    )


def source_to_ids(source: bytes, alphabet: bytes | None = None) -> tuple[bytes, np.ndarray]:
    if alphabet is None:
        alphabet = bytes(sorted(set(source)))
    if len(alphabet) > NODES:
        raise ValueError(f"source has {len(alphabet)} symbols; EIS206 limit is {NODES}")
    lut = np.full(256, -1, dtype=np.int16)
    for i, b in enumerate(alphabet):
        lut[b] = i
    ids = lut[np.frombuffer(source, dtype=np.uint8)]
    if np.any(ids < 0):
        raise ValueError("source symbol missing from alphabet")
    return alphabet, ids.astype(np.int16)


def ids_to_source(ids: np.ndarray, alphabet: bytes) -> bytes:
    a = np.frombuffer(alphabet, dtype=np.uint8)
    if np.any(ids < 0) or np.any(ids >= len(a)):
        raise ValueError("node id outside alphabet")
    return a[ids].tobytes()


def compose_payload(ids: np.ndarray, batch: int = 256) -> bytes:
    """All 206 nodes play in every frame.

    Each node owns a nonzero orthogonal carrier. The selected page is +AMP;
    every other node is -AMP. Thus the stored waveform is one mixed
    composition of all node states, not a bank of isolated lane slots.
    """
    ids = np.asarray(ids, dtype=np.int16).reshape(-1)
    lim = (1 << (SAMPLE_BITS - 1)) - 1
    chunks: list[bytes] = []
    for p in range(0, len(ids), batch):
        cur = ids[p:p + batch]
        H = np.zeros((len(cur), FRAME_SAMPLES // 2 + 1), dtype=np.complex128)
        H[:, 1:NODES + 1] = -AMP
        H[np.arange(len(cur)), cur + 1] = AMP
        x = np.fft.irfft(H, n=FRAME_SAMPLES, axis=1)
        if np.max(np.abs(x)) >= 1.0:
            raise ValueError("mixed composition clipped")
        q = np.rint(x * lim).astype("<i2")
        chunks.append(q.tobytes())
    return b"".join(chunks)


def listen_payload(payload: bytes, source_length: int, batch: int = 256) -> np.ndarray:
    q = np.frombuffer(payload, dtype="<i2")
    expected = source_length * FRAME_SAMPLES
    if q.size != expected:
        raise ValueError("mixed signal payload length mismatch")
    frames = q.reshape(source_length, FRAME_SAMPLES)
    ids = np.empty(source_length, dtype=np.int16)
    lim = (1 << (SAMPLE_BITS - 1)) - 1
    for p in range(0, source_length, batch):
        x = frames[p:p + batch].astype(np.float64) / lim
        R = np.fft.rfft(x, axis=1).real[:, 1:NODES + 1]
        chosen = np.argmax(R, axis=1)
        top = R[np.arange(len(chosen)), chosen]
        # Quantization should preserve a strict positive selected carrier.
        if np.any(top <= 0):
            raise ValueError("listener found no positive selected node")
        ids[p:p + len(chosen)] = chosen
    return ids


def write_signal(source: bytes, path: str | Path, alphabet: bytes | None = None) -> dict:
    alphabet, ids = source_to_ids(source, alphabet)
    payload = compose_payload(ids)
    h = SignalHeader(
        source_length=len(source),
        payload_bytes=len(payload),
        alphabet=alphabet,
        source_sha256=sha256(source),
        payload_sha256=sha256(payload),
    )
    raw = build_signal_header(h) + payload
    Path(path).write_bytes(raw)
    return {
        "source_bytes": len(source),
        "alphabet_nodes_used": len(alphabet),
        "physical_nodes_playing_per_frame": NODES,
        "signal_payload_bytes": len(payload),
        "signal_file_bytes": len(raw),
        "signal_ratio_vs_source": len(raw) / len(source),
    }


def read_signal(path: str | Path) -> tuple[SignalHeader, bytes]:
    raw = Path(path).read_bytes()
    if len(raw) < SIGNAL_HEADER_BYTES:
        raise ValueError("short signal")
    h = parse_signal_header(raw[:SIGNAL_HEADER_BYTES])
    payload = raw[SIGNAL_HEADER_BYTES:]
    if len(payload) != h.payload_bytes or sha256(payload) != h.payload_sha256:
        raise ValueError("signal payload integrity failure")
    return h, payload


def events_from_ids(ids: np.ndarray) -> bytes:
    """Encode full-field transitions relative to the previous finished field.

    0: the 206-node composition is unchanged.
    1..206: selected destination node + 1.

    Given the previous selected node, a nonzero event implies exactly two
    physical node flips: old +1 -> -1 and new -1 -> +1.
    """
    out = bytearray(len(ids))
    prev = -1
    for i, x in enumerate(np.asarray(ids, dtype=np.int16)):
        cur = int(x)
        if cur == prev:
            out[i] = 0
        else:
            out[i] = cur + 1
            prev = cur
    return bytes(out)


def ids_from_events(events: bytes) -> np.ndarray:
    out = np.empty(len(events), dtype=np.int16)
    prev = -1
    for i, e in enumerate(events):
        if e == 0:
            if prev < 0:
                raise ValueError("first event cannot be unchanged")
            cur = prev
        elif 1 <= e <= 206:
            cur = e - 1
        else:
            raise ValueError("bad field event")
        out[i] = cur
        prev = cur
    return out


def compress_events(events: bytes, backend: str) -> bytes:
    if backend == "raw":
        return events
    if backend == "tc_rle":
        return tc_rle_encode(events)
    if backend == "zlib":
        return zlib.compress(events, 9)
    if backend == "lzma":
        return lzma.compress(events, preset=9)
    raise ValueError("unknown backend")


def decompress_events(body: bytes, backend_id: int) -> bytes:
    backend = BACKEND_NAMES.get(backend_id)
    if backend == "raw":
        return body
    if backend == "tc_rle":
        return tc_rle_decode(body)
    if backend == "zlib":
        return zlib.decompress(body)
    if backend == "lzma":
        return lzma.decompress(body)
    raise ValueError("unknown backend")


def build_tc_header(backend: str, signal_sha: bytes, source_sha: bytes, signal_header: bytes, body_len: int) -> bytes:
    if len(signal_header) != SIGNAL_HEADER_BYTES:
        raise ValueError("bad embedded signal header")
    b = bytearray(TC_HEADER_BYTES)
    b[:8] = TC_MAGIC
    struct.pack_into("<HHB3xQQ", b, 8, TC_VERSION, TC_HEADER_BYTES, BACKENDS[backend],
                     parse_signal_header(signal_header).source_length, body_len)
    b[32:64] = signal_sha
    b[64:96] = source_sha
    b[96:416] = signal_header
    return bytes(b)


def parse_tc_header(raw: bytes):
    if len(raw) != TC_HEADER_BYTES or raw[:8] != TC_MAGIC:
        raise ValueError("bad TruCompute-EIS206 header")
    version, hbytes, backend_id, event_count, body_len = struct.unpack_from("<HHB3xQQ", raw, 8)
    if version != TC_VERSION or hbytes != TC_HEADER_BYTES:
        raise ValueError("unsupported TruCompute-EIS206 version")
    return backend_id, event_count, body_len, raw[32:64], raw[64:96], raw[96:416]


def pack_signal(signal_path: str | Path, artifact_path: str | Path, backend: str) -> dict:
    signal_raw = Path(signal_path).read_bytes()
    h, payload = read_signal(signal_path)
    ids = listen_payload(payload, h.source_length)
    source = ids_to_source(ids, h.alphabet)
    if sha256(source) != h.source_sha256:
        raise ValueError("listener source SHA mismatch")
    events = events_from_ids(ids)
    body = compress_events(events, backend)
    outer = build_tc_header(backend, sha256(signal_raw), h.source_sha256,
                            signal_raw[:SIGNAL_HEADER_BYTES], len(body))
    artifact = outer + body
    Path(artifact_path).write_bytes(artifact)
    changes = sum(1 for e in events if e)
    repeats = len(events) - changes
    return {
        "backend": backend,
        "source_bytes": len(source),
        "signal_bytes": len(signal_raw),
        "event_bytes_pre_backend": len(events),
        "changed_compositions": changes,
        "unchanged_compositions": repeats,
        "artifact_bytes": len(artifact),
        "artifact_vs_signal": len(artifact) / len(signal_raw),
        "artifact_vs_source": len(artifact) / len(source),
    }


def unpack_artifact(artifact_path: str | Path, signal_out: str | Path, source_out: str | Path) -> dict:
    raw = Path(artifact_path).read_bytes()
    if len(raw) < TC_HEADER_BYTES:
        raise ValueError("short artifact")
    backend_id, event_count, body_len, expected_signal_sha, expected_source_sha, signal_header = parse_tc_header(
        raw[:TC_HEADER_BYTES]
    )
    body = raw[TC_HEADER_BYTES:]
    if len(body) != body_len:
        raise ValueError("artifact body length mismatch")
    events = decompress_events(body, backend_id)
    if len(events) != event_count:
        raise ValueError("event count mismatch")
    ids = ids_from_events(events)
    sh = parse_signal_header(signal_header)
    source = ids_to_source(ids, sh.alphabet)
    if sha256(source) != expected_source_sha or sha256(source) != sh.source_sha256:
        raise ValueError("recovered source SHA mismatch")
    payload = compose_payload(ids)
    regenerated = signal_header + payload
    if sha256(payload) != sh.payload_sha256 or sha256(regenerated) != expected_signal_sha:
        raise ValueError("exact mixed signal regeneration failed")
    Path(source_out).write_bytes(source)
    Path(signal_out).write_bytes(regenerated)
    return {
        "backend": BACKEND_NAMES[backend_id],
        "source_bytes": len(source),
        "signal_bytes": len(regenerated),
        "exact_source": True,
        "exact_signal": True,
        "source_sha256": sha256(source).hex(),
        "signal_sha256": sha256(regenerated).hex(),
    }


def probe(source_path: str | Path, prefix: int) -> dict:
    with open(source_path, "rb") as f:
        source = f.read(prefix) if prefix > 0 else f.read()
    if not source:
        raise ValueError("empty source")
    with tempfile.TemporaryDirectory() as td_s:
        td = Path(td_s)
        signal = td / "source.eis206m"
        sig_meta = write_signal(source, signal)
        signal_raw = signal.read_bytes()
        h, payload = read_signal(signal)
        ids = listen_payload(payload, h.source_length)
        listened = ids_to_source(ids, h.alphabet)
        if listened != source:
            raise AssertionError("mixed signal listener failed")

        result = {
            **sig_meta,
            "source_sha256": sha256(source).hex(),
            "signal_sha256": sha256(signal_raw).hex(),
            "direct_zlib_bytes": len(zlib.compress(source, 9)),
            "direct_lzma_bytes": len(lzma.compress(source, preset=9)),
            "signal_zlib_bytes": len(zlib.compress(signal_raw, 9)),
            "backends": {},
        }
        for backend in BACKENDS:
            artifact = td / f"source.{backend}.tce206"
            regen_signal = td / f"regen.{backend}.eis206m"
            regen_source = td / f"regen.{backend}.bin"
            enc = pack_signal(signal, artifact, backend)
            dec = unpack_artifact(artifact, regen_signal, regen_source)
            if regen_source.read_bytes() != source:
                raise AssertionError("cold source replay failed")
            if regen_signal.read_bytes() != signal_raw:
                raise AssertionError("cold signal replay failed")
            result["backends"][backend] = {**enc, **dec}
        return result


def main():
    ap = argparse.ArgumentParser(description="TruCompute x EIS206 mixed-field codec v21")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("signal")
    p.add_argument("source")
    p.add_argument("output")

    p = sub.add_parser("pack")
    p.add_argument("signal")
    p.add_argument("artifact")
    p.add_argument("--backend", choices=BACKENDS, default="lzma")

    p = sub.add_parser("unpack")
    p.add_argument("artifact")
    p.add_argument("signal_output")
    p.add_argument("source_output")

    p = sub.add_parser("probe")
    p.add_argument("source")
    p.add_argument("--prefix", type=int, default=4096)

    a = ap.parse_args()
    if a.cmd == "signal":
        src = Path(a.source).read_bytes()
        print(json.dumps(write_signal(src, a.output), indent=2, sort_keys=True))
    elif a.cmd == "pack":
        print(json.dumps(pack_signal(a.signal, a.artifact, a.backend), indent=2, sort_keys=True))
    elif a.cmd == "unpack":
        print(json.dumps(unpack_artifact(a.artifact, a.signal_output, a.source_output), indent=2, sort_keys=True))
    else:
        print(json.dumps(probe(a.source, a.prefix), indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
