#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, importlib.util, json, lzma, os, pathlib, subprocess, sys, wave

ROOT=pathlib.Path(__file__).resolve().parents[1]
QAM_PATH=ROOT/"audio_lab"/"orchestral_qam_v2.py"
spec=importlib.util.spec_from_file_location("eis_qam",QAM_PATH)
qam=importlib.util.module_from_spec(spec); spec.loader.exec_module(qam)

def sha_file(p):
    h=hashlib.sha256()
    with open(p,"rb") as f:
        for b in iter(lambda:f.read(8<<20),b""): h.update(b)
    return h.hexdigest()

def pcm_sha(path):
    with wave.open(path,"rb") as w:
        return hashlib.sha256(w.readframes(w.getnframes())).hexdigest()

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("source")
    ap.add_argument("hmc_carrier")
    ap.add_argument("hmc_decoder")
    ap.add_argument("--sr",type=int,default=96000)
    ap.add_argument("--block",type=int,default=64)
    ap.add_argument("--carriers",type=int,default=31)
    ap.add_argument("--bits-axis",type=int,default=21)
    ap.add_argument("--channels",type=int,default=1)
    ap.add_argument("--sample-bits",type=int,default=24,choices=[16,24])
    ap.add_argument("--drive",type=float,default=1.4)
    ap.add_argument("--out-prefix",default="hmc_eis")
    a=ap.parse_args()

    source=pathlib.Path(a.source)
    carrier=pathlib.Path(a.hmc_carrier)
    data=carrier.read_bytes()
    wav=a.out_prefix+".wav"
    flac=a.out_prefix+".flac"
    rt=a.out_prefix+".rt.wav"
    recovered_hmc=a.out_prefix+".recovered.hcd"
    recovered_source=a.out_prefix+".recovered.source"

    meta=qam.encode(data,wav,a.sr,a.block,a.carriers,a.bits_axis,a.channels,a.sample_bits,a.drive)
    recovered=qam.decode(wav,len(data),a.block,a.carriers,a.bits_axis,a.drive)
    pathlib.Path(recovered_hmc).write_bytes(recovered)
    eis_exact=(recovered==data)

    wb=pathlib.Path(wav).read_bytes()
    wav_xz=lzma.compress(wb,format=lzma.FORMAT_XZ,preset=lzma.PRESET_EXTREME|9)
    pathlib.Path(a.out_prefix+".wav.xz").write_bytes(wav_xz)
    carrier_xz=lzma.compress(data,format=lzma.FORMAT_XZ,preset=lzma.PRESET_EXTREME|9)
    pathlib.Path(a.out_prefix+".hcd.xz").write_bytes(carrier_xz)

    subprocess.run(["flac","-8","-f","-o",flac,wav],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    subprocess.run(["flac","-d","-f","-o",rt,flac],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)

    hmc_exact=False
    if eis_exact:
        subprocess.run([a.hmc_decoder,"d",recovered_hmc,recovered_source],check=True,
                       stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        hmc_exact=(source.stat().st_size==pathlib.Path(recovered_source).stat().st_size
                   and sha_file(source)==sha_file(recovered_source))

    hmc_decoder_bytes=os.path.getsize(a.hmc_decoder)
    eis_decoder_bytes=os.path.getsize(QAM_PATH)
    generic_decoder_bytes=hmc_decoder_bytes+eis_decoder_bytes

    result={
        "chain":"source -> HMC Domino -> EIS QAM -> HMC Domino -> source",
        "source_bytes":source.stat().st_size,
        "source_sha256":sha_file(source),
        "hmc_carrier_bytes":len(data),
        "hmc_carrier_sha256":hashlib.sha256(data).hexdigest(),
        "hmc_carrier_xz_bytes":len(carrier_xz),
        "eis_recovered_hmc_sha256":hashlib.sha256(recovered).hexdigest(),
        "eis_carrier_exact":eis_exact,
        "end_to_end_exact":hmc_exact,
        "hmc_decoder_bytes":hmc_decoder_bytes,
        "eis_decoder_script_bytes":eis_decoder_bytes,
        "combined_custom_decoder_bytes":generic_decoder_bytes,
        "sr":a.sr,"block_samples":a.block,"carriers":a.carriers,
        "bits_axis":a.bits_axis,"channels":a.channels,"sample_bits":a.sample_bits,
        **meta,
        "wav_bytes":os.path.getsize(wav),
        "flac_bytes":os.path.getsize(flac),
        "wav_xz_bytes":len(wav_xz),
        "wav_pcm_exact_after_flac":pcm_sha(wav)==pcm_sha(rt),
    }
    for k in ("hmc_carrier_bytes","hmc_carrier_xz_bytes","wav_bytes","flac_bytes","wav_xz_bytes"):
        result[k+"_ratio_vs_source"]=result[k]/result["source_bytes"]
    result["complete_hmc_bytes"]=result["hmc_carrier_bytes"]+hmc_decoder_bytes
    result["complete_eis_wav_bytes"]=result["wav_bytes"]+generic_decoder_bytes
    result["complete_eis_flac_bytes"]=result["flac_bytes"]+generic_decoder_bytes
    result["complete_eis_wav_xz_bytes"]=result["wav_xz_bytes"]+generic_decoder_bytes
    print(json.dumps(result,indent=2))
    raise SystemExit(0 if eis_exact and hmc_exact and result["wav_pcm_exact_after_flac"] else 3)

if __name__=="__main__":
    main()

# CANONICAL REPLICATION NOTICE (2026-09-18): predecessor/lineage artifact. Current protocol: compression/EIS_K32_FORMAL_REPLICATION_V1.md
