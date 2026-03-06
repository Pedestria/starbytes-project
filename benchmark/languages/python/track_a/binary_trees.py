#!/usr/bin/env python3
import sys


class Node:
    __slots__ = ("item", "left", "right")

    def __init__(self, item: int, left: "Node | None", right: "Node | None"):
        self.item = item
        self.left = left
        self.right = right


def make_tree(item: int, depth: int) -> Node:
    if depth == 0:
        return Node(item, None, None)
    child_depth = depth - 1
    return Node(item, make_tree(item * 2 - 1, child_depth), make_tree(item * 2, child_depth))


def check_tree(node: Node | None) -> int:
    if node is None:
        return 0
    return node.item + check_tree(node.left) - check_tree(node.right)


def run(max_depth: int) -> int:
    min_depth = 4
    stretch_depth = max_depth + 1

    checksum = check_tree(make_tree(0, stretch_depth))
    long_lived = make_tree(0, max_depth)

    depth = min_depth
    while depth <= max_depth:
        iterations = 1 << (max_depth - depth + min_depth)
        i = 1
        while i <= iterations:
            checksum += check_tree(make_tree(i, depth))
            checksum += check_tree(make_tree(-i, depth))
            i += 1
        depth += 2

    checksum += check_tree(long_lived)
    return checksum


def main() -> int:
    max_depth = int(sys.argv[1]) if len(sys.argv) > 1 else 12
    print(run(max_depth))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
