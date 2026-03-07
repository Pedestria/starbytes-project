#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

python3 "$ROOT/benchmark/runners/run_track_a.py" --mode ttfr "$@"
python3 "$ROOT/benchmark/runners/run_track_a.py" --mode steady-state "$@"
