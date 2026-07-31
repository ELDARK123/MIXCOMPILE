#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import random
import runpy
import shutil
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HELPERS = runpy.run_path(str(Path(__file__).with_name("19_calibrate_ghidra.py")))
run_one = HELPERS["run_one"]
append_csv = HELPERS["append_csv"]


def load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


def write_csv(path: Path, rows: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", required=True, type=Path)
    parser.add_argument("--run-dir", required=True, type=Path)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    config = load(args.config.resolve())
    ghidra = load(ROOT / "config/ghidra_calibration.json")
    experiment = load(ROOT / "config/experiment_config.json")
    if config["formal_freeze_id"] != experiment["formal_freeze_id"]:
        raise RuntimeError("formal freeze mismatch")
    ghidra.update({key: config[key] for key in (
        "process_timeout_seconds", "function_decompile_timeout_seconds",
        "analysis_timeout_per_file_seconds", "max_cpu")})
    build_root = ROOT / config["build_root"]
    inputs = []
    for variant, directory in config["variants"].items():
        for sample in config["samples"]:
            binary = build_root / directory / sample["relative_elf"]
            if not binary.is_file():
                raise RuntimeError(f"missing binary: {binary}")
            inputs.append({"variant": variant, "benchmark": sample["name"],
                           "binary_path": str(binary), "sha256": sha256(binary)})
    schedule = []
    for repetition in range(1, config["analysis_repetitions"] + 1):
        batch = list(inputs)
        random.Random(config["order_seed"] + repetition).shuffle(batch)
        schedule += [{"kind": "analysis", "repetition": repetition, **item} for item in batch]
    schedule = [{"order": index, **item} for index, item in enumerate(schedule, 1)]
    plan = {"inputs": len(inputs), "analyses": len(schedule),
            "variants": list(config["variants"]), "samples": [s["name"] for s in config["samples"]],
            "repetitions": config["analysis_repetitions"],
            "process_timeout_seconds": config["process_timeout_seconds"]}
    print(json.dumps(plan, indent=2))
    if args.dry_run:
        return 0
    run_dir = args.run_dir.resolve()
    fresh = not run_dir.exists()
    if fresh:
        for child in ("logs", "results/raw/metrics", "results/tables", "config_snapshot", "tmp/projects"):
            (run_dir / child).mkdir(parents=True, exist_ok=True)
        for source, target in (
            (args.config.resolve(), run_dir / "config_snapshot/formal_testsuite_ghidra.json"),
            (ROOT / "config/ghidra_calibration.json", run_dir / "config_snapshot/ghidra_calibration.json"),
            (ROOT / "config/experiment_config.json", run_dir / "config_snapshot/experiment_config.json"),
            (Path(__file__), run_dir / "config_snapshot/35_run_formal_testsuite_ghidra.py"),
            (ROOT / ghidra["script"], run_dir / "config_snapshot/CollectDecompilationMetrics.java"),
        ):
            shutil.copy2(source, target)
        write_csv(run_dir / "results/raw/input_binary_manifest.csv", inputs)
        (run_dir / "raw_schedule.json").write_text(json.dumps(schedule, indent=2) + "\n")
        (run_dir / "run_manifest.json").write_text(json.dumps({
            "schema_version": 1, "run_id": run_dir.name, "stage": "formal_testsuite_ghidra",
            "status": "started", "planned_analyses": len(schedule), "network_used": False}, indent=2) + "\n")
    raw_path = run_dir / "results/raw/ghidra_runs.csv"
    rows = read_csv(raw_path) if raw_path.exists() else []
    completed = {(int(row["order"]), int(row["repetition"])) for row in rows}
    home, projects = run_dir / "tmp/ghidra_home", run_dir / "tmp/projects"
    script_path = ROOT / ghidra["script"]
    if fresh:
        warm = {"order": 0, "kind": "analysis", "repetition": 0, **inputs[0]}
        warm_row = run_one(run_dir, ghidra, warm, home, projects, script_path)
        if warm_row["parse_status"] != "ok":
            raise RuntimeError("Ghidra warmup failed")
    for item in schedule:
        if (item["order"], item["repetition"]) in completed:
            continue
        row = run_one(run_dir, ghidra, item, home, projects, script_path)
        append_csv(raw_path, row)
        rows.append({key: str(value) for key, value in row.items()})
    summaries = []
    for variant in config["variants"]:
        for sample in config["samples"]:
            group = [row for row in rows if row["variant"] == variant and row["benchmark"] == sample["name"]]
            parsed = [row for row in group if row["parse_status"] == "ok"]
            summaries.append({
                "variant": variant, "sample": sample["name"], "runs": len(group),
                "parsed_runs": len(parsed),
                "median_wall_seconds": statistics.median(float(r["wall_seconds"]) for r in parsed) if parsed else "",
                "median_decompilation_seconds": statistics.median(float(r["decompilation_elapsed_seconds"]) for r in parsed) if parsed else "",
                "median_successful_functions": statistics.median(int(r["successful_functions"]) for r in parsed) if parsed else "",
                "function_timeouts": sum(int(r["timed_out_functions"] or 0) for r in parsed),
                "function_failures": sum(int(r["failed_functions"] or 0) for r in parsed),
            })
    write_csv(run_dir / "results/tables/table_ghidra_by_sample.csv", summaries)
    lookup = {(row["variant"], row["sample"]): row for row in summaries}
    ratios = []
    for variant in ("gcc", "balanced", "optimized", "ollvm-full"):
        values = []
        for sample in config["samples"]:
            ref = lookup[("llvm", sample["name"])]["median_decompilation_seconds"]
            val = lookup[(variant, sample["name"])]["median_decompilation_seconds"]
            ratio = float(val) / float(ref) if ref != "" and val != "" and float(ref) else ""
            ratios.append({"variant": variant, "sample": sample["name"],
                           "reference_variant": "llvm", "decompilation_ratio_vs_llvm": ratio})
            if ratio != "": values.append(ratio)
        ratios.append({"variant": variant, "sample": "ALL_MEDIAN",
                       "reference_variant": "llvm",
                       "decompilation_ratio_vs_llvm": statistics.median(values) if values else ""})
    write_csv(run_dir / "results/tables/table_ghidra_ratios.csv", ratios)
    manifest = load(run_dir / "run_manifest.json")
    manifest.update({"status": "completed", "analyses": len(rows),
                     "parsed": sum(row["parse_status"] == "ok" for row in rows),
                     "process_timeouts": sum(row["parse_status"] == "timeout" for row in rows),
                     "metric_role": config["metric_role"], "claim_scope": config["claim_scope"]})
    (run_dir / "run_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    if (run_dir / "tmp").exists(): shutil.rmtree(run_dir / "tmp")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
