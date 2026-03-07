#!/usr/bin/env python3
import argparse
import datetime as dt
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
from dataclasses import dataclass


WORKLOAD_FILES = {
    "binary-trees": "binary_trees",
    "n-body": "n_body",
    "spectral-norm": "spectral_norm",
    "fasta": "fasta",
    "k-nucleotide": "k_nucleotide",
    "regex-redux": "regex_redux",
}


@dataclass
class BenchmarkCommand:
    label: str
    command: str


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


def default_rustc_bin() -> str:
    rustup_root = Path.home() / ".rustup" / "toolchains"
    if rustup_root.exists():
        candidates = sorted(rustup_root.glob("*/bin/rustc"))
        if candidates:
            return str(candidates[0])
    return "rustc"


def quoted(parts: list[str]) -> str:
    return shlex.join([str(part) for part in parts])


def require_executable(name: str, command: str) -> None:
    if os.path.sep in command:
        if not Path(command).exists():
            raise SystemExit(f"Missing required tool `{name}`: {command}")
        return
    if shutil.which(command) is None:
        raise SystemExit(f"Missing required tool `{name}`: {command}")


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
    if language == "starbytes":
        return root / "benchmark" / "languages" / language / "track_a" / f"{stem}.starb"
    if language == "python":
        return root / "benchmark" / "languages" / language / "track_a" / f"{stem}.py"
    if language == "go":
        return root / "benchmark" / "languages" / language / "track_a" / f"{stem}.go"
    if language == "rust":
        return root / "benchmark" / "languages" / language / "track_a" / f"{stem}.rs"
    raise ValueError(f"Unknown language: {language}")


def build_starbytes_command(root: Path,
                            starbytes_bin: str,
                            native_dir: Path,
                            build_dir: Path,
                            workload: str,
                            args: list[str],
                            mode: str) -> tuple[BenchmarkCommand, str | None]:
    src = source_path(root, "starbytes", workload)
    cache_dir = build_dir / "starbytes-cache"
    cache_dir.mkdir(parents=True, exist_ok=True)

    if mode == "ttfr":
        command = quoted([
            starbytes_bin,
            "run",
            str(src),
            "-d",
            str(cache_dir),
            "-L",
            str(native_dir),
            "--",
            *args,
        ])
        prepare = quoted(["rm", "-rf", str(cache_dir)]) + " && " + quoted(["mkdir", "-p", str(cache_dir)])
        return BenchmarkCommand("starbytes", command), prepare

    output_path = build_dir / "starbytes-steady" / f"{WORKLOAD_FILES[workload]}.starbmod"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    warmup_cmd = [
        starbytes_bin,
        "compile",
        str(src),
        "-o",
        str(output_path),
        "-d",
        str(cache_dir),
        "-L",
        str(native_dir),
        "--no-run",
        "--",
        *args,
    ]
    subprocess.run(warmup_cmd, cwd=root, check=True)
    command = quoted([
        starbytes_bin,
        "compile",
        str(src),
        "-o",
        str(output_path),
        "-d",
        str(cache_dir),
        "-L",
        str(native_dir),
        "--run",
        "--",
        *args,
    ])
    return BenchmarkCommand("starbytes", command), None


def build_python_command(root: Path, python_bin: str, workload: str, args: list[str]) -> BenchmarkCommand:
    src = source_path(root, "python", workload)
    return BenchmarkCommand("python", quoted([python_bin, str(src), *args]))


def build_go_command(root: Path,
                     go_bin: str,
                     build_dir: Path,
                     workload: str,
                     args: list[str],
                     mode: str) -> BenchmarkCommand:
    src = source_path(root, "go", workload)
    cache_dir = build_dir / "go-env" / "cache"
    tmp_dir = build_dir / "go-env" / "tmp"
    cache_dir.mkdir(parents=True, exist_ok=True)
    tmp_dir.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["GOCACHE"] = str(cache_dir)
    env["GOTMPDIR"] = str(tmp_dir)
    if mode == "ttfr":
        return BenchmarkCommand(
            "go",
            quoted([
                "env",
                f"GOCACHE={cache_dir}",
                f"GOTMPDIR={tmp_dir}",
                go_bin,
                "run",
                str(src),
                *args,
            ]),
        )

    binary = build_dir / "go-steady" / WORKLOAD_FILES[workload]
    binary.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([go_bin, "build", "-o", str(binary), str(src)], cwd=root, check=True, env=env)
    return BenchmarkCommand("go", quoted([str(binary), *args]))


def build_rust_command(root: Path,
                       rustc_bin: str,
                       build_dir: Path,
                       workload: str,
                       args: list[str],
                       mode: str) -> BenchmarkCommand:
    src = source_path(root, "rust", workload)
    binary = build_dir / ("rust-ttfr" if mode == "ttfr" else "rust-steady") / WORKLOAD_FILES[workload]
    binary.parent.mkdir(parents=True, exist_ok=True)

    if mode == "ttfr":
        command = quoted([rustc_bin, "-O", str(src), "-o", str(binary)]) + " && " + quoted([str(binary), *args])
        return BenchmarkCommand("rust", command)

    subprocess.run([rustc_bin, "-O", str(src), "-o", str(binary)], cwd=root, check=True)
    return BenchmarkCommand("rust", quoted([str(binary), *args]))


