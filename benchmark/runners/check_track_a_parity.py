#!/usr/bin/env python3
import argparse
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys

WORKLOAD_FILES = {
    "binary-trees": "binary_trees",
    "n-body": "n_body",
    "spectral-norm": "spectral_norm",
    "fasta": "fasta",
    "k-nucleotide": "k_nucleotide",
    "regex-redux": "regex_redux",
}

ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def default_starbytes_bin(root: Path) -> str:
    candidate = root / "build" / "bin" / "starbytes"
    if candidate.exists():
        return str(candidate)
    return "starbytes"


def workload_args(inputs: dict, workload: str, root: Path) -> list[str]:
    if workload == "binary-trees":
        return [str(inputs[workload]["max_depth"])]
    if workload == "n-body":
        return [str(inputs[workload]["steps"])]
    if workload == "spectral-norm":
        config = inputs[workload]
        return [str(config["n"]), str(config["iterations"])]
    if workload == "fasta":
        config = inputs[workload]
        return [str(config["repeat_length"]), str(config["random_length"]), str(config["seed"])]
    if workload in {"k-nucleotide", "regex-redux"}:
        input_file = root / inputs[workload]["input_file"]
        return [str(input_file)]
    raise ValueError(f"Unknown workload: {workload}")


def source_path(root: Path, language: str, workload: str) -> Path:
    stem = WORKLOAD_FILES[workload]
    suffix = {
        "starbytes": ".starb",
        "python": ".py",
    }[language]
    return root / "benchmark" / "languages" / language / "track_a" / f"{stem}{suffix}"


def clean_lines(raw: str) -> list[str]:
    cleaned = ANSI_RE.sub("", raw)
    lines: list[str] = []
    for line in cleaned.splitlines():
        text = line.strip()
        if not text:
            continue
        if text == "Interp Starting":
            continue
        lines.append(text)
    return lines


def parse_numeric(text: str):
    try:
        if text.lower().startswith(("0x", "-0x", "+0x")):
            return None
        return float(text)
    except ValueError:
        return None


def compare_outputs(workload: str,
                    starbytes_lines: list[str],
                    python_lines: list[str],
                    tolerance: float) -> list[str]:
    problems: list[str] = []
    if len(starbytes_lines) != len(python_lines):
        problems.append(
            f"{workload}: line count mismatch (starbytes={len(starbytes_lines)} python={len(python_lines)})"
        )
        return problems
    for index, (star_line, py_line) in enumerate(zip(starbytes_lines, python_lines), start=1):
        star_num = parse_numeric(star_line)
        py_num = parse_numeric(py_line)
        if star_num is not None and py_num is not None:
            if not math.isclose(star_num, py_num, rel_tol=tolerance, abs_tol=tolerance):
                problems.append(
                    f"{workload}: numeric mismatch on line {index}: starbytes={star_line} python={py_line}"
                )
            continue
        if star_line != py_line:
            problems.append(
                f"{workload}: text mismatch on line {index}: starbytes={star_line!r} python={py_line!r}"
            )
    return problems


def run_command(command: list[str], cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=cwd, text=True, capture_output=True)


def main() -> int:
    root = repo_root()
    parser = argparse.ArgumentParser(description="Check Track A output parity between Starbytes and Python.")
    parser.add_argument("--inputs", default=str(root / "benchmark" / "data" / "track_a" / "track_a_inputs.json"))
    parser.add_argument("--starbytes-bin", default=default_starbytes_bin(root))
    parser.add_argument("--python-bin", default=sys.executable or "python3")
    parser.add_argument("--tolerance", type=float, default=1e-5)
    parser.add_argument("--workload", action="append", choices=sorted(WORKLOAD_FILES.keys()))
    args = parser.parse_args()

    inputs = load_json(Path(args.inputs))
    native_dir = root / "build" / "stdlib"
    if not native_dir.exists():
        raise SystemExit(f"Missing Starbytes native stdlib directory: {native_dir}")

    workloads = args.workload or list(WORKLOAD_FILES.keys())
    failures: list[str] = []
    for workload in workloads:
        run_args = workload_args(inputs, workload, root)
        starbytes_cmd = [
            args.starbytes_bin,
            "run",
            str(source_path(root, "starbytes", workload)),
            "-L",
            str(native_dir),
            "--",
            *run_args,
        ]
        python_cmd = [
            args.python_bin,
            str(source_path(root, "python", workload)),
            *run_args,
        ]

        starbytes_result = run_command(starbytes_cmd, root)
        if starbytes_result.returncode != 0:
            failures.append(
                f"{workload}: starbytes exited with {starbytes_result.returncode}\n"
                f"stdout:\n{starbytes_result.stdout}\n"
                f"stderr:\n{starbytes_result.stderr}"
            )
            continue

        python_result = run_command(python_cmd, root)
        if python_result.returncode != 0:
            failures.append(
                f"{workload}: python exited with {python_result.returncode}\n"
                f"stdout:\n{python_result.stdout}\n"
                f"stderr:\n{python_result.stderr}"
            )
            continue

        failures.extend(
            compare_outputs(
                workload,
                clean_lines(starbytes_result.stdout),
                clean_lines(python_result.stdout),
                args.tolerance,
            )
        )

    if failures:
        for failure in failures:
            print(failure, file=sys.stderr)
        return 1

    print("TRACK-A-PARITY-OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
