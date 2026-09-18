#!/usr/bin/env python3
"""
Staggered multicarrier audio modem probe.

- 8 independent synthetic "instrument" lanes.
- 4 FSK states per lane = 2 bits/lane.
- 8 lanes = 16 payload bits per logical symbol index (2 source bytes).
- Each lane has its own phase-shifted symbol clock.
- Each lane is silent during its guard interval; guards therefore do not line up.
- MP3 is treated as the transmission/container channel.
- Decoder uses per-lane matched filtering and must reproduce the source exactly.

This is an engineering probe, not a compression claim. It measures the maximum
zero-error payload rate that survives MP3 for this signaling family.
"""
from __future__ import annotations
import argparse, hashlib, json, math, os, subprocess, wave, lzma, bz2, gzip
from array import array
from pathlib import Path

LANES = 8
STATES = 4
BITS_PER_STATE = 2
SYNC_SECONDS = 0.25
SYNC_GAP_SECONDS = 0.05
SYNC_F1 = 430.0
SYNC_F2 = 860.0

# Synthetic instrument centers. State offsets are added around each center.
CENTERS = [900.0, 2200.0, 3500.0, 4800.0, 6100.0, 7400.0, 8700.0, 10000.0]
STATE_OFFSETS = [-240.0, -80.0, 80.0, 240.0]

