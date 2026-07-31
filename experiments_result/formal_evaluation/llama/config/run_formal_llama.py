#!/usr/bin/env python3
"""Run formal llama.cpp build and llama-bench measurements for MIXCOMPILE."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import random
import shutil
import subprocess
import time
from pathlib import Path
from statistics import median


ROOT = Path(__file__).resolve().parents[1]


def load_json(path: Path):
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def write_json(path: Path, value) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def write_csv(path: Path, rows: list[dict], fields: list[str] | None = None) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fields = fields or (list(rows[0]) if rows else [])
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def run_logged(command: list[str], log_path: Path, timeout: int) -> tuple[str, float, int | None]:
    max_return_bytes = 16 * 1024 * 1024
    started = time.perf_counter()
    log_path.parent.mkdir(parents=True, exist_ok=True)
    try:
        with log_path.open("w", encoding="utf-8") as log_stream:
            result = subprocess.run(command, text=True, stdout=log_stream,
                                    stderr=subprocess.STDOUT, timeout=timeout,
                                    check=False)
        rc = result.returncode
    except subprocess.TimeoutExpired:
        rc = None
    elapsed = time.perf_counter() - started
    output = (log_path.read_text(encoding="utf-8", errors="replace")
              if log_path.stat().st_size <= max_return_bytes else "")
    return output, elapsed, rc


def variant_compiler_and_flags(variant: str, paths: dict, config: dict) -> tuple[str, str, list[str]]:
    if variant == "gcc":
        return paths["GCC"], paths["GXX"], []
    if variant == "llvm":
        return paths["LLVM_CLANG"], paths["LLVM_CLANGXX"], []
    if variant == "ollvm-full":
        return paths["OLLVM_CLANG"], paths["OLLVM_CLANGXX"], [
            "-mllvm", "-sobf", "-mllvm", "-icall", "-mllvm", "-split",
            "-mllvm", "-fla", "-mllvm", "-sub", "-mllvm", "-bcf",
            "-mllvm", "-ibr", "-mllvm", "-igv",
        ]
    weights = ROOT / "config" / ("optimized_pass_weights.json" if variant == "optimized" else "baseline_pass_weights.json")
    return paths["MIXCOMPILE_CLANG"], paths["MIXCOMPILE_CLANGXX"], [
        "-mllvm", "-pd", "-mllvm", "-ibr", "-mllvm", "-bcf", "-mllvm", "-fla",
        "-mllvm", "-split", "-mllvm", "-igv", "-mllvm", "-sub", "-mllvm", "-icall",
        "-mllvm", "-mix-mode=cost", "-mllvm", f"-mix-profile={variant}",
        "-mllvm", f"-mix-seed={config['seed']}", "-mllvm", f"-mix-cost-config={weights}",
    ]


def parse_llama_csv(text: str) -> list[dict[str, str]]:
    lines = [line for line in text.splitlines() if line.strip()]
    header_index = None
    for index, line in enumerate(lines):
        if "model_filename" in line and "avg_ts" in line:
            header_index = index
            break
    if header_index is None:
        return []
    return list(csv.DictReader(lines[header_index:]))


def numeric(row: dict[str, str], *names: str) -> float | None:
    for name in names:
        if name in row and row[name] != "":
            try:
                return float(row[name])
            except ValueError:
                pass
    return None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--run-dir", type=Path, required=True)
    args = parser.parse_args()
    config = load_json(args.config)
    paths = load_json(ROOT / "config/paths.local.json")
    experiment = load_json(ROOT / "config/experiment_config.json")
    run_dir = args.run_dir.resolve()
    build_root = Path(config["build_root"])
    if run_dir.exists() or build_root.exists():
        raise SystemExit("run directory or build root already exists")
    if experiment["formal_freeze_id"] != config["formal_freeze_id"] or not experiment["formal_freeze_valid"]:
        raise SystemExit("formal freeze mismatch")
    freeze_root = ROOT / "artifacts/formal_freeze" / config["formal_freeze_id"]
    freeze_manifest = load_json(freeze_root / "freeze_manifest.json")
    if sha256(Path(paths["MIXCOMPILE_CLANG"])) != freeze_manifest["formal_binaries"]["clang_sha256"]:
        raise SystemExit("current MIXCOMPILE compiler does not match formal freeze")
    current_passdecider = (Path(paths["MIXCOMPILE_SOURCE_ROOT"]) /
                           "llvm/lib/Passes/Obfuscation/PassDecider.cpp")
    frozen_passdecider = (freeze_root /
                          "source/llvm/lib/Passes/Obfuscation/PassDecider.cpp")
    if sha256(current_passdecider) != sha256(frozen_passdecider):
        raise SystemExit("current PassDecider.cpp does not match formal freeze")
    optimized_weights = ROOT / "config/optimized_pass_weights.json"
    if "optimized" in config["variants"] and sha256(optimized_weights) != config["optimized_weights_sha256"]:
        raise SystemExit("optimized pass weights do not match formal config")
    for child in ("logs", "raw", "results/tables", "config_snapshot"):
        (run_dir / child).mkdir(parents=True, exist_ok=True)
    shutil.copy2(args.config, run_dir / "config_snapshot" / args.config.name)
    shutil.copy2(ROOT / "config/paths.local.json", run_dir / "config_snapshot/paths.local.json")
    if "optimized" in config["variants"]:
        shutil.copy2(optimized_weights,
                     run_dir / "config_snapshot/optimized_pass_weights.json")
    shutil.copy2(Path(__file__), run_dir / "config_snapshot" / Path(__file__).name)

    model = Path(paths["LLAMA_MODEL_PATH"])
    if sha256(model) != load_json(ROOT / "config/llama_config.json")["model_sha256"]:
        raise RuntimeError("llama model sha256 mismatch")
    build_rows, binary_rows, raw_rows = [], [], []
    benches: dict[str, Path] = {}
    common = config["compile_flags"]
    link_flags = config["link_flags"]

    for variant in config["variants"]:
        cc, cxx, backend = variant_compiler_and_flags(variant, paths, config)
        build_dir = build_root / variant
        c_flags = " ".join(common + backend)
        cxx_flags = " ".join(common + backend)
        cmake = ["cmake", "-G", "Ninja", "-S", paths["LLAMA_CPP_SOURCE_ROOT"], "-B", str(build_dir),
                 "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_C_COMPILER={cc}", f"-DCMAKE_CXX_COMPILER={cxx}",
                 f"-DCMAKE_C_FLAGS_RELEASE={c_flags}", f"-DCMAKE_CXX_FLAGS_RELEASE={cxx_flags}",
                 f"-DCMAKE_EXE_LINKER_FLAGS={' '.join(link_flags)}", *config["cmake_options"]]
        build = ["cmake", "--build", str(build_dir), "--target", "llama-bench", "--", f"-j{config['build_jobs']}"]
        _, configure_seconds, configure_rc = run_logged(cmake, run_dir / f"logs/configure_{variant}.log", config["configure_timeout_seconds"])
        build_seconds, build_rc = 0.0, "NOT_RUN"
        if configure_rc == 0:
            _, build_seconds, build_rc = run_logged(build, run_dir / f"logs/build_{variant}.log", config["build_timeout_seconds"])
        build_rows.append({"variant": variant, "compiler": cc, "compiler_sha256": sha256(Path(cc)),
                           "configure_exit_code": "TIMEOUT" if configure_rc is None else configure_rc,
                           "configure_seconds": f"{configure_seconds:.6f}",
                           "build_exit_code": "TIMEOUT" if build_rc is None else build_rc,
                           "build_seconds": f"{build_seconds:.6f}",
                           "status": "PASS" if configure_rc == 0 and build_rc == 0 else "FAIL"})
        bench = build_dir / "bin/llama-bench"
        if not bench.is_file():
            binary_rows.append({"variant": variant, "status": "MISSING", "path": str(bench), "sha256": "", "file_size_bytes": ""})
            continue
        binary_rows.append({"variant": variant, "status": "OK", "path": str(bench), "sha256": sha256(bench),
                            "file_size_bytes": bench.stat().st_size})
        benches[variant] = bench

    for phase, blocks in (("warmup", config["warmups"]),
                          ("measure", config["repetitions"])):
        for rep in range(1, blocks + 1):
            schedule = [(variant, scenario) for variant in config["variants"]
                        if variant in benches for scenario in config["scenarios"]]
            random.Random(20260721 + (0 if phase == "warmup" else 1000) + rep).shuffle(schedule)
            for variant, scenario in schedule:
                bench = benches[variant]
                spec = config["scenarios"][scenario]
                command = [str(bench), "-m", str(model), "-ngl", str(config["gpu_layers"]), "-t", str(config["threads"]),
                           "-p", str(spec["n_prompt"]), "-n", str(spec["n_gen"]), "-r", "1", "--no-warmup", "-o", "csv"]
                output, elapsed, rc = run_logged(command, run_dir / f"logs/bench_{variant}_{scenario}_{phase}_{rep:02d}.log", config["bench_timeout_seconds"])
                parsed = parse_llama_csv(output)
                tokens_per_second = numeric(parsed[-1], "avg_ts", "avg_t/s", "avg_tps") if parsed else None
                avg_ns = numeric(parsed[-1], "avg_ns") if parsed else None
                raw_rows.append({"variant": variant, "scenario": scenario, "phase": phase, "repetition": rep,
                                 "exit_code": "TIMEOUT" if rc is None else rc, "elapsed_seconds": f"{elapsed:.6f}",
                                 "parse_status": "OK" if parsed and rc == 0 else ("TIMEOUT" if rc is None else "FAILED"),
                                 "tokens_per_second": "" if tokens_per_second is None else tokens_per_second,
                                 "avg_ns": "" if avg_ns is None else avg_ns,
                                 "log": f"logs/bench_{variant}_{scenario}_{phase}_{rep:02d}.log"})

    measure = [row for row in raw_rows if row["phase"] == "measure" and row["parse_status"] == "OK"]
    summary_rows = []
    for variant in config["variants"]:
        for scenario in config["scenarios"]:
            rows = [row for row in measure if row["variant"] == variant and row["scenario"] == scenario]
            values = [float(row["tokens_per_second"]) for row in rows if row["tokens_per_second"] != ""]
            summary_rows.append({"variant": variant, "scenario": scenario, "ok_repetitions": len(values),
                                 "planned_repetitions": config["repetitions"],
                                 "median_tokens_per_second": "" if not values else median(values),
                                 "min_tokens_per_second": "" if not values else min(values),
                                 "max_tokens_per_second": "" if not values else max(values)})
    write_csv(run_dir / "results/tables/table_llama_builds.csv", build_rows)
    write_csv(run_dir / "results/tables/table_llama_binaries.csv", binary_rows)
    write_csv(run_dir / "results/raw/llama_bench_repetitions.csv", raw_rows)
    write_csv(run_dir / "results/tables/table_llama_summary.csv", summary_rows)
    write_json(run_dir / "run_manifest.json", {"schema_version": 1, "run_id": run_dir.name, "stage": "formal_llama",
              "status": "completed", "formal_freeze_id": config["formal_freeze_id"], "build_variants": len(build_rows),
              "build_passes": sum(row["status"] == "PASS" for row in build_rows), "measurement_rows": len(measure),
              "planned_measurement_rows": len(config["variants"]) * len(config["scenarios"]) * config["repetitions"],
              "network_used": False, "model_sha256": sha256(model)})
    return 0 if all(row["status"] == "PASS" for row in build_rows) else 1


if __name__ == "__main__":
    raise SystemExit(main())
