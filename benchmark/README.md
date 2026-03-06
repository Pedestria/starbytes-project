# Starbytes Benchmark Harness

Phase 1 benchmark foundation files are organized as:

- contracts: `benchmark/workloads/track_a/contracts.json`
- shared inputs: `benchmark/data/track_a/track_a_inputs.json`
- implementations: `benchmark/languages/<lang>/track_a/*`
- runners: `benchmark/runners/run_track_a.sh`, `benchmark/runners/run_all.sh`
- summaries: `benchmark/results/summaries/`

Quick start:

```bash
./benchmark/runners/run_track_a.sh --mode ttfr
```