def sha256_bytes(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()

def ffmpeg_mp3(wav_path: str, mp3_path: str, bitrate_kbps: int) -> None:
    subprocess.run([
        "ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
        "-i", wav_path, "-ac", "1", "-b:a", f"{bitrate_kbps}k", mp3_path
    ], check=True)

def ffmpeg_pcm(mp3_path: str, pcm_path: str, sr: int) -> None:
    subprocess.run([
        "ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
        "-i", mp3_path, "-f", "s16le", "-acodec", "pcm_s16le",
        "-ac", "1", "-ar", str(sr), pcm_path
    ], check=True)

def read_pcm(path: str) -> array:
    a = array("h")
    a.frombytes(Path(path).read_bytes())
    return a

def put_tone(buf, start, count, sr, freq, amp):
    # Hann taper inside the active interval to reduce wideband transition energy.
    if count <= 1:
        return
    end = min(len(buf), start + count)
    for i, n in enumerate(range(start, end)):
        w = 0.5 - 0.5 * math.cos(2.0 * math.pi * (i + 0.5) / count)
        buf[n] += amp * w * math.sin(2.0 * math.pi * freq * i / sr)

def logical_groups(data: bytes):
    # 2-bit groups, MSB first. Pad the final logical frame with zero groups.
    groups = []
    for b in data:
        groups.extend([(b >> 6) & 3, (b >> 4) & 3, (b >> 2) & 3, b & 3])
    while len(groups) % LANES:
        groups.append(0)
    frames = len(groups) // LANES
    lane_syms = [[0] * frames for _ in range(LANES)]
    p = 0
    for k in range(frames):
        for lane in range(LANES):
            lane_syms[lane][k] = groups[p]
            p += 1
    return lane_syms, frames

def rebuild_bytes(lane_syms, byte_count):
    groups = []
    frames = len(lane_syms[0]) if lane_syms else 0
    for k in range(frames):
        for lane in range(LANES):
            groups.append(lane_syms[lane][k])
    out = bytearray()
    for i in range(0, len(groups), 4):
        if len(out) >= byte_count:
            break
        g = groups[i:i+4]
        if len(g) < 4:
            break
        out.append((g[0] << 6) | (g[1] << 4) | (g[2] << 2) | g[3])
    return bytes(out[:byte_count])

def generate(data: bytes, wav_path: str, sr: int, period_ms: float, guard_ms: float):
    period = max(8, round(sr * period_ms / 1000.0))
    guard = max(1, round(sr * guard_ms / 1000.0))
    active = period - guard
    if active < 8:
        raise ValueError("active interval too short")
    lane_syms, frames = logical_groups(data)

    sync1 = round(SYNC_SECONDS * sr)
    syncgap = round(SYNC_GAP_SECONDS * sr)
    sync2 = round(SYNC_SECONDS * sr)
    lead = sync1 + syncgap + sync2 + syncgap

    # Full-period phase offsets distribute each lane's guard/transition through time.
    phases = [round(lane * period / LANES) for lane in range(LANES)]
    total = lead + max(phases) + frames * period + period
    mix = [0.0] * total

    # Global preamble used only to anchor timing after MP3 decode.
    put_tone(mix, 0, sync1, sr, SYNC_F1, 0.45)
    put_tone(mix, sync1 + syncgap, sync2, sr, SYNC_F2, 0.45)

    lane_amp = 0.72 / LANES  # conservative sum headroom
    for lane in range(LANES):
        phi = phases[lane]
        center = CENTERS[lane]
        for k, st in enumerate(lane_syms[lane]):
            # Guard first, active tone second; lane phase makes guards non-coincident.
            start = lead + phi + k * period + guard
            freq = center + STATE_OFFSETS[st]
            put_tone(mix, start, active, sr, freq, lane_amp)

    pcm = array("h")
    for x in mix:
        x = max(-0.98, min(0.98, x))
        pcm.append(int(round(x * 32767)))

    with wave.open(wav_path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(pcm.tobytes())

    return {
        "period_samples": period,
        "guard_samples": guard,
        "active_samples": active,
        "phases": phases,
        "lead_samples": lead,
        "frames": frames,
        "payload_bits_per_logical_frame": LANES * BITS_PER_STATE,
        "payload_bps_nominal": (LANES * BITS_PER_STATE) * sr / period,
    }

def tone_energy(samples, start, count, sr, freq):
    # Matched-filter energy at a known frequency over one active lane interval.
    if start < 0 or start + count > len(samples) or count <= 0:
        return -1.0
    c = 0.0
    s = 0.0
    # Apply the same smooth window family used by the sender.
    for i in range(count):
        x = samples[start + i]
        w = 0.5 - 0.5 * math.cos(2.0 * math.pi * (i + 0.5) / count)
        ang = 2.0 * math.pi * freq * i / sr
        c += x * w * math.cos(ang)
        s += x * w * math.sin(ang)
    return c*c + s*s

def sync_score(samples, sr, offset):
    # Score the two known preamble tones; used to find MP3 encoder/decoder delay.
    n1 = round(SYNC_SECONDS * sr)
    gap = round(SYNC_GAP_SECONDS * sr)
    n2 = round(SYNC_SECONDS * sr)
    if offset < 0 or offset + n1 + gap + n2 > len(samples):
        return -1.0
    e1 = tone_energy(samples, offset, n1, sr, SYNC_F1)
    e2 = tone_energy(samples, offset + n1 + gap, n2, sr, SYNC_F2)
    # Penalize cross-tone leakage.
    x1 = tone_energy(samples, offset, n1, sr, SYNC_F2)
    x2 = tone_energy(samples, offset + n1 + gap, n2, sr, SYNC_F1)
    return (e1 + e2) - 0.5 * (x1 + x2)

def find_sync(samples, sr):
    # MP3 decoders may add/remove encoder delay. Search a bounded prefix.
    max_search = min(len(samples) - 1, round(0.08 * sr))
    step = max(1, round(sr / 4000))  # ~0.25 ms coarse grid
    best = (float("-inf"), 0)
    for off in range(0, max_search + 1, step):
        sc = sync_score(samples, sr, off)
        if sc > best[0]:
            best = (sc, off)
    # Local sample-resolution refinement.
    coarse = best[1]
    lo = max(0, coarse - 2 * step)
    hi = min(max_search, coarse + 2 * step)
    for off in range(lo, hi + 1):
        sc = sync_score(samples, sr, off)
        if sc > best[0]:
            best = (sc, off)
    return best[1], best[0]

def decode(samples, byte_count, sr, meta):
    sync_off, score = find_sync(samples, sr)
    lead = meta["lead_samples"]
    period = meta["period_samples"]
    guard = meta["guard_samples"]
    active = meta["active_samples"]
    phases = meta["phases"]
    frames = meta["frames"]

    out_lanes = [[0] * frames for _ in range(LANES)]
    margins = []
    for lane in range(LANES):
        center = CENTERS[lane]
        freqs = [center + o for o in STATE_OFFSETS]
        phi = phases[lane]
        for k in range(frames):
            start = sync_off + lead + phi + k * period + guard
            es = [tone_energy(samples, start, active, sr, f) for f in freqs]
            order = sorted(range(STATES), key=lambda i: es[i], reverse=True)
            best = order[0]
            out_lanes[lane][k] = best
            top = es[order[0]]
            second = es[order[1]]
            margins.append(0.0 if top <= 0 else (top - second) / top)

    recovered = rebuild_bytes(out_lanes, byte_count)
    return recovered, {
        "sync_offset_samples": sync_off,
        "sync_score": score,
        "min_margin": min(margins) if margins else 0.0,
        "mean_margin": sum(margins)/len(margins) if margins else 0.0,
    }

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("source")
    ap.add_argument("--prefix", type=int, default=4096)
    ap.add_argument("--sr", type=int, default=48000)
    ap.add_argument("--period-ms", type=float, required=True)
    ap.add_argument("--guard-ms", type=float, required=True)
    ap.add_argument("--bitrate", type=int, required=True)
    ap.add_argument("--out-prefix", default="stagger")
    args = ap.parse_args()

    data = Path(args.source).read_bytes()[:args.prefix]
    wav = args.out_prefix + ".wav"
    mp3 = args.out_prefix + ".mp3"
    pcm = args.out_prefix + ".pcm"

    meta = generate(data, wav, args.sr, args.period_ms, args.guard_ms)
    ffmpeg_mp3(wav, mp3, args.bitrate)
    ffmpeg_pcm(mp3, pcm, args.sr)
    samples = read_pcm(pcm)
    rec, diag = decode(samples, len(data), args.sr, meta)

    errors = sum(a != b for a, b in zip(data, rec)) + abs(len(data) - len(rec))
    exact = errors == 0 and len(rec) == len(data)
    duration_sec = len(samples) / args.sr
    payload_bps_effective = (len(data) * 8) / duration_sec if duration_sec else 0.0
    full_seconds_est = (1_000_000_000 * 8) / meta["payload_bps_nominal"]
    full_mp3_est = args.bitrate * 1000.0 / 8.0 * full_seconds_est

    mp3_blob = Path(mp3).read_bytes()
    xz_blob = lzma.compress(mp3_blob, format=lzma.FORMAT_XZ, preset=lzma.PRESET_EXTREME | 9)
    bz2_blob = bz2.compress(mp3_blob, compresslevel=9)
    gz_blob = gzip.compress(mp3_blob, compresslevel=9, mtime=0)
    # Verify the outer compression layers are exact representations of the MP3 bitstream.
    assert lzma.decompress(xz_blob) == mp3_blob
    assert bz2.decompress(bz2_blob) == mp3_blob
    assert gzip.decompress(gz_blob) == mp3_blob
    outer = {
        "mp3_raw_bytes": len(mp3_blob),
        "mp3_xz9e_bytes": len(xz_blob),
        "mp3_bz2_bytes": len(bz2_blob),
        "mp3_gzip9_bytes": len(gz_blob),
    }
    outer["best_lossless_outer"] = min(
        (("xz9e", len(xz_blob)), ("bz2", len(bz2_blob)), ("gzip9", len(gz_blob))),
        key=lambda x: x[1]
    )[0]
    outer["best_lossless_outer_bytes"] = min(len(xz_blob), len(bz2_blob), len(gz_blob))
    outer["best_outer_ratio_vs_mp3"] = outer["best_lossless_outer_bytes"] / len(mp3_blob) if mp3_blob else 1.0

    result = {
        "codec": "staggered_multicarrier_v1",
        "lanes": LANES,
        "states_per_lane": STATES,
        "bits_per_lane_symbol": BITS_PER_STATE,
        "source_prefix_bytes": len(data),
        "source_sha256": sha256_bytes(data),
        "sample_rate": args.sr,
        "period_ms": args.period_ms,
        "guard_ms": args.guard_ms,
        "bitrate_kbps": args.bitrate,
        "period_samples": meta["period_samples"],
        "guard_samples": meta["guard_samples"],
        "active_samples": meta["active_samples"],
        "phase_offsets_samples": meta["phases"],
        "nominal_payload_bps": meta["payload_bps_nominal"],
        "measured_audio_seconds": duration_sec,
        "effective_payload_bps_including_preamble": payload_bps_effective,
        "mp3_bytes": os.path.getsize(mp3),
        "outer_lossless_compression": outer,
        "errors": errors,
        "exact": exact,
        "recovered_sha256": sha256_bytes(rec) if len(rec) == len(data) else None,
        "decoder_diagnostics": diag,
        "full_enwik9_seconds_est_at_nominal_rate": full_seconds_est,
        "full_enwik9_mp3_bytes_est_at_same_bitrate": int(full_mp3_est),
        "full_enwik9_mp3_MB_est": full_mp3_est / 1e6,
        "note": "Full-size values are rate extrapolations only; a full exact run is required before treating them as results."
    }
    print(json.dumps(result, indent=2))
    return 0 if exact else 3

if __name__ == "__main__":
    raise SystemExit(main())
