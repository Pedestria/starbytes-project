package main

import (
	"fmt"
	"os"
	"strconv"
)

type Node struct {
	item       int
	left,right *Node
}

func makeTree(item int, depth int) *Node {
	if depth == 0 {
		return &Node{item: item}
	}
	childDepth := depth - 1
	return &Node{
		item:  item,
		left:  makeTree(item*2-1, childDepth),
		right: makeTree(item*2, childDepth),
	}
}

func checkTree(node *Node) int {
	if node == nil {
		return 0
	}
	return node.item + checkTree(node.left) - checkTree(node.right)
}

func run(maxDepth int) int {
	minDepth := 4
	stretchDepth := maxDepth + 1

	checksum := checkTree(makeTree(0, stretchDepth))
	longLivedTree := makeTree(0, maxDepth)

	for depth := minDepth; depth <= maxDepth; depth += 2 {
		iterations := 1 << (maxDepth - depth + minDepth)
		for i := 1; i <= iterations; i++ {
			checksum += checkTree(makeTree(i, depth))
			checksum += checkTree(makeTree(-i, depth))
		}
	}

	checksum += checkTree(longLivedTree)
	return checksum
}

func main() {
	maxDepth := 12
	if len(os.Args) > 1 {
		if parsed, err := strconv.Atoi(os.Args[1]); err == nil {
			maxDepth = parsed
		}
	}
	fmt.Println(run(maxDepth))
}
