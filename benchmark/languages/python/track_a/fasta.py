#!/usr/bin/env python3
import sys

SOURCE = "GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGG"
WEIGHTS = (
    ("A", 270),
    ("C", 120),
    ("G", 120),
    ("T", 270),
    ("N", 220),
)


def count_repeat(length: int) -> dict[str, int]:
    counts = {key: 0 for key, _ in WEIGHTS}
    for i in range(length):
        base = SOURCE[i % len(SOURCE)]
        counts[base] += 1
    return counts


def choose_base(value: int) -> str:
    total = 0
    for base, weight in WEIGHTS:
        total += weight
        if value < total:
            return base
    return "N"


def count_random(length: int, seed: int) -> dict[str, int]:
    counts = {key: 0 for key, _ in WEIGHTS}
    state = seed
    for _ in range(length):
        state = (state * 3877 + 29573) % 139968
        base = choose_base(state % 1000)
        counts[base] += 1
    return counts


def main() -> int:
    repeat_length = int(sys.argv[1]) if len(sys.argv) > 1 else 4000
    random_length = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
    seed = int(sys.argv[3]) if len(sys.argv) > 3 else 42

    repeat_counts = count_repeat(repeat_length)
    random_counts = count_random(random_length, seed)

    print(repeat_length)
    print(random_length)
    print(repeat_counts["A"])
    print(repeat_counts["C"])
    print(repeat_counts["G"])
    print(repeat_counts["T"])
    print(random_counts["A"])
    print(random_counts["C"])
    print(random_counts["G"])
    print(random_counts["T"])
    print(random_counts["N"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
