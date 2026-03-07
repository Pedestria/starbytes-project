#!/usr/bin/env python3
import re
import sys

COUNT_PATTERNS = [
    "AGGGTAAA|TTTACCCT",
    "[CGT]GGGTAAA|TTTACCC[ACG]",
    "A[ACT]GGTAAA|TTTACC[AGT]T",
    "AG[ACT]GTAAA|TTTAC[AGT]CT",
    "AGG[ACT]TAAA|TTTA[AGT]CCT",
]


def normalize_sequence(path: str) -> str:
    pieces: list[str] = []
    with open(path, "r", encoding="utf-8") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if line and not line.startswith(">"):
                pieces.append(line.upper())
    return "".join(pieces)


def main() -> int:
    path = sys.argv[1] if len(sys.argv) > 1 else "benchmark/data/track_a/dna_input.fasta"
    with open(path, "r", encoding="utf-8") as handle:
        raw = handle.read()

    cleaned = normalize_sequence(path)
    counts = [len(re.findall(pattern, cleaned)) for pattern in COUNT_PATTERNS]

    replaced = cleaned
    replaced = re.sub("AGGGTAAA", "<A>", replaced)
    replaced = re.sub("TTTACCCT", "<B>", replaced)
    replaced = re.sub("CGGGTAAA", "<C>", replaced)

    print(len(raw))
    print(len(cleaned))
    for count in counts:
        print(count)
    print(len(replaced))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
