#!/usr/bin/env python3
from __future__ import annotations
import hashlib,json,os,shutil,subprocess,sys,time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"msf_recording_v4"))
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
    msf=Path("enwik100m_v4_N128.msf")

    t0=time.perf_counter()
    enc=compose(src,msf,N)
    t1=time.perf_counter()

    # Hard orchestra -> recording -> audience boundary.
    src.unlink()
    orchestra=ROOT/"msf_recording_v4"/"orchestra_encoder.py"
    if orchestra.exists(): orchestra.unlink()

    audience=Path("audience_v4")
    if audience.exists():shutil.rmtree(audience)
    audience.mkdir()
    shutil.copy2(msf,audience/"recording.msf")
    for name in ("audience_decoder.py","signal_basis.py","msf_format.py"):
        shutil.copy2(ROOT/"msf_recording_v4"/name,audience/name)

    proc=subprocess.run(
        [sys.executable,"audience_decoder.py","recording.msf","recovered.txt"],
        cwd=audience,env={**os.environ,"PYTHONPATH":""},
        text=True,capture_output=True
    )
    if proc.returncode:
        print(proc.stdout)
        print(proc.stderr,file=sys.stderr)
        raise SystemExit(proc.returncode)
    dec=json.loads(proc.stdout)
    t2=time.perf_counter()

    recovered=audience/"recovered.txt"
    recovered_sha=sha256_file(recovered)
    recovered_bytes=recovered.stat().st_size

    result={
      "architecture":"MSF recording v4 — one composite signal sample per timestamp",
      "source":"first 100,000,000 bytes of canonical enwik9",
      "source_bytes":source_bytes,
      "source_sha256":source_sha,
      "instrument_count":N,
      "page_count":N,
      "alphabet_size":enc["alphabet_size"],
      "shared_directional_note_lexicon":enc["shared_directional_note_lexicon"],
      "symbols_per_full_timestamp":enc["symbols_per_full_timestamp"],
      "frame_count":enc["frame_count"],
      "recorded_samples":enc["recorded_samples"],
      "recorded_samples_per_timestamp":enc["recorded_samples_per_timestamp"],
      "sample_bytes":enc["sample_bytes"],
      "recorded_signal_payload_bytes":enc["payload_bytes"],
      "signal_ratio":enc["signal_ratio"],
      "signal_compression_percent":enc["signal_compression_percent"],
      "complete_msf_bytes":enc["complete_msf_bytes"],
      "complete_file_ratio":enc["complete_msf_bytes"]/source_bytes,
      "source_removed_before_decode":not src.exists(),
      "orchestra_encoder_removed_before_decode":not orchestra.exists(),
      "audience_workspace_files":sorted(p.name for p in audience.iterdir() if p.name!="recovered.txt"),
      "decoder_input":"recording.msf only",
      "recovered_bytes":recovered_bytes,
      "recovered_sha256":recovered_sha,
      "exact_reconstruction":bool(dec["exact_reconstruction"] and recovered_bytes==source_bytes and recovered_sha==source_sha),
      "compose_seconds":t1-t0,
      "listen_seconds":t2-t1,
      "status":"PASS" if dec["exact_reconstruction"] and recovered_bytes==source_bytes and recovered_sha==source_sha else "FAIL"
    }
    Path("msf_recording_v4_100mb_report.json").write_text(json.dumps(result,indent=2)+"\n")
    print(json.dumps(result,indent=2))
    if result["status"]!="PASS":raise SystemExit(2)

if __name__=="__main__":main()
