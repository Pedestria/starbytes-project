# Track A Phase 1 Report (Foundation Slice)

Status: Implemented benchmark framework, contracts, shared input data, language workload sources, and hyperfine runner pipeline.

## Implemented Workloads

- binary-trees
- n-body
- spectral-norm

## Implemented Languages

- starbytes
- python
- go (source present)
- rust (source present)

## Run Commands

```bash
# TTFR mode
./benchmark/runners/run_track_a.sh --mode ttfr --runs 20 --warmup 3

# Steady-state mode
./benchmark/runners/run_track_a.sh --mode steady-state --runs 20 --warmup 3

# Full Track A
./benchmark/runners/run_all.sh --runs 20 --warmup 3
```

Generated artifacts:
- raw: `benchmark/results/raw/*.hyperfine.json`
- summary JSON: `benchmark/results/summaries/track_a_<mode>_<timestamp>.summary.json`
- report markdown: `benchmark/results/summaries/track_a_<mode>_<timestamp>.report.md`

## Local Validation Notes

- starbytes + python kernels have been smoke-validated locally.
- go/rust/hyperfine execution depends on host toolchain availability.

## Next Increment (Phase 1 completion)

- Add `fasta`, `k-nucleotide`, and `regex-redux` kernels to all language sets.
- Execute full measured runs on a host with all toolchains + hyperfine installed.
