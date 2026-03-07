package main

import (
	"fmt"
	"os"
	"strings"
)

func normalizeSequence(path string) (string, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return "", err
	}
	var builder strings.Builder
	for _, line := range strings.Split(string(data), "\n") {
		trimmed := strings.TrimSpace(line)
		if trimmed != "" && !strings.HasPrefix(trimmed, ">") {
			builder.WriteString(strings.ToUpper(trimmed))
		}
	}
	return builder.String(), nil
}

func countKmers(sequence string, k int) map[string]int {
	counts := map[string]int{}
	if len(sequence) < k {
		return counts
	}
	for i := 0; i <= len(sequence)-k; i++ {
		token := sequence[i : i+k]
		counts[token]++
	}
	return counts
}

func main() {
	path := "benchmark/data/track_a/dna_input.fasta"
	if len(os.Args) > 1 {
		path = os.Args[1]
	}
	sequence, err := normalizeSequence(path)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}

	freq1 := countKmers(sequence, 1)
	freq2 := countKmers(sequence, 2)
	freq3 := countKmers(sequence, 3)
	freq4 := countKmers(sequence, 4)
	freq6 := countKmers(sequence, 6)
	freq8 := countKmers(sequence, 8)

	fmt.Println(len(sequence))
	fmt.Println(len(freq1))
	fmt.Println(len(freq2))
	fmt.Println(len(freq3))
	fmt.Println(len(freq4))
	fmt.Println(len(freq6))
	fmt.Println(len(freq8))
	fmt.Println(freq3["GGT"])
	fmt.Println(freq4["GGTA"])
	fmt.Println(freq6["GGTATT"])
	fmt.Println(freq8["AGGGTAAA"])
}
