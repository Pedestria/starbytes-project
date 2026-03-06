# Track A Workload Contracts

This directory defines the Track A kernel contracts and shared inputs used by all language implementations.

Phase 1 scope in this repository currently includes the following implemented kernels:
- `binary-trees`
- `n-body`
- `spectral-norm`

Notes:
- Python/Go/Rust implementations follow straightforward Benchmarks Game style kernels.
- Starbytes implementations are Phase 1 foundation equivalents (non-recursive variants where needed)
  to keep the full harness runnable on current language/runtime constraints.

Planned kernels (next increment):
- `fasta`
- `k-nucleotide`
- `regex-redux`

All implementations must consume the shared constants in `benchmark/data/track_a/track_a_inputs.json`.
