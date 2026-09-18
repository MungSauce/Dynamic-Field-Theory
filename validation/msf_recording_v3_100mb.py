#!/usr/bin/env python3
from __future__ import annotations
import hashlib, json, os, shutil, subprocess, sys, time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"msf_recording_v3"))
from orchestra_encoder import compose

N=128

def sha256_file(path,chunk=8<<20):
    h=hashlib.sha256()
    with open(path,"rb") as f:
        for b in iter(lambda:f.read(chunk),b""):h.update(b)
    return h.hexdigest()

def main():
    src=Path(sys.argv[1] if len(sys.argv)>1 else "enwik100m")
    source_bytes=src.stat().st_size
    source_sha=sha256_file(src)
    msf=Path("enwik100m_v3_N128.msf")

    t=time.perf_counter()
    compose_report=compose(src,msf,N)
    t1=time.perf_counter()

    # Cold boundary: source and orchestra implementation are unavailable to audience.
    src.unlink()
    encoder_path=ROOT/"msf_recording_v3"/"orchestra_encoder.py"
    if encoder_path.exists(): encoder_path.unlink()

    audience=Path("audience_workspace")
    if audience.exists(): shutil.rmtree(audience)
    audience.mkdir()
    shutil.copy2(msf,audience/"recording.msf")
    for name in ("audience_decoder.py","signal_field.py","msf_format.py"):
        shutil.copy2(ROOT/"msf_recording_v3"/name,audience/name)

    env=dict(os.environ)
    env["PYTHONPATH"]=""
    proc=subprocess.run(
        [sys.executable,"audience_decoder.py","recording.msf","recovered.txt"],
        cwd=audience,env=env,text=True,capture_output=True
    )
    if proc.returncode:
        print(proc.stdout);print(proc.stderr,file=sys.stderr);raise SystemExit(proc.returncode)
    decode_report=json.loads(proc.stdout)
    recovered_sha=sha256_file(audience/"recovered.txt")
    recovered_bytes=(audience/"recovered.txt").stat().st_size
    t2=time.perf_counter()

    result={
      "architecture":"MSF recording v3 — orchestra / recording / audience",
      "source":"first 100,000,000 bytes of canonical enwik9",
      "source_bytes":source_bytes,
      "source_sha256":source_sha,
      "instrument_count":N,
      "page_count":N,
      "shared_directional_note_lexicon":compose_report["shared_directional_note_lexicon"],
      "symbols_per_full_timestamp":compose_report["symbols_per_full_timestamp"],
      "frame_count":compose_report["frame_count"],
      "complex_samples":compose_report["complex_samples"],
      "payload_bytes":compose_report["payload_bytes"],
      "complete_msf_bytes":compose_report["complete_msf_bytes"],
      "bytes_per_timestamp":compose_report["bytes_per_timestamp"],
      "source_bytes_per_timestamp":compose_report["source_bytes_per_timestamp"],
      "signal_ratio":compose_report["payload_bytes"]/source_bytes,
      "signal_compression_percent":100*(1-compose_report["payload_bytes"]/source_bytes),
      "complete_file_ratio":compose_report["complete_msf_bytes"]/source_bytes,
      "complete_file_compression_percent":100*(1-compose_report["complete_msf_bytes"]/source_bytes),
      "source_removed_before_decode":not src.exists(),
      "orchestra_encoder_removed_before_decode":not encoder_path.exists(),
      "audience_workspace_files":sorted(p.name for p in audience.iterdir() if p.name!="recovered.txt"),
      "decoder_input":"recording.msf only",
      "recovered_bytes":recovered_bytes,
      "recovered_sha256":recovered_sha,
      "exact_reconstruction":recovered_bytes==source_bytes and recovered_sha==source_sha and decode_report["exact_reconstruction"],
      "compose_seconds":t1-t,
      "listen_seconds":t2-t1,
      "status":"PASS" if recovered_bytes==source_bytes and recovered_sha==source_sha else "FAIL"
    }
    Path("msf_recording_v3_100mb_report.json").write_text(json.dumps(result,indent=2)+"\n")
    print(json.dumps(result,indent=2))
    if result["status"]!="PASS":raise SystemExit(2)

if __name__=="__main__":main()
