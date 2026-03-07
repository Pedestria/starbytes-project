#!/usr/bin/env python3
import sys


def normalize_sequence(path: str) -> str:
    pieces: list[str] = []
    with open(path, "r", encoding="utf-8") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if line and not line.startswith(">"):
                pieces.append(line.upper())
    return "".join(pieces)


def count_kmers(sequence: str, k: int) -> dict[str, int]:
    counts: dict[str, int] = {}
    if len(sequence) < k:
        return counts
    for i in range(len(sequence) - k + 1):
        token = sequence[i : i + k]
        counts[token] = counts.get(token, 0) + 1
    return counts


def main() -> int:
    path = sys.argv[1] if len(sys.argv) > 1 else "benchmark/data/track_a/dna_input.fasta"
    sequence = normalize_sequence(path)
    freq1 = count_kmers(sequence, 1)
    freq2 = count_kmers(sequence, 2)
    freq3 = count_kmers(sequence, 3)
    freq4 = count_kmers(sequence, 4)
    freq6 = count_kmers(sequence, 6)
    freq8 = count_kmers(sequence, 8)

    print(len(sequence))
    print(len(freq1))
    print(len(freq2))
    print(len(freq3))
    print(len(freq4))
    print(len(freq6))
    print(len(freq8))
    print(freq3.get("GGT", 0))
    print(freq4.get("GGTA", 0))
    print(freq6.get("GGTATT", 0))
    print(freq8.get("AGGGTAAA", 0))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
