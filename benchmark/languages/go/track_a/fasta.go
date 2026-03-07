package main

import (
	"fmt"
	"os"
	"strconv"
)

const source = "GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGG"

var orderedBases = []byte{'A', 'C', 'G', 'T', 'N'}
var baseWeights = []int{270, 120, 120, 270, 220}

func countRepeat(length int) map[byte]int {
	counts := map[byte]int{'A': 0, 'C': 0, 'G': 0, 'T': 0, 'N': 0}
	for i := 0; i < length; i++ {
		counts[source[i%len(source)]]++
	}
	return counts
}

func chooseBase(value int) byte {
	total := 0
	for i, weight := range baseWeights {
		total += weight
		if value < total {
			return orderedBases[i]
		}
	}
	return 'N'
}

func countRandom(length int, seed int) map[byte]int {
	counts := map[byte]int{'A': 0, 'C': 0, 'G': 0, 'T': 0, 'N': 0}
	state := seed
	for i := 0; i < length; i++ {
		state = (state*3877 + 29573) % 139968
		counts[chooseBase(state%1000)]++
	}
	return counts
}

func main() {
	repeatLength := 4000
	randomLength := 4000
	seed := 42
	if len(os.Args) > 1 {
		if parsed, err := strconv.Atoi(os.Args[1]); err == nil {
			repeatLength = parsed
		}
	}
	if len(os.Args) > 2 {
		if parsed, err := strconv.Atoi(os.Args[2]); err == nil {
			randomLength = parsed
		}
	}
	if len(os.Args) > 3 {
		if parsed, err := strconv.Atoi(os.Args[3]); err == nil {
			seed = parsed
		}
	}

	repeatCounts := countRepeat(repeatLength)
	randomCounts := countRandom(randomLength, seed)

	fmt.Println(repeatLength)
	fmt.Println(randomLength)
	fmt.Println(repeatCounts['A'])
	fmt.Println(repeatCounts['C'])
	fmt.Println(repeatCounts['G'])
	fmt.Println(repeatCounts['T'])
	fmt.Println(randomCounts['A'])
	fmt.Println(randomCounts['C'])
	fmt.Println(randomCounts['G'])
	fmt.Println(randomCounts['T'])
	fmt.Println(randomCounts['N'])
}
