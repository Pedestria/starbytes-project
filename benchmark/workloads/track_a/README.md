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
- Phase 0 parity alignment in this repository targets the local Python reference implementations under `benchmark/languages/python/track_a`.
- `fasta` remains the current simplified count-based kernel in Phase 0; it is intentionally aligned to the local Python implementation rather than the full benchmark-game generator variant.
- Use `benchmark/runners/check_track_a_parity.py` to validate Starbytes output against Python before trusting timing comparisons.
- The lighter regression-gate inputs are stored in `benchmark/data/track_a/track_a_parity_inputs.json`; benchmark reporting still uses `benchmark/data/track_a/track_a_inputs.json`.
