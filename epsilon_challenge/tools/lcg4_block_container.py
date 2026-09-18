#!/usr/bin/env python3
import argparse, hashlib, os, struct, subprocess, tempfile

MAGIC = b"L4B1"
VERSION = 1

def put_var(f, v):
    while v >= 128:
        f.write(bytes([(v & 127) | 128]))
        v >>= 7
    f.write(bytes([v]))

def get_var(f):
    v = 0
    sh = 0
    while True:
        b = f.read(1)
        if not b:
            raise EOFError("short varint")
        c = b[0]
        v |= (c & 127) << sh
        if not (c & 128):
            return v
        sh += 7
        if sh > 63:
            raise ValueError("varint overflow")

def encode(codec, src, dst, block_size, max_rules):
    total = os.path.getsize(src)
    count = (total + block_size - 1) // block_size
    with open(src, "rb") as inp, open(dst, "wb") as out:
        out.write(MAGIC)
        out.write(bytes([VERSION]))
        out.write(struct.pack("<IQQ", block_size, total, count))
        for idx in range(count):
            data = inp.read(block_size)
            if not data:
                raise RuntimeError("unexpected EOF")
            with tempfile.TemporaryDirectory() as td:
                p_in = os.path.join(td, "b.in")
                p_car = os.path.join(td, "b.lcg4")
                open(p_in, "wb").write(data)
                cp = subprocess.run(
                    [codec, "e", p_in, p_car, str(max_rules)],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.PIPE,
                    text=True,
                )
                if cp.returncode:
                    raise RuntimeError(f"block {idx} encode failed: {cp.stderr[-2000:]}")
                carrier = open(p_car, "rb").read()
            put_var(out, len(carrier))
            out.write(carrier)
            if idx % 25 == 0 or idx + 1 == count:
                print(f"ENCODE_BLOCK={idx+1}/{count} SOURCE={len(data)} CARRIER={len(carrier)}", flush=True)

def decode(codec, src, dst):
    with open(src, "rb") as inp:
        if inp.read(4) != MAGIC:
            raise ValueError("bad block-container magic")
        ver = inp.read(1)
        if not ver or ver[0] != VERSION:
            raise ValueError("bad block-container version")
        block_size, total, count = struct.unpack("<IQQ", inp.read(20))
        written = 0
        with open(dst, "wb") as out:
            for idx in range(count):
                n = get_var(inp)
                carrier = inp.read(n)
                if len(carrier) != n:
                    raise EOFError("short block carrier")
                with tempfile.TemporaryDirectory() as td:
                    p_car = os.path.join(td, "b.lcg4")
                    p_out = os.path.join(td, "b.out")
                    open(p_car, "wb").write(carrier)
                    cp = subprocess.run(
                        [codec, "d", p_car, p_out],
                        stdout=subprocess.DEVNULL,
                        stderr=subprocess.PIPE,
                        text=True,
                    )
                    if cp.returncode:
                        raise RuntimeError(f"block {idx} decode failed: {cp.stderr[-2000:]}")
                    data = open(p_out, "rb").read()
                out.write(data)
                written += len(data)
                if idx % 25 == 0 or idx + 1 == count:
                    print(f"DECODE_BLOCK={idx+1}/{count} BYTES={len(data)}", flush=True)
        if written != total:
            raise RuntimeError(f"decoded size {written} != expected {total}")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=["e","d"])
    ap.add_argument("codec")
    ap.add_argument("src")
    ap.add_argument("dst")
    ap.add_argument("--block-size", type=int, default=2_000_000)
    ap.add_argument("--max-rules", type=int, default=256)
    a = ap.parse_args()
    if a.mode == "e":
        encode(a.codec, a.src, a.dst, a.block_size, a.max_rules)
    else:
        decode(a.codec, a.src, a.dst)

if __name__ == "__main__":
    main()
