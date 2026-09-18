#!/usr/bin/env python3
import argparse
from pathlib import Path
from codec import listen
p=argparse.ArgumentParser(description="Recover the exact source from an MSF modifier-bank signal.")
p.add_argument("msf"); p.add_argument("output",nargs="?")
a=p.parse_args()
out=Path(a.output) if a.output else Path(a.msf).with_suffix(".recovered.txt")
r=listen(a.msf,out)
print(out)
for k,v in r.items(): print(f"{k}: {v}")
