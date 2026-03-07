#!/usr/bin/env python3
import math
import sys


def A(i: int, j: int) -> float:
    ij = i + j
    return 1.0 / ((ij * (ij + 1) // 2) + i + 1)


def multiply_av(v):
    n = len(v)
    out = [0.0] * n
    for i in range(n):
        s = 0.0
        for j in range(n):
            s += A(i, j) * v[j]
        out[i] = s
    return out


def multiply_atv(v):
    n = len(v)
    out = [0.0] * n
    for i in range(n):
        s = 0.0
        for j in range(n):
            s += A(j, i) * v[j]
        out[i] = s
    return out


def multiply_at_av(v):
    return multiply_atv(multiply_av(v))


def run(n: int, iterations: int) -> float:
    u = [1.0] * n
    v = [0.0] * n
    for _ in range(iterations):
        v = multiply_at_av(u)
        u = multiply_at_av(v)

    v_bv = 0.0
    vv = 0.0
    for i in range(n):
        v_bv += u[i] * v[i]
        vv += v[i] * v[i]
    return math.sqrt(v_bv / vv)


def main() -> int:
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 120
    iterations = int(sys.argv[2]) if len(sys.argv) > 2 else 8
    print(f"{run(n, iterations):.9f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
