package main

import (
	"fmt"
	"os"
	"regexp"
	"strings"
)

var countPatterns = []string{
	"AGGGTAAA|TTTACCCT",
	"[CGT]GGGTAAA|TTTACCC[ACG]",
	"A[ACT]GGTAAA|TTTACC[AGT]T",
	"AG[ACT]GTAAA|TTTAC[AGT]CT",
	"AGG[ACT]TAAA|TTTA[AGT]CCT",
}

func normalizeSequence(path string) (string, string, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return "", "", err
	}
	raw := string(data)
	var builder strings.Builder
	for _, line := range strings.Split(raw, "\n") {
		trimmed := strings.TrimSpace(line)
		if trimmed != "" && !strings.HasPrefix(trimmed, ">") {
			builder.WriteString(strings.ToUpper(trimmed))
		}
	}
	return raw, builder.String(), nil
}

func main() {
	path := "benchmark/data/track_a/dna_input.fasta"
	if len(os.Args) > 1 {
		path = os.Args[1]
	}
	raw, cleaned, err := normalizeSequence(path)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}

	counts := make([]int, 0, len(countPatterns))
	for _, pattern := range countPatterns {
		re := regexp.MustCompile(pattern)
		counts = append(counts, len(re.FindAllStringIndex(cleaned, -1)))
	}

	replaced := regexp.MustCompile("AGGGTAAA").ReplaceAllString(cleaned, "<A>")
	replaced = regexp.MustCompile("TTTACCCT").ReplaceAllString(replaced, "<B>")
	replaced = regexp.MustCompile("CGGGTAAA").ReplaceAllString(replaced, "<C>")

	fmt.Println(len(raw))
	fmt.Println(len(cleaned))
	for _, count := range counts {
		fmt.Println(count)
	}
	fmt.Println(len(replaced))
}
