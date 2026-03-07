# Starbytes Benchmark Harness

Phase 1 benchmark files are organized as:

- contracts: `benchmark/workloads/track_a/contracts.json`
- shared inputs: `benchmark/data/track_a/track_a_inputs.json`
- shared FASTA fixture: `benchmark/data/track_a/dna_input.fasta`
- implementations: `benchmark/languages/<lang>/track_a/*`
- runners: `benchmark/runners/run_track_a.sh`, `benchmark/runners/run_all.sh`, `benchmark/runners/run_track_a.py`, `benchmark/runners/check_track_a_parity.py`
- summaries: `benchmark/results/summaries/`

Quick start:

```bash
./benchmark/runners/run_track_a.sh --mode ttfr --runs 20 --warmup 3
```

Local smoke validation without `hyperfine`:

```bash
./benchmark/runners/run_track_a.sh --mode ttfr --smoke
```

Output parity validation against the local Python reference implementations:

```bash
./benchmark/runners/check_track_a_parity.py
```

Smaller parity gate inputs for the full regression suite live at `benchmark/data/track_a/track_a_parity_inputs.json`.
