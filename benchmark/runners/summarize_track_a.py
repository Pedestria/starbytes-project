#!/usr/bin/env python3
import argparse
import datetime as dt
import glob
import json
import os
import platform
import statistics
import subprocess
from typing import Any


def _git_rev(root: str) -> str:
    try:
        out = subprocess.check_output(["git", "-C", root, "rev-parse", "HEAD"], text=True)
        return out.strip()
    except Exception:
        return "unknown"


def _load_json(path: str) -> Any:
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def build_summary(args: argparse.Namespace) -> dict[str, Any]:
    root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    contracts = _load_json(args.contracts)
    inputs = _load_json(args.inputs)

    pattern = os.path.join(args.raw_dir, f"{args.mode}_*.hyperfine.json")
    raw_files = sorted(glob.glob(pattern))

    workloads: list[dict[str, Any]] = []

    for raw_file in raw_files:
        workload = os.path.basename(raw_file)
        workload = workload.removeprefix(f"{args.mode}_").removesuffix(".hyperfine.json")

        payload = _load_json(raw_file)
        results = payload.get("results", [])

        entries: list[dict[str, Any]] = []
        python_mean = None

        for result in results:
            label = result.get("command", "unknown")
            normalized = label
            if normalized not in {"starbytes", "python", "go", "rust"}:
                lower = normalized.lower()
                if "starbytes" in lower:
                    normalized = "starbytes"
                elif "python" in lower:
                    normalized = "python"
                elif "go " in lower or lower.startswith("go"):
                    normalized = "go"
                elif "rustc" in lower or lower.endswith("_rust"):
                    normalized = "rust"
            entry = {
                "language": normalized,
                "mean_s": result.get("mean"),
                "stddev_s": result.get("stddev"),
                "median_s": result.get("median"),
                "min_s": result.get("min"),
                "max_s": result.get("max"),
                "runs": len(result.get("times", [])),
            }
            if normalized == "python":
                python_mean = result.get("mean")
            entries.append(entry)

        if python_mean:
            for entry in entries:
                mean = entry.get("mean_s")
                if mean is not None:
                    entry["runtime_rel_python"] = mean / python_mean

        workloads.append(
            {
                "workload": workload,
                "raw_file": os.path.relpath(raw_file, root),
                "results": entries,
            }
        )

    host = {
        "machine": platform.machine(),
        "platform": platform.platform(),
        "processor": platform.processor(),
        "python": platform.python_version(),
    }

    summary: dict[str, Any] = {
        "schema_version": "1.0.0",
        "generated_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "phase": contracts.get("phase"),
        "track": contracts.get("track"),
        "mode": args.mode,
        "commit": _git_rev(root),
        "contracts": os.path.relpath(args.contracts, root),
        "inputs": inputs,
        "host": host,
        "workloads": workloads,
    }

    rel_values: list[float] = []
    for workload in workloads:
        for result in workload["results"]:
            if result.get("language") == "starbytes" and result.get("runtime_rel_python"):
                rel_values.append(result["runtime_rel_python"])

    if rel_values:
        summary["starbytes_rel_python_geomean"] = statistics.geometric_mean(rel_values)

    return summary


def write_report(summary: dict[str, Any], report_path: str) -> None:
    lines: list[str] = []
    lines.append("# Track A Benchmark Report")
    lines.append("")
    lines.append(f"- phase: {summary.get('phase')}")
    lines.append(f"- track: {summary.get('track')}")
    lines.append(f"- mode: {summary.get('mode')}")
    lines.append(f"- commit: {summary.get('commit')}")
    lines.append(f"- generated_utc: {summary.get('generated_utc')}")
    lines.append("")

    for workload in summary.get("workloads", []):
        lines.append(f"## {workload['workload']}")
        lines.append("")
        lines.append("| language | mean(s) | stddev(s) | median(s) | rel_python | runs |")
        lines.append("|---|---:|---:|---:|---:|---:|")
        for result in workload.get("results", []):
            rel = result.get("runtime_rel_python")
            rel_text = f"{rel:.4f}" if rel is not None else "-"
            lines.append(
                "| {language} | {mean:.6f} | {stddev:.6f} | {median:.6f} | {rel} | {runs} |".format(
                    language=result.get("language", "-"),
                    mean=float(result.get("mean_s", 0.0) or 0.0),
                    stddev=float(result.get("stddev_s", 0.0) or 0.0),
                    median=float(result.get("median_s", 0.0) or 0.0),
                    rel=rel_text,
                    runs=result.get("runs", 0),
                )
            )
        lines.append("")

    geo = summary.get("starbytes_rel_python_geomean")
    if geo is not None:
        lines.append(f"Starbytes relative runtime geometric mean (python=1.0): {geo:.4f}")

    with open(report_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", required=True)
    parser.add_argument("--inputs", required=True)
    parser.add_argument("--contracts", required=True)
    parser.add_argument("--raw-dir", required=True)
    parser.add_argument("--summary", required=True)
    parser.add_argument("--report", required=True)
    args = parser.parse_args()

    summary = build_summary(args)

    os.makedirs(os.path.dirname(args.summary), exist_ok=True)
    with open(args.summary, "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2)

    os.makedirs(os.path.dirname(args.report), exist_ok=True)
    write_report(summary, args.report)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
