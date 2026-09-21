from __future__ import annotations

import argparse
import json

from qompress.environment import decode_file, encode_file, QompressQNode


def _print(obj: object) -> None:
    print(json.dumps(obj, indent=2, sort_keys=True))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="qompress-hutter-reference",
        description=(
            "Pure-Python single 256x256 QNode reference: "
            "one byte equals one Q-turn"
        ),
    )
    sub = parser.add_subparsers(dest="cmd", required=True)

    enc = sub.add_parser("encode")
    enc.add_argument("source")
    enc.add_argument("seed")

    dec = sub.add_parser("decode")
    dec.add_argument("seed")
    dec.add_argument("output")
    dec.add_argument("--max-turns", type=int, default=None)

    aud = sub.add_parser("audit")
    aud.add_argument("seed")

    args = parser.parse_args(argv)
    if args.cmd == "encode":
        _print(encode_file(args.source, args.seed))
    elif args.cmd == "decode":
        _print(decode_file(args.seed, args.output, args.max_turns))
    else:
        _print(QompressQNode.from_file(args.seed).audit())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
