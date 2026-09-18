#!/usr/bin/env python3
import argparse
from pathlib import Path
from codec import compose
p=argparse.ArgumentParser(description="Compose a source file into an MSF modifier-bank signal.")
p.add_argument("source"); p.add_argument("output",nargs="?"); p.add_argument("-n","--instruments",type=int,default=128)
a=p.parse_args()
out=Path(a.output) if a.output else Path(a.source).with_suffix(".msf")
r=compose(a.source,out,a.instruments)
print(out)
for k,v in r.items(): print(f"{k}: {v}")
