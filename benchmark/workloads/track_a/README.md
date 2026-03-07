# Track A Workload Contracts

This directory defines the Track A kernel contracts and shared inputs used by all language implementations.

Phase 1 scope in this repository includes the following implemented kernels:
- `binary-trees`
- `n-body`
- `spectral-norm`
- `fasta`
- `k-nucleotide`
- `regex-redux`

Notes:
- All implementations consume the shared constants in `benchmark/data/track_a/track_a_inputs.json`.
- `k-nucleotide` and `regex-redux` both use the locked FASTA fixture at `benchmark/data/track_a/dna_input.fasta`.
- Phase 1 uses shared single-threaded workload sizes that remain practical for the current Starbytes runtime while keeping all four language implementations on identical inputs.
- The Starbytes `steady-state` runner path currently uses a warmed compile/run fallback because the toolchain does not yet expose a standalone compiled-module executor.
