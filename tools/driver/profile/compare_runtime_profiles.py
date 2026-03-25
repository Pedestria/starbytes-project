#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path


def load_profile(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if data.get("type") != "runtime_profile":
        raise SystemExit(f"{path} is not a runtime profile JSON file")
    return data


def pct_delta(before: float, after: float) -> str:
    if before == 0:
        return "n/a"
    delta = ((after - before) / before) * 100.0
    return f"{delta:+.1f}%"


def indexed_counts(entries: list[dict], key: str = "name") -> dict[str, float]:
    out: dict[str, float] = {}
    for entry in entries:
        name = entry.get(key)
        if not isinstance(name, str):
            continue
        count = entry.get("count")
        total_ms = entry.get("total_ms")
        if isinstance(count, (int, float)):
            out[name] = float(count)
        elif isinstance(total_ms, (int, float)):
            out[name] = float(total_ms)
    return out


def function_totals(entries: list[dict]) -> dict[str, float]:
    out: dict[str, float] = {}
    for entry in entries:
        name = entry.get("name")
        total_ms = entry.get("total_ms")
        if isinstance(name, str) and isinstance(total_ms, (int, float)):
            out[name] = float(total_ms)
    return out


def object_counts(section: dict | None, field: str) -> dict[str, float]:
    if not isinstance(section, dict):
        return {}
    out: dict[str, float] = {}
    for entry in section.get(field, []):
        kind = entry.get("kind")
        count = entry.get("count")
        if isinstance(kind, str) and isinstance(count, (int, float)):
            out[kind] = float(count)
    return out


def top_deltas(before: dict[str, float], after: dict[str, float], limit: int) -> list[tuple[str, float, float, float]]:
    names = set(before) | set(after)
    rows: list[tuple[str, float, float, float]] = []
    for name in names:
        lhs = before.get(name, 0.0)
        rhs = after.get(name, 0.0)
        rows.append((name, lhs, rhs, rhs - lhs))
    rows.sort(key=lambda row: (abs(row[3]), abs(row[2]), row[0]), reverse=True)
    return rows[:limit]


def print_section(title: str, rows: list[tuple[str, float, float, float]], unit: str) -> None:
    print(title)
    if not rows:
        print("  (none)")
        return
    for name, lhs, rhs, delta in rows:
        print(f"  {name}: {lhs:.3f}{unit} -> {rhs:.3f}{unit} ({delta:+.3f}{unit})")


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare two Starbytes runtime profile JSON files.")
    parser.add_argument("baseline", help="Path to the baseline runtime profile JSON")
    parser.add_argument("candidate", help="Path to the candidate runtime profile JSON")
    parser.add_argument("--top", type=int, default=8, help="Number of rows to print per section")
    args = parser.parse_args()

    baseline_path = Path(args.baseline)
    candidate_path = Path(args.candidate)
    baseline = load_profile(baseline_path)
    candidate = load_profile(candidate_path)

    baseline_total = float(baseline.get("timings_ms", {}).get("total_runtime", 0.0))
    candidate_total = float(candidate.get("timings_ms", {}).get("total_runtime", 0.0))

    print("Runtime Profile Comparison")
    print(f"baseline: {baseline_path}")
    print(f"candidate: {candidate_path}")
    print(f"total runtime: {baseline_total:.3f} ms -> {candidate_total:.3f} ms ({pct_delta(baseline_total, candidate_total)})")

    overlap = candidate.get("overlap_rules") or baseline.get("overlap_rules") or {}
    if overlap.get("subsystems_are_inclusive", True):
        print("note: subsystem timings are overlapping/inclusive; compare rows individually, do not sum them.")
    if overlap.get("function_totals_are_inclusive", True):
        print("note: function totals are inclusive; parent and child time overlaps by design.")
    print("")

    baseline_counts = baseline.get("counts", {})
    candidate_counts = candidate.get("counts", {})
    print("Top-level counts")
    for key in [
        "dispatch",
        "function_calls",
        "refcount_increments",
        "refcount_decrements",
        "runtime_quickened_sites",
        "runtime_quickened_executions",
        "runtime_quickened_specializations",
        "runtime_quickened_fallbacks",
    ]:
        lhs = float(baseline_counts.get(key, 0.0))
        rhs = float(candidate_counts.get(key, 0.0))
        print(f"  {key}: {lhs:.0f} -> {rhs:.0f} ({pct_delta(lhs, rhs)})")
    print("")

    print_section(
        "Top subsystem deltas (ms)",
        top_deltas(indexed_counts(baseline.get("subsystems", [])), indexed_counts(candidate.get("subsystems", [])), args.top),
        " ms",
    )
    print("")

    print_section(
        "Top opcode count deltas",
        top_deltas(indexed_counts(baseline.get("opcodes", [])), indexed_counts(candidate.get("opcodes", [])), args.top),
        "",
    )
    print("")

    print_section(
        "Top function total-time deltas (ms)",
        top_deltas(function_totals(baseline.get("functions", [])), function_totals(candidate.get("functions", [])), args.top),
        " ms",
    )
    print("")

    baseline_objects = baseline.get("objects")
    candidate_objects = candidate.get("objects")
    print_section(
        "Top object allocation deltas",
        top_deltas(object_counts(baseline_objects, "allocations"), object_counts(candidate_objects, "allocations"), args.top),
        "",
    )
    print("")

    print_section(
        "Top object deallocation deltas",
        top_deltas(object_counts(baseline_objects, "deallocations"), object_counts(candidate_objects, "deallocations"), args.top),
        "",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
