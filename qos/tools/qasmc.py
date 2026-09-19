#!/usr/bin/env python3
import argparse, struct
from pathlib import Path

STATE={"-":0,"-+":1,"+-":2,"+":3}
MODE={"CASCADE":0,"DEEPEN":1,"CANCEL":2}

def parse_int(s): return int(s,0)
def u16(n): return struct.pack("<H",n)
def u32(n): return struct.pack("<I",n)

def compile_qasm(text:str)->bytes:
    out=bytearray()
    for lineno,raw in enumerate(text.splitlines(),1):
        line=raw.split("#",1)[0].strip()
        if not line or line.startswith("MATRIX "): continue
        x=line.split(); op=x[0].upper()
        try:
            if op=="NODE" and len(x)==3:
                out+=b"\x01"+u16(parse_int(x[1]))+bytes([STATE[x[2]]])
            elif op=="EDGE" and len(x)==4:
                out+=b"\x02"+u16(parse_int(x[1]))+u16(parse_int(x[2]))+bytes([MODE[x[3].upper()]])
            elif op=="STRIKE" and len(x)==3:
                out+=b"\x03"+u16(parse_int(x[1]))+bytes([(1 if x[2]=="+" else 255) if x[2] in ("+","-") else (_ for _ in ()).throw(ValueError("bad strike"))])
            elif op=="SEVER_NODE" and len(x)==2:
                out+=b"\x04"+u16(parse_int(x[1]))
            elif op=="SEVER_EDGE" and len(x)==3:
                out+=b"\x05"+u16(parse_int(x[1]))+u16(parse_int(x[2]))
            elif op=="SETTLE" and len(x) in (1,2):
                out+=b"\x06"+u32(parse_int(x[1]) if len(x)==2 else 100000)
            elif op=="EXPECT" and len(x)==3:
                out+=b"\x07"+u16(parse_int(x[1]))+bytes([STATE[x[2]]])
            elif op=="END" and len(x)==1:
                out+=b"\xff"
            else: raise ValueError("syntax")
        except (KeyError,ValueError) as e:
            raise SystemExit(f"line {lineno}: {raw}: {e}")
    if not out or out[-1]!=0xff: out+=b"\xff"
    return bytes(out)

def c_header(data,name="qos_boot_program"):
    body=",".join(f"0x{b:02x}" for b in data)
    return f"#ifndef QOS_BOOT_PROGRAM_H\n#define QOS_BOOT_PROGRAM_H\nstatic const unsigned char {name}[]={{ {body} }};\nstatic const unsigned int {name}_len={len(data)}u;\n#endif\n"

def main():
    ap=argparse.ArgumentParser(description="Q-ASM topology compiler")
    ap.add_argument("source"); ap.add_argument("output"); ap.add_argument("--c-header",action="store_true")
    a=ap.parse_args(); data=compile_qasm(Path(a.source).read_text())
    if a.c_header: Path(a.output).write_text(c_header(data))
    else: Path(a.output).write_bytes(data)
    print(f"QASM_COMPILE=PASS bytes={len(data)}")
if __name__=="__main__":main()
