# Starbytes Cross-Language Benchmark Spec

Date: February 26, 2026  
Project: Starbytes  
Scope: Compare Starbytes against Python, Go, and Rust across algorithmic, scripting, and web-service workloads using reproducible methodology.

## 1. Goals

- Measure Starbytes performance in representative real developer scenarios, not only synthetic microbenchmarks.
- Produce statistically meaningful comparisons against popular reference languages:
  - Python (CPython)
  - Go
  - Rust
- Report where Starbytes is strong/weak by workload class:
  - compute-heavy
  - text/data scripting
  - request/response server paths

## 2. Benchmark Tracks

### Track A: Algorithmic Kernels (Benchmarks Game Inspired)

Use a constrained subset inspired by the Computer Language Benchmarks Game:
- `binary-trees`
- `n-body`
- `spectral-norm`
- `fasta`
- `k-nucleotide`
- `regex-redux`

Purpose:
- Compare raw CPU throughput and memory behavior under known benchmark kernels.

Rules:
- Use semantically equivalent implementations across languages.
- Prefer single-thread baseline implementations first.
- Add an optional parallel variant only when all languages have a fair/idiomatic equivalent.

### Track B: Scripting/Data Workloads (pyperformance Inspired)

Use Starbytes equivalents of common pyperformance-style categories:
- JSON load/transform/dump (small, medium, large payloads)
- Regex extraction and replacement on multi-MB text
- Array/Dict/Map-heavy transform pipeline
- Startup-sensitive short scripts (process startup + simple task)
- Unicode normalization/case/segment workloads

Purpose:
- Measure realistic runtime behavior for scripting and tooling use cases.

### Track C: Web/API Workloads (TechEmpower Inspired)

Implement minimal equivalent endpoints:
- Plaintext response
- JSON response
- Optional DB-free mixed handler (routing + serialization only)

Purpose:
- Measure request throughput, tail latency, and runtime overhead in service patterns.

## 3. Fairness and Comparison Policy

- Same workload definition and input data for all languages.
- No language-specific unsafe intrinsics unless all implementations use equivalent low-level tuning.
- Keep implementation style idiomatic and maintainable; avoid intentionally unidiomatic code.
- Separate two execution modes:
  - `TTFR` (time-to-first-result): source invocation path (`run`-style).
  - `Steady-State`: ahead-of-time/prebuilt artifacts where applicable.
- Publish full source for all benchmark implementations.

## 4. Environment Controls

- Primary host: dedicated Linux machine (no background CI load during runs).
- Fix CPU scaling governor (`performance`) and pin benchmark process affinity.
- Disable turbo scaling only if required for stability (record whether enabled).
- Record machine metadata in results:
  - CPU model
  - core/thread count
  - RAM
  - kernel version
  - OS version
- Record toolchain versions per run:
  - Starbytes version + commit
  - Python version
  - Go version
  - Rust + cargo version

## 5. Measurement Tooling

- Command-level timing and summary:
  - `hyperfine` (warmups, multiple runs, JSON export)
- Python-specific benchmark rigor:
  - `pyperf` / `pyperformance` for controlled repeated execution
- Go benchmark analysis:
  - `go test -bench`
  - `benchstat` for A/B summaries
- Rust benchmark analysis:
  - `cargo bench`
  - `criterion` for stable statistical microbench runs
- Memory:
  - Linux: `/usr/bin/time -v` (max RSS)
  - macOS fallback (local dev only): `/usr/bin/time -l`

## 6. Metrics

Required:
- Runtime: median, p95, stddev/confidence interval
- Throughput (req/s) for Track C
- Tail latency (p95, p99) for Track C
- Peak RSS memory
- Build time (where applicable)
- Incremental rebuild time
- Artifact size

Optional (Phase 2+):
- CPU cycles/instructions/cache misses (`perf stat`)
- Startup latency decomposition (process start vs work execution)

## 7. Output and Scoring

- Store raw results in machine-readable files (JSON/CSV) per run.
- Publish per-track tables (A/B/C); do not collapse into one global score initially.
- Normalization:
  - Use Python baseline `= 1.0` for relative runtime on each workload.
- Track score:
  - Geometric mean of relative runtime across workloads in that track.
- Final summary:
  - Keep separate track scores; if a single composite is shown, include explicit weights.

## 8. Repository Layout (Planned)

```text
benchmark/
  StarbytesCrossLanguageBenchmarkSpec.md
  workloads/
    track_a/
    track_b/
    track_c/
  runners/
    run_track_a.sh
    run_track_b.sh
    run_track_c.sh
    run_all.sh
  languages/
    starbytes/
    python/
    go/
    rust/
  data/
  results/
    raw/
    summaries/
```

## 9. Execution Plan

### Phase 1: Foundation

1. Define workload contracts and shared test data.
2. Implement Track A workloads in all four languages.
3. Add `hyperfine` harness and standardized output schema.
4. Produce first reproducible report for Track A.

### Phase 2: Scripting Reality Check

1. Implement Track B workloads.
2. Add Python `pyperf`/`pyperformance` validation passes.
3. Add Starbytes compile profiling correlation (`--profile-compile-out`) for Starbytes build/TTFR paths.

### Phase 3: Service Path

1. Implement Track C minimal servers.
2. Run throughput/latency tests with fixed client settings.
3. Publish per-track and cross-track summary.

## 10. Acceptance Criteria

- Each track has:
  - documented workload definitions
  - runnable scripts
  - reproducible output artifacts
- At least 20 measured runs per command (after warmup) unless workload duration is long enough to justify fewer runs.
- Benchmark reports include:
  - exact commit SHAs
  - toolchain versions
  - host metadata
  - raw + summarized results

## 11. Risk Controls

- Prevent benchmark drift: lock input data and workload specs.
- Prevent accidental bias: enforce code review for benchmark implementations and harness scripts.
- Prevent overfitting: include both synthetic kernels and real scripting/service workloads.

## 12. External Reference Baselines

- Benchmarks Game methodology and kernel definitions:
  - https://benchmarksgame-team.pages.debian.net/benchmarksgame/how-programs-are-measured.html
- Python benchmarking suites:
  - https://pyperformance.readthedocs.io/
  - https://pyperf.readthedocs.io/en/latest/runner.html
- Go benchmark format and analysis:
  - https://pkg.go.dev/testing/
  - https://pkg.go.dev/golang.org/x/perf/cmd
- Rust benchmark execution:
  - https://doc.rust-lang.org/cargo/commands/cargo-bench.html
  - https://docs.rs/crate/criterion/latest
- Web benchmark reference patterns:
  - https://github.com/TechEmpower/FrameworkBenchmarks
