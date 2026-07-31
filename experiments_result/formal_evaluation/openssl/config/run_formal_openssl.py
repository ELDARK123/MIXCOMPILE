#!/usr/bin/env python3
from __future__ import annotations
import argparse,csv,hashlib,json,os,random,re,shutil,subprocess,time
from pathlib import Path
from statistics import median
ROOT=Path(__file__).resolve().parents[1]
def load(p): return json.loads(Path(p).read_text())
def sha(p):
 h=hashlib.sha256()
 with Path(p).open("rb") as f:
  for b in iter(lambda:f.read(1048576),b""): h.update(b)
 return h.hexdigest()
def run(cmd,log,timeout,env=None,cwd=None):
 t=time.perf_counter()
 Path(log).parent.mkdir(parents=True,exist_ok=True)
 try:
  with Path(log).open("w",encoding="utf-8") as f:
   r=subprocess.run(cmd,text=True,stdout=f,stderr=subprocess.STDOUT,timeout=timeout,env=env,cwd=cwd,check=False)
  rc=r.returncode
 except subprocess.TimeoutExpired: rc=None
 out=Path(log).read_text(errors="replace") if Path(log).stat().st_size<=16*1024*1024 else ""
 return out,time.perf_counter()-t,rc
def wc(p,rows,fields=None):
 Path(p).parent.mkdir(parents=True,exist_ok=True)
 with Path(p).open("w",newline="") as f:
  w=csv.DictWriter(f,fieldnames=fields or list(rows[0])); w.writeheader(); w.writerows(rows)
def flags(v,paths,cfg):
 if v=="gcc": return paths["GCC"],paths["GXX"],[]
 if v=="llvm": return paths["LLVM_CLANG"],paths["LLVM_CLANGXX"],[]
 if v=="ollvm-full": return paths["OLLVM_CLANG"],paths["OLLVM_CLANGXX"],["-mllvm","-sobf","-mllvm","-icall","-mllvm","-split","-mllvm","-fla","-mllvm","-sub","-mllvm","-bcf","-mllvm","-ibr","-mllvm","-igv"]
 w=ROOT/"config"/("optimized_pass_weights.json" if v=="optimized" else "baseline_pass_weights.json")
 return paths["MIXCOMPILE_CLANG"],paths["MIXCOMPILE_CLANGXX"],["-mllvm","-pd","-mllvm","-ibr","-mllvm","-bcf","-mllvm","-fla","-mllvm","-split","-mllvm","-igv","-mllvm","-sub","-mllvm","-icall","-mllvm","-mix-mode=cost","-mllvm",f"-mix-profile={v}","-mllvm",f"-mix-seed={cfg['seed']}","-mllvm",f"-mix-cost-config={w}"]
def parse(out):
 vals=[]
 for line in out.splitlines():
  if line.startswith("+F:"):
   for x in line.split(":")[3:]:
    try: vals.append(float(x))
    except ValueError: pass
 return vals[-1] if vals else None
