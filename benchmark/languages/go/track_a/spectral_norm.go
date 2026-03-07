package main

import (
	"fmt"
	"math"
	"os"
	"strconv"
)

func A(i, j int) float64 {
	ij := i + j
	return 1.0 / float64((ij*(ij+1)/2)+i+1)
}

func multiplyAv(v []float64) []float64 {
	n := len(v)
	out := make([]float64, n)
	for i := 0; i < n; i++ {
		s := 0.0
		for j := 0; j < n; j++ {
			s += A(i, j) * v[j]
		}
		out[i] = s
	}
	return out
}

func multiplyAtv(v []float64) []float64 {
	n := len(v)
	out := make([]float64, n)
	for i := 0; i < n; i++ {
		s := 0.0
		for j := 0; j < n; j++ {
			s += A(j, i) * v[j]
		}
		out[i] = s
	}
	return out
}

func multiplyAtAv(v []float64) []float64 {
	return multiplyAtv(multiplyAv(v))
}

func run(n, iterations int) float64 {
	u := make([]float64, n)
	for i := range u {
		u[i] = 1.0
	}
	v := make([]float64, n)
	for i := 0; i < iterations; i++ {
		v = multiplyAtAv(u)
		u = multiplyAtAv(v)
	}

	vBv := 0.0
	vv := 0.0
	for i := 0; i < n; i++ {
		vBv += u[i] * v[i]
		vv += v[i] * v[i]
	}
	return math.Sqrt(vBv / vv)
}

func main() {
	n := 120
	iterations := 8
	if len(os.Args) > 1 {
		if parsed, err := strconv.Atoi(os.Args[1]); err == nil {
			n = parsed
		}
	}
	if len(os.Args) > 2 {
		if parsed, err := strconv.Atoi(os.Args[2]); err == nil {
			iterations = parsed
		}
	}
	fmt.Printf("%.9f\n", run(n, iterations))
}