def build_commands(root: Path,
                   inputs: dict,
                   args: argparse.Namespace,
                   workload: str,
                   build_dir: Path) -> tuple[list[BenchmarkCommand], str | None]:
    run_args = workload_args(inputs, workload, root)
    native_dir = root / "build" / "stdlib"
    if not native_dir.exists():
        raise SystemExit(f"Missing Starbytes native stdlib directory: {native_dir}")

    starbytes_cmd, prepare = build_starbytes_command(
        root, args.starbytes_bin, native_dir, build_dir, workload, run_args, args.mode
    )
    commands = [
        starbytes_cmd,
        build_python_command(root, args.python_bin, workload, run_args),
        build_go_command(root, args.go_bin, build_dir, workload, run_args, args.mode),
        build_rust_command(root, args.rustc_bin, build_dir, workload, run_args, args.mode),
    ]
    return commands, prepare


def run_smoke(root: Path, workload: str, commands: list[BenchmarkCommand]) -> None:
    for command in commands:
        result = subprocess.run(command.command, cwd=root, shell=True, text=True, capture_output=True)
        if result.returncode != 0:
            sys.stderr.write(result.stdout)
            sys.stderr.write(result.stderr)
            raise SystemExit(f"Smoke run failed for {workload} [{command.label}]")


def run_hyperfine(root: Path,
                  hyperfine_bin: str,
                  workload: str,
                  commands: list[BenchmarkCommand],
                  prepare: str | None,
                  raw_path: Path,
                  runs: int,
                  warmup: int) -> None:
    hyperfine_cmd = [
        hyperfine_bin,
        "--runs",
        str(runs),
        "--warmup",
        str(warmup),
        "--export-json",
        str(raw_path),
    ]
    if prepare:
        hyperfine_cmd.extend(["--prepare", prepare])
    for command in commands:
        hyperfine_cmd.extend(["-n", command.label, command.command])
    subprocess.run(hyperfine_cmd, cwd=root, check=True)


def write_dry_run(mode: str, workload_commands: dict[str, list[BenchmarkCommand]]) -> None:
    print(f"Track A {mode} command plan")
    for workload, commands in workload_commands.items():
        print(f"[{workload}]")
        for command in commands:
            print(f"{command.label}: {command.command}")


def main() -> int:
    root = repo_root()
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", required=True, choices=["ttfr", "steady-state"])
    parser.add_argument("--runs", type=int, default=20)
    parser.add_argument("--warmup", type=int, default=3)
    parser.add_argument("--contracts", default=str(root / "benchmark" / "workloads" / "track_a" / "contracts.json"))
    parser.add_argument("--inputs", default=str(root / "benchmark" / "data" / "track_a" / "track_a_inputs.json"))
    parser.add_argument("--starbytes-bin", default=default_starbytes_bin(root))
    parser.add_argument("--python-bin", default=sys.executable or "python3")
    parser.add_argument("--go-bin", default="go")
    parser.add_argument("--rustc-bin", default=default_rustc_bin())
    parser.add_argument("--hyperfine-bin", default="hyperfine")
    parser.add_argument("--raw-root", default=str(root / "benchmark" / "results" / "raw"))
    parser.add_argument("--summary-root", default=str(root / "benchmark" / "results" / "summaries"))
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--smoke", action="store_true")
    args = parser.parse_args()

    contracts = load_json(Path(args.contracts))
    inputs = load_json(Path(args.inputs))
    implemented_workloads = contracts["implemented_workloads"]

    require_executable("starbytes", args.starbytes_bin)
    require_executable("python", args.python_bin)
    require_executable("go", args.go_bin)
    require_executable("rustc", args.rustc_bin)
    if not args.smoke and not args.dry_run:
        require_executable("hyperfine", args.hyperfine_bin)

    timestamp = dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    raw_root = Path(args.raw_root)
    summary_root = Path(args.summary_root)
    run_raw_dir = raw_root / f"track_a_{args.mode}_{timestamp}"
    build_dir = raw_root / ".build" / f"track_a_{args.mode}_{timestamp}"
    run_raw_dir.mkdir(parents=True, exist_ok=True)
    build_dir.mkdir(parents=True, exist_ok=True)
    summary_root.mkdir(parents=True, exist_ok=True)

    workload_commands: dict[str, list[BenchmarkCommand]] = {}
    workload_prepare: dict[str, str | None] = {}
    for workload in implemented_workloads:
        commands, prepare = build_commands(root, inputs, args, workload, build_dir)
        workload_commands[workload] = commands
        workload_prepare[workload] = prepare

    if args.dry_run:
        write_dry_run(args.mode, workload_commands)
        return 0

    if args.smoke:
        for workload in implemented_workloads:
            run_smoke(root, workload, workload_commands[workload])
        return 0

    for workload in implemented_workloads:
        raw_path = run_raw_dir / f"{args.mode}_{workload}.hyperfine.json"
        run_hyperfine(
            root,
            args.hyperfine_bin,
            workload,
            workload_commands[workload],
            workload_prepare[workload],
            raw_path,
            args.runs,
            args.warmup,
        )

    summary_path = summary_root / f"track_a_{args.mode}_{timestamp}.summary.json"
    report_path = summary_root / f"track_a_{args.mode}_{timestamp}.report.md"
    summarize_cmd = [
        args.python_bin,
        str(root / "benchmark" / "runners" / "summarize_track_a.py"),
        "--mode",
        args.mode,
        "--inputs",
        str(Path(args.inputs)),
        "--contracts",
        str(Path(args.contracts)),
        "--raw-dir",
        str(run_raw_dir),
        "--summary",
        str(summary_path),
        "--report",
        str(report_path),
        "--starbytes-bin",
        args.starbytes_bin,
        "--python-bin",
        args.python_bin,
        "--go-bin",
        args.go_bin,
        "--rustc-bin",
        args.rustc_bin,
        "--hyperfine-bin",
        args.hyperfine_bin,
    ]
    subprocess.run(summarize_cmd, cwd=root, check=True)
    print(report_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