def main():
 a=argparse.ArgumentParser(); a.add_argument("--config",required=True); a.add_argument("--run-dir",required=True); x=a.parse_args()
 c=load(x.config); p=load(ROOT/"config/paths.local.json"); e=load(ROOT/"config/experiment_config.json")
 rd=Path(x.run_dir).resolve(); br=Path(c["build_root"])
 if rd.exists() or br.exists() or e["formal_freeze_id"]!=c["formal_freeze_id"] or not e["formal_freeze_valid"]: raise SystemExit("path/freeze mismatch")
 fr=ROOT/"artifacts/formal_freeze"/c["formal_freeze_id"]
 fm=load(fr/"freeze_manifest.json")
 if sha(p["MIXCOMPILE_CLANG"])!=fm["formal_binaries"]["clang_sha256"]: raise SystemExit("compiler/freeze mismatch")
 cur=Path(p["MIXCOMPILE_SOURCE_ROOT"])/"llvm/lib/Passes/Obfuscation/PassDecider.cpp"
 frozen=fr/"source/llvm/lib/Passes/Obfuscation/PassDecider.cpp"
 if sha(cur)!=sha(frozen): raise SystemExit("PassDecider/freeze mismatch")
 ow=ROOT/"config/optimized_pass_weights.json"; bw=ROOT/"config/baseline_pass_weights.json"
 if "optimized" in c["variants"] and sha(ow)!=c["optimized_weights_sha256"]: raise SystemExit("optimized weights mismatch")
 if "balanced" in c["variants"] and sha(bw)!=c["baseline_weights_sha256"]: raise SystemExit("baseline weights mismatch")
 for d in ("logs","raw","results/tables","config_snapshot"): (rd/d).mkdir(parents=True,exist_ok=True)
 shutil.copy2(x.config,rd/"config_snapshot/formal_openssl.json"); shutil.copy2(__file__,rd/"config_snapshot/30_run_formal_openssl.py")
 shutil.copy2(ROOT/"config/paths.local.json",rd/"config_snapshot/paths.local.json")
 if "optimized" in c["variants"]: shutil.copy2(ow,rd/"config_snapshot/optimized_pass_weights.json")
 if "balanced" in c["variants"]: shutil.copy2(bw,rd/"config_snapshot/baseline_pass_weights.json")
 builds=[]; bins={}
 for v in c["variants"]:
  src=br/v; shutil.copytree(p["OPENSSL_SOURCE_ROOT"],src,symlinks=True)
  cc,cxx,be=flags(v,p,c); env=dict(os.environ,CC=cc,CXX=cxx,CFLAGS=" ".join(c["compile_flags"]+be),CXXFLAGS=" ".join(c["compile_flags"]+be),LDFLAGS=" ".join(c["link_flags"]))
  run(["make","distclean"],rd/f"logs/distclean_{v}.log",600,env,src)
  _,cs,cr=run(["perl","Configure",*c["configure_options"]],rd/f"logs/configure_{v}.log",600,env,src)
  bs=0; rr="NOT_RUN"
  if cr==0: _,bs,rr=run(["make",f"-j{c['build_jobs']}"],rd/f"logs/build_{v}.log",c["build_timeout_seconds"],env,src)
  b=src/"apps/openssl"; ok=cr==0 and rr==0 and b.is_file()
  smoke_out,smoke_seconds,smoke_rc=("",0.0,"NOT_RUN")
  if ok: smoke_out,smoke_seconds,smoke_rc=run([str(b),"version"],rd/f"logs/smoke_{v}.log",60,cwd=src)
  valid=ok and smoke_rc==0 and "OpenSSL 3.6.1" in smoke_out
  builds.append({"variant":v,"configure_exit":"TIMEOUT" if cr is None else cr,"configure_seconds":cs,"build_exit":"TIMEOUT" if rr is None else rr,"build_seconds":bs,"smoke_exit":"TIMEOUT" if smoke_rc is None else smoke_rc,"smoke_seconds":smoke_seconds,"status":"PASS" if valid else "FAIL","binary_sha256":sha(b) if ok else "","file_size":b.stat().st_size if ok else ""})
  if valid: bins[v]=b
 raw=[]
 for phase,n in (("warmup",c["warmups"]),("measure",c["repetitions"])):
  for rep in range(1,n+1):
   jobs=[(v,a,s) for v in c["variants"] if v in bins for a in c["algorithms"] for s in c["block_sizes"]]; random.Random(3000+(0 if phase=="warmup" else 1000)+rep).shuffle(jobs)
   for v,a,s in jobs:
    cmd=[str(bins[v]),"speed","-seconds",str(c["seconds_per_speed"]),"-bytes",str(s),"-mr","-evp",c["algorithms"][a]]
    out,sec,rc=run(cmd,rd/f"logs/speed_{v}_{a.replace('/','-')}_{s}_{phase}_{rep:02d}.log",c["speed_timeout_seconds"])
    val=parse(out); raw.append({"variant":v,"algorithm":a,"block_size":s,"phase":phase,"repetition":rep,"status":"OK" if rc==0 and val is not None else ("TIMEOUT" if rc is None else "FAIL"),"bytes_per_second":"" if val is None else val,"elapsed_seconds":sec})
 summary=[]
 for v in c["variants"]:
  for a in c["algorithms"]:
   for s in c["block_sizes"]:
    z=[float(r["bytes_per_second"]) for r in raw if r["variant"]==v and r["algorithm"]==a and int(r["block_size"])==s and r["phase"]=="measure" and r["status"]=="OK"]
    summary.append({"variant":v,"algorithm":a,"block_size":s,"ok_repetitions":len(z),"planned":c["repetitions"],"median_bytes_per_second":median(z) if z else ""})
 wc(rd/"results/tables/table_openssl_builds.csv",builds); wc(rd/"results/raw/openssl_speed_repetitions.csv",raw,["variant","algorithm","block_size","phase","repetition","status","bytes_per_second","elapsed_seconds"]); wc(rd/"results/tables/table_openssl_summary.csv",summary)
 shutil.rmtree(br)
 (rd/"run_manifest.json").write_text(json.dumps({"schema_version":1,"run_id":rd.name,"stage":"formal_openssl","status":"completed","build_passes":sum(r["status"]=="PASS" for r in builds),"build_variants":len(builds),"measurement_rows":sum(r["phase"]=="measure" and r["status"]=="OK" for r in raw),"formal_freeze_id":c["formal_freeze_id"],"temporary_build_root_removed":True,"network_used":False},indent=2)+"\n")
 return 0
if __name__=="__main__": raise SystemExit(main())
