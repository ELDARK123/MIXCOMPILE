#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import shutil
import signal
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

COMPILER_IDS = {
    "gcc": "gcc",
    "llvm": "llvm",
    "balanced": "mixcompile",
    "optimized": "mixcompile-optimized",
    "ollvm-full": "ollvm-full",
}

BINARY_OVERRIDES = {
    "MultiSource/Benchmarks/Fhourstones-3.1": "MultiSource/Benchmarks/Fhourstones-3.1/fhourstones3.1",
    "MultiSource/Benchmarks/SciMark2-C": "MultiSource/Benchmarks/SciMark2-C/scimark2",
    "MultiSource/Applications/Burg": "MultiSource/Applications/Burg/burg",
}


def load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def write_csv(path: Path, rows: list[dict], fields: list[str] | None = None) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    selected = fields or list(rows[0])
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=selected)
        writer.writeheader()
        writer.writerows(rows)


def run(command: list[str], log: Path, timeout: int) -> tuple[float, int | None]:
    started = time.perf_counter()
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("w", encoding="utf-8") as stream:
        process = subprocess.Popen(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT,
                                   text=True, start_new_session=True)
        try:
            exit_code = process.wait(timeout=timeout)
            return time.perf_counter() - started, exit_code
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
            return time.perf_counter() - started, None


def binary_relative(sample: str) -> Path:
    source = Path(sample)
    key = source.as_posix()
    if key in BINARY_OVERRIDES:
        return Path(BINARY_OVERRIDES[key])
    if source.parts[:3] == ("SingleSource", "Benchmarks", "Shootout"):
        return source.parent / f"Shootout-{source.stem}"
    if source.suffix:
        return source.with_suffix("")
    return source / source.name


