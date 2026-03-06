package main

import (
	"fmt"
	"math"
	"os"
	"strconv"
)

const (
	PI          = 3.141592653589793
	SOLAR_MASS  = 4.0 * PI * PI
	DAYS_PER_YEAR = 365.24
)

type Body struct {
	x, y, z float64
	vx, vy, vz float64
	mass float64
}

func makeBodies() []Body {
	return []Body{
		{0, 0, 0, 0, 0, 0, SOLAR_MASS},
		{
			4.84143144246472090e00,
			-1.16032004402742839e00,
			-1.03622044471123109e-01,
			1.66007664274403694e-03 * DAYS_PER_YEAR,
			7.69901118419740425e-03 * DAYS_PER_YEAR,
			-6.90460016972063023e-05 * DAYS_PER_YEAR,
			9.54791938424326609e-04 * SOLAR_MASS,
		},
		{
			8.34336671824457987e00,
			4.12479856412430479e00,
			-4.03523417114321381e-01,
			-2.76742510726862411e-03 * DAYS_PER_YEAR,
			4.99852801234917238e-03 * DAYS_PER_YEAR,
			2.30417297573763929e-05 * DAYS_PER_YEAR,
			2.85885980666130812e-04 * SOLAR_MASS,
		},
		{
			1.28943695621391310e01,
			-1.51111514016986312e01,
			-2.23307578892655734e-01,
			2.96460137564761618e-03 * DAYS_PER_YEAR,
			2.37847173959480950e-03 * DAYS_PER_YEAR,
			-2.96589568540237556e-05 * DAYS_PER_YEAR,
			4.36624404335156298e-05 * SOLAR_MASS,
		},
		{
			1.53796971148509165e01,
			-2.59193146099879641e01,
			1.79258772950371181e-01,
			2.68067772490389322e-03 * DAYS_PER_YEAR,
			1.62824170038242295e-03 * DAYS_PER_YEAR,
			-9.51592254519715870e-05 * DAYS_PER_YEAR,
			5.15138902046611451e-05 * SOLAR_MASS,
		},
	}
}

func offsetMomentum(bodies []Body) {
	var px, py, pz float64
	for i := range bodies {
		px += bodies[i].vx * bodies[i].mass
		py += bodies[i].vy * bodies[i].mass
		pz += bodies[i].vz * bodies[i].mass
	}
	bodies[0].vx = -px / SOLAR_MASS
	bodies[0].vy = -py / SOLAR_MASS
	bodies[0].vz = -pz / SOLAR_MASS
}

func advance(bodies []Body, dt float64, steps int) {
	for s := 0; s < steps; s++ {
		for i := 0; i < len(bodies)-1; i++ {
			for j := i + 1; j < len(bodies); j++ {
				dx := bodies[i].x - bodies[j].x
				dy := bodies[i].y - bodies[j].y
				dz := bodies[i].z - bodies[j].z
				d2 := dx*dx + dy*dy + dz*dz
				d := math.Sqrt(d2)
				mag := dt / (d2 * d)
				bim := bodies[i].mass * mag
				bjm := bodies[j].mass * mag

				bodies[i].vx -= dx * bjm
				bodies[i].vy -= dy * bjm
				bodies[i].vz -= dz * bjm
				bodies[j].vx += dx * bim
				bodies[j].vy += dy * bim
				bodies[j].vz += dz * bim
			}
		}
		for i := range bodies {
			bodies[i].x += dt * bodies[i].vx
			bodies[i].y += dt * bodies[i].vy
			bodies[i].z += dt * bodies[i].vz
		}
	}
}

func energy(bodies []Body) float64 {
	var e float64
	for i := range bodies {
		b := bodies[i]
		e += 0.5 * b.mass * (b.vx*b.vx + b.vy*b.vy + b.vz*b.vz)
		for j := i + 1; j < len(bodies); j++ {
			dx := b.x - bodies[j].x
			dy := b.y - bodies[j].y
			dz := b.z - bodies[j].z
			e -= (b.mass * bodies[j].mass) / math.Sqrt(dx*dx+dy*dy+dz*dz)
		}
	}
	return e
}

func main() {
	steps := 5000
	if len(os.Args) > 1 {
		if parsed, err := strconv.Atoi(os.Args[1]); err == nil {
			steps = parsed
		}
	}
	bodies := makeBodies()
	offsetMomentum(bodies)
	before := energy(bodies)
	advance(bodies, 0.01, steps)
	after := energy(bodies)
	fmt.Printf("%.9f\n", before)
	fmt.Printf("%.9f\n", after)
}
