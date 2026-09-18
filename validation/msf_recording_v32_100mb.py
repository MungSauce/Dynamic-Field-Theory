#!/usr/bin/env python3
from __future__ import annotations
import hashlib,json,os,shutil,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"msf_recording_v32"))
from orchestra_encoder import compose
N=128

def sha256_file(path,chunk=8<<20):
    h=hashlib.sha256()
    with open(path,"rb") as f:
        for b in iter(lambda:f.read(chunk),b""):h.update(b)
    return h.hexdigest()

def main():
    src=Path(sys.argv[1] if len(sys.argv)>1 else "enwik100m")
    n=src.stat().st_size;ss=sha256_file(src);msf=Path("enwik100m_v32_N128.msf")
    t=time.perf_counter();cr=compose(src,msf,N);t1=time.perf_counter()
    src.unlink()
    enc=ROOT/"msf_recording_v32"/"orchestra_encoder.py"
    if enc.exists():enc.unlink()
    aud=Path("audience_v32")
    if aud.exists():shutil.rmtree(aud)
    aud.mkdir();shutil.copy2(msf,aud/"recording.msf")
    for name in ("audience_decoder.py","signal_field.py","signal_scrambler.py","signal_packing.py","msf_format.py"):
        shutil.copy2(ROOT/"msf_recording_v32"/name,aud/name)
    env=dict(os.environ);env["PYTHONPATH"]=""
    p=subprocess.run([sys.executable,"audience_decoder.py","recording.msf","recovered.txt"],cwd=aud,env=env,text=True,capture_output=True)
    if p.returncode:
        print(p.stdout);print(p.stderr,file=sys.stderr);raise SystemExit(p.returncode)
    dr=json.loads(p.stdout);rh=sha256_file(aud/"recovered.txt");rb=(aud/"recovered.txt").stat().st_size;t2=time.perf_counter()
    result={
      "architecture":"MSF recording v3.2 — DSSS/CDMA-like composite + PRN chip scrambling",
      "source":"canonical enwik8 (100,000,000 bytes)",
      "source_bytes":n,"source_sha256":ss,
      "instrument_count":N,"page_count":N,
      "shared_directional_note_lexicon":cr["shared_directional_note_lexicon"],
      "symbols_per_full_timestamp":cr["symbols_per_full_timestamp"],
      "frame_count":cr["frame_count"],
      "signal_sample_count":cr["signal_sample_count"],
      "signal_sample_alphabet":cr["signal_sample_alphabet"],
      "signal_payload_bytes":cr["signal_payload_bytes"],
      "signal_ratio":cr["signal_payload_bytes"]/n,
      "signal_compression_percent":100*(1-cr["signal_payload_bytes"]/n),
      "complete_msf_bytes":cr["complete_msf_bytes"],
      "complete_file_ratio":cr["complete_msf_bytes"]/n,
      "complete_file_compression_percent":100*(1-cr["complete_msf_bytes"]/n),
      "instrument_basis":cr["instrument_basis"],
      "postmix_recording_mask":cr["postmix_recording_mask"],
      "source_removed_before_decode":not src.exists(),
      "orchestra_encoder_removed_before_decode":not enc.exists(),
      "audience_workspace_files":sorted(x.name for x in aud.iterdir() if x.name!="recovered.txt"),
      "decoder_input":"recording.msf only",
      "audience_method":dr["audience_method"],
      "recovered_bytes":rb,"recovered_sha256":rh,
      "exact_reconstruction":rb==n and rh==ss and dr["exact_reconstruction"],
      "compose_seconds":t1-t,"listen_seconds":t2-t1,
      "status":"PASS" if rb==n and rh==ss and dr["exact_reconstruction"] else "FAIL"
    }
    Path("msf_recording_v32_100mb_report.json").write_text(json.dumps(result,indent=2)+"\n")
    print(json.dumps(result,indent=2))
    if result["status"]!="PASS":raise SystemExit(2)
if __name__=="__main__":main()