def text_size(size_tool: str, binary: Path) -> int:
    result = subprocess.run([size_tool, "--format=sysv", str(binary)], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
    if result.returncode != 0:
        raise RuntimeError(f"llvm-size failed: {binary}")
    for line in result.stdout.splitlines():
        fields = line.split()
        if fields and fields[0] == ".text" and len(fields) >= 2:
            return int(fields[1])
    raise RuntimeError(f"missing .text row: {binary}")


def compiler_path(variant: str, paths: dict) -> Path:
    return Path({
        "gcc": paths["GCC"],
        "llvm": paths["LLVM_CLANG"],
        "balanced": paths["MIXCOMPILE_CLANG"],
        "optimized": paths["MIXCOMPILE_CLANG"],
        "ollvm-full": paths["OLLVM_CLANG"],
    }[variant])


def validate(config: dict, paths: dict, split: dict) -> None:
    experiment = load(ROOT / "config/experiment_config.json")
    if experiment["formal_freeze_id"] != config["formal_freeze_id"] or not experiment["formal_freeze_valid"]:
        raise RuntimeError("formal freeze mismatch")
    if len(split["validation"]) != config["expected_samples"]:
        raise RuntimeError("validation sample count mismatch")
    if sha256(ROOT / "config/optimized_pass_weights.json") != config["optimized_weights_sha256"]:
        raise RuntimeError("optimized weight hash mismatch")
    if sha256(ROOT / "config/baseline_pass_weights.json") != config["baseline_weights_sha256"]:
        raise RuntimeError("baseline weight hash mismatch")
    for variant in config["variants"]:
        if sha256(compiler_path(variant, paths)) != config["compiler_sha256"][variant]:
            raise RuntimeError(f"compiler hash mismatch: {variant}")
    variants = load(ROOT / "config/compiler_variants.json")["variants"]
    expected_ollvm = [item for name in config["ollvm_full_strategies"] for item in ("-mllvm", f"-{name}")]
    if variants["ollvm-full"]["backend_flags"] != expected_ollvm:
        raise RuntimeError("OLLVM-full strategy list mismatch")
    for name, profile, weight_marker in (("balanced", "balanced", "{baseline_weights}"),
                                          ("optimized", "optimized", "{optimized_weights}")):
        flags = variants[COMPILER_IDS[name]]["backend_flags"]
        required = ["-pd", "-ibr", "-bcf", "-fla", "-split", "-igv", "-sub", "-icall",
                    f"-mix-profile={profile}", weight_marker]
        if any(not any(marker in item for item in flags) for marker in required):
            raise RuntimeError(f"incomplete MIXCOMPILE flags: {name}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", required=True, type=Path)
    parser.add_argument("--run-dir", required=True, type=Path)
    parser.add_argument("--build-root", required=True, type=Path)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    config = load(args.config.resolve())
    paths = load(ROOT / "config/paths.local.json")
    split = load(ROOT / config["validation_split"])
    validate(config, paths, split)
    run_dir = args.run_dir.resolve()
    build_root = args.build_root.resolve()
    plan = {
        "run_dir": str(run_dir), "build_root": str(build_root),
        "variants": config["variants"], "samples": len(split["validation"]),
        "stage": "formal_testsuite_build_correctness_static",
    }
    print(json.dumps(plan, indent=2))
    if args.dry_run:
        return 0
    if run_dir.exists() or build_root.exists():
        raise RuntimeError("run/build path already exists")

    for child in ("logs", "raw", "results/raw", "results/tables", "config_snapshot"):
        (run_dir / child).mkdir(parents=True, exist_ok=True)
    build_root.mkdir(parents=True)
    for source, target in (
        (args.config.resolve(), run_dir / "config_snapshot/formal_testsuite.json"),
        (ROOT / "config/experiment_config.json", run_dir / "config_snapshot/experiment_config.json"),
        (ROOT / "config/paths.local.json", run_dir / "config_snapshot/paths.local.json"),
        (ROOT / "config/compiler_variants.json", run_dir / "config_snapshot/compiler_variants.json"),
        (ROOT / "config/benchmark_split.json", run_dir / "config_snapshot/benchmark_split.json"),
        (ROOT / "config/baseline_pass_weights.json", run_dir / "config_snapshot/baseline_pass_weights.json"),
        (ROOT / "config/optimized_pass_weights.json", run_dir / "config_snapshot/optimized_pass_weights.json"),
        (Path(__file__), run_dir / "config_snapshot/31_run_formal_testsuite_build.py"),
        (ROOT / "scripts/03_build_test_suite.py", run_dir / "config_snapshot/03_build_test_suite.py"),
        (ROOT / "scripts/04_run_correctness.py", run_dir / "config_snapshot/04_run_correctness.py"),
    ):
        shutil.copy2(source, target)

    manifest = {
        "schema_version": 1, "run_id": run_dir.name, "stage": plan["stage"],
        "status": "started", "formal_freeze_id": config["formal_freeze_id"],
        "build_root": str(build_root), "started_at_utc": datetime.now(timezone.utc).isoformat(),
        "network_used": False,
    }
    (run_dir / "run_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    build_rows: list[dict] = []
    correctness_rows: list[dict] = []
    static_rows: list[dict] = []

    for variant in config["variants"]:
        compiler_id = COMPILER_IDS[variant]
        build_dir = build_root / variant
        command = [sys.executable, str(ROOT / "scripts/03_build_test_suite.py"),
                   "--compiler", compiler_id, "--subset", "validation",
                   "--build-dir", str(build_dir), "--run-dir", str(run_dir),
                   "--mix-seed", str(config["seed"]), "--execute"]
        seconds, rc = run(command, run_dir / f"logs/orchestrator_build_{variant}.log",
                          config["build_timeout_seconds"])
        build_rows.append({
            "variant": variant, "compiler_id": compiler_id,
            "compiler_path": str(compiler_path(variant, paths)),
            "compiler_sha256": sha256(compiler_path(variant, paths)),
            "elapsed_seconds": seconds, "exit_code": "TIMEOUT" if rc is None else rc,
            "status": "PASS" if rc == 0 else ("TIMEOUT" if rc is None else "FAIL"),
            "build_dir": str(build_dir), "command": " ".join(command),
        })
        write_csv(run_dir / "results/tables/table_testsuite_builds.csv", build_rows)
        if rc != 0:
            continue

        correctness_command = [sys.executable, str(ROOT / "scripts/04_run_correctness.py"),
                               "--build-dir", str(build_dir), "--variant", compiler_id,
                               "--subset", "validation", "--run-dir", str(run_dir),
                               "--allow-validation", "--execute"]
        correct_seconds, correct_rc = run(
            correctness_command, run_dir / f"logs/orchestrator_correctness_{variant}.log",
            config["correctness_timeout_seconds"])
        source_csv = run_dir / f"results/tables/table_correctness_{compiler_id}_validation.csv"
        if source_csv.is_file():
            with source_csv.open(encoding="utf-8", newline="") as stream:
                for row in csv.DictReader(stream):
                    row["variant"] = variant
                    row["correctness_batch_seconds"] = correct_seconds
                    row["correctness_batch_exit"] = "TIMEOUT" if correct_rc is None else correct_rc
                    correctness_rows.append(row)
            write_csv(run_dir / "results/tables/table_testsuite_correctness.csv", correctness_rows)

        passed = {row["benchmark"] for row in correctness_rows
                  if row["variant"] == variant and row["status"] == "PASS"}
        for sample in split["validation"]:
            relative = binary_relative(sample)
            benchmark = relative.as_posix()
            binary = build_dir / relative
            if benchmark not in passed or not binary.is_file():
                continue
            static_rows.append({
                "variant": variant, "benchmark": benchmark, "source_entry": sample,
                "binary_path": str(binary), "binary_sha256": sha256(binary),
                "file_size_bytes": binary.stat().st_size,
                "text_size_bytes": text_size(paths["LLVM_SIZE"], binary),
            })
        if static_rows:
            write_csv(run_dir / "results/raw/testsuite_static_per_sample.csv", static_rows)

    expected = len(config["variants"]) * config["expected_samples"]
    manifest.update({
        "status": "build_correctness_static_completed",
        "completed_at_utc": datetime.now(timezone.utc).isoformat(),
        "build_passes": sum(row["status"] == "PASS" for row in build_rows),
        "build_variants": len(config["variants"]),
        "correctness_passes": sum(row["status"] == "PASS" for row in correctness_rows),
        "correctness_expected": expected,
        "static_rows": len(static_rows),
        "build_root_retained_for_runtime_and_radiff": True,
    })
    (run_dir / "run_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return 0 if manifest["build_passes"] == len(config["variants"]) else 2


if __name__ == "__main__":
    raise SystemExit(main())
