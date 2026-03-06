use std::env;

const PI: f64 = 3.141592653589793;
const SOLAR_MASS: f64 = 4.0 * PI * PI;
const DAYS_PER_YEAR: f64 = 365.24;

#[derive(Clone, Copy)]
struct Body {
    x: f64,
    y: f64,
    z: f64,
    vx: f64,
    vy: f64,
    vz: f64,
    mass: f64,
}

fn make_bodies() -> Vec<Body> {
    vec![
        Body {
            x: 0.0,
            y: 0.0,
            z: 0.0,
            vx: 0.0,
            vy: 0.0,
            vz: 0.0,
            mass: SOLAR_MASS,
        },
        Body {
            x: 4.84143144246472090e00,
            y: -1.16032004402742839e00,
            z: -1.03622044471123109e-01,
            vx: 1.66007664274403694e-03 * DAYS_PER_YEAR,
            vy: 7.69901118419740425e-03 * DAYS_PER_YEAR,
            vz: -6.90460016972063023e-05 * DAYS_PER_YEAR,
            mass: 9.54791938424326609e-04 * SOLAR_MASS,
        },
        Body {
            x: 8.34336671824457987e00,
            y: 4.12479856412430479e00,
            z: -4.03523417114321381e-01,
            vx: -2.76742510726862411e-03 * DAYS_PER_YEAR,
            vy: 4.99852801234917238e-03 * DAYS_PER_YEAR,
            vz: 2.30417297573763929e-05 * DAYS_PER_YEAR,
            mass: 2.85885980666130812e-04 * SOLAR_MASS,
        },
        Body {
            x: 1.28943695621391310e01,
            y: -1.51111514016986312e01,
            z: -2.23307578892655734e-01,
            vx: 2.96460137564761618e-03 * DAYS_PER_YEAR,
            vy: 2.37847173959480950e-03 * DAYS_PER_YEAR,
            vz: -2.96589568540237556e-05 * DAYS_PER_YEAR,
            mass: 4.36624404335156298e-05 * SOLAR_MASS,
        },
        Body {
            x: 1.53796971148509165e01,
            y: -2.59193146099879641e01,
            z: 1.79258772950371181e-01,
            vx: 2.68067772490389322e-03 * DAYS_PER_YEAR,
            vy: 1.62824170038242295e-03 * DAYS_PER_YEAR,
            vz: -9.51592254519715870e-05 * DAYS_PER_YEAR,
            mass: 5.15138902046611451e-05 * SOLAR_MASS,
        },
    ]
}

fn offset_momentum(bodies: &mut [Body]) {
    let mut px = 0.0;
    let mut py = 0.0;
    let mut pz = 0.0;
    for b in bodies.iter() {
        px += b.vx * b.mass;
        py += b.vy * b.mass;
        pz += b.vz * b.mass;
    }
    bodies[0].vx = -px / SOLAR_MASS;
    bodies[0].vy = -py / SOLAR_MASS;
    bodies[0].vz = -pz / SOLAR_MASS;
}

fn advance(bodies: &mut [Body], dt: f64, steps: usize) {
    for _ in 0..steps {
        for i in 0..(bodies.len() - 1) {
            for j in (i + 1)..bodies.len() {
                let dx = bodies[i].x - bodies[j].x;
                let dy = bodies[i].y - bodies[j].y;
                let dz = bodies[i].z - bodies[j].z;
                let d2 = dx * dx + dy * dy + dz * dz;
                let d = d2.sqrt();
                let mag = dt / (d2 * d);
                let bim = bodies[i].mass * mag;
                let bjm = bodies[j].mass * mag;

                bodies[i].vx -= dx * bjm;
                bodies[i].vy -= dy * bjm;
                bodies[i].vz -= dz * bjm;
                bodies[j].vx += dx * bim;
                bodies[j].vy += dy * bim;
                bodies[j].vz += dz * bim;
            }
        }

        for b in bodies.iter_mut() {
            b.x += dt * b.vx;
            b.y += dt * b.vy;
            b.z += dt * b.vz;
        }
    }
}

fn energy(bodies: &[Body]) -> f64 {
    let mut e = 0.0;
    for i in 0..bodies.len() {
        let bi = bodies[i];
        e += 0.5 * bi.mass * (bi.vx * bi.vx + bi.vy * bi.vy + bi.vz * bi.vz);
        for j in (i + 1)..bodies.len() {
            let bj = bodies[j];
            let dx = bi.x - bj.x;
            let dy = bi.y - bj.y;
            let dz = bi.z - bj.z;
            e -= (bi.mass * bj.mass) / (dx * dx + dy * dy + dz * dz).sqrt();
        }
    }
    e
}

fn main() {
    let steps = env::args()
        .nth(1)
        .and_then(|s| s.parse::<usize>().ok())
        .unwrap_or(5000);

    let mut bodies = make_bodies();
    offset_momentum(&mut bodies);
    let before = energy(&bodies);
    advance(&mut bodies, 0.01, steps);
    let after = energy(&bodies);
    println!("{:.9}", before);
    println!("{:.9}", after);
}
