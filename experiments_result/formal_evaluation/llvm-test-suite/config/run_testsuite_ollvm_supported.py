#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import shutil
import subprocess
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FAILED_SAMPLE = "MultiSource/Applications/Burg"
BINARY_OVERRIDES = {
    "MultiSource/Benchmarks/Fhourstones-3.1": "MultiSource/Benchmarks/Fhourstones-3.1/fhourstones3.1",
    "MultiSource/Benchmarks/SciMark2-C": "MultiSource/Benchmarks/SciMark2-C/scimark2",
}


def load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


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


def test_relative(sample: str) -> Path:
    return binary_relative(sample).with_suffix(".test")


def write_csv(path: Path, rows: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def run(command: list[str], log: Path, timeout: int, cwd: Path | None = None) -> tuple[float, int | None]:
    started = time.perf_counter()
    with log.open("w", encoding="utf-8") as stream:
        try:
            result = subprocess.run(command, cwd=cwd, stdout=stream, stderr=subprocess.STDOUT,
                                    text=True, timeout=timeout, check=False)
            return time.perf_counter() - started, result.returncode
        except subprocess.TimeoutExpired:
            return time.perf_counter() - started, None


def parse_text_size(tool: str, binary: Path) -> int:
    result = subprocess.run([tool, "--format=sysv", str(binary)], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
    for line in result.stdout.splitlines():
        fields = line.split()
        if fields and fields[0] == ".text" and len(fields) >= 2:
            return int(fields[1])
    raise RuntimeError(f"llvm-size failed: {binary}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-dir", required=True, type=Path)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    run_dir = args.run_dir.resolve()
    manifest_path = run_dir / "run_manifest.json"
    manifest = load(manifest_path)
    config = load(run_dir / "config_snapshot/formal_testsuite.json")
    paths = load(ROOT / "config/paths.local.json")
    split = load(ROOT / "config/benchmark_split.json")
    original = load(run_dir / "raw/build_manifest_ollvm-full_validation.json")
    samples = [sample for sample in split["validation"] if sample != FAILED_SAMPLE]
    targets = [target for target in original["explicit_targets"] if target != "burg"]
    if len(samples) != 30 or len(targets) != 30:
        raise RuntimeError("expected exactly 30 OLLVM-supported samples")
    configure = list(original["configure_command"])
    if sum(item.startswith("-DCFLAGS=") for item in configure) != 1 or any(
            item.startswith("-DCXXFLAGS=") for item in configure):
        raise RuntimeError("backend flags are not injected exactly once")
    flags = next(item for item in configure if item.startswith("-DCFLAGS="))
    if any(flags.count(f"-{name}") != 1 for name in config["ollvm_full_strategies"]):
        raise RuntimeError("OLLVM strategy cardinality mismatch")
    build_root = Path(manifest["build_root"])
    build_dir = build_root / "ollvm-full-supported30"
    configure[configure.index("-B") + 1] = str(build_dir)
    build = ["cmake", "--build", str(build_dir), "--target", *targets, "--", f"-j{config['build_jobs']}"]
    plan = {"build_dir": str(build_dir), "samples": len(samples), "excluded_failure": FAILED_SAMPLE,
            "strategies": config["ollvm_full_strategies"], "configure": configure, "build": build}
    print(json.dumps(plan, indent=2))
    if args.dry_run:
        return 0
    if build_dir.exists():
        raise RuntimeError("supported30 build directory already exists")
    shutil.copy2(Path(__file__), run_dir / "config_snapshot/32_run_testsuite_ollvm_supported.py")
    configure_seconds, configure_rc = run(configure, run_dir / "logs/configure_ollvm-full_supported30.log", 600)
    build_seconds, build_rc = (0.0, "NOT_RUN")
    if configure_rc == 0:
        build_seconds, build_rc = run(build, run_dir / "logs/build_ollvm-full_supported30.log",
                                      config["build_timeout_seconds"])
    build_row = {
        "variant": "ollvm-full", "scope": "supported30", "excluded_sample": FAILED_SAMPLE,
        "configure_seconds": configure_seconds,
        "configure_exit": "TIMEOUT" if configure_rc is None else configure_rc,
        "build_seconds": build_seconds, "build_exit": "TIMEOUT" if build_rc is None else build_rc,
        "status": "PASS" if configure_rc == 0 and build_rc == 0 else "FAIL",
    }
    write_csv(run_dir / "results/tables/table_testsuite_ollvm_supported30_build.csv", [build_row])
    if build_rc != 0:
        return 2

    timeit = build_dir / "litsupport/modules/timeit.py"
    text = timeit.read_text(encoding="utf-8")
    text = text.replace('args += ["--limit-file-size", "209715200"]',
                        'args += ["--limit-file-size", "10485760"]')
    timeit.write_text(text, encoding="utf-8")
    result_json = run_dir / "raw/lit_results_ollvm-full_supported30.json"
    tests = [str(build_dir / test_relative(sample)) for sample in samples]
    lit_command = [paths["LLVM_LIT"], "-v", "-j1", "-o", str(result_json), *tests]
    correctness_seconds, correctness_rc = run(
        lit_command, run_dir / "logs/correctness_ollvm-full_supported30.log",
        config["correctness_timeout_seconds"])
    data = load(result_json) if result_json.is_file() else {"tests": []}
    correctness_rows = []
    for test in data["tests"]:
        correctness_rows.append({
            "variant": "ollvm-full", "scope": "supported30",
            "benchmark": test["name"].split("test-suite :: ", 1)[1].removesuffix(".test"),
            "status": test["code"], "elapsed_seconds": test.get("elapsed", ""),
            "batch_seconds": correctness_seconds,
            "batch_exit": "TIMEOUT" if correctness_rc is None else correctness_rc,
        })
    correctness_rows.append({
        "variant": "ollvm-full", "scope": "full31",
        "benchmark": "MultiSource/Applications/Burg/burg", "status": "BUILD_CRASH",
        "elapsed_seconds": "", "batch_seconds": "", "batch_exit": 139,
    })
    write_csv(run_dir / "results/tables/table_testsuite_ollvm_correctness.csv", correctness_rows)

    static_rows = []
    for sample in samples:
        relative = binary_relative(sample)
        binary = build_dir / relative
        if not binary.is_file():
            continue
        static_rows.append({
            "variant": "ollvm-full", "benchmark": relative.as_posix(), "source_entry": sample,
            "binary_path": str(binary), "binary_sha256": sha256(binary),
            "file_size_bytes": binary.stat().st_size,
            "text_size_bytes": parse_text_size(paths["LLVM_SIZE"], binary),
        })
    write_csv(run_dir / "results/raw/testsuite_ollvm_static_per_sample.csv", static_rows)
    pass_count = sum(row["status"] == "PASS" for row in correctness_rows)
    manifest.update({
        "status": "build_correctness_static_completed_with_ollvm_burg_crash",
        "ollvm_full_supported_build_passes": len(static_rows),
        "ollvm_full_correctness_passes": pass_count,
        "ollvm_full_build_crashes": 1,
        "ollvm_full_failed_sample": FAILED_SAMPLE,
        "ollvm_supported_completed_at_utc": datetime.now(timezone.utc).isoformat(),
    })
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return 0 if len(static_rows) == 30 and pass_count == 30 else 3


if __name__ == "__main__":
    raise SystemExit(main())
