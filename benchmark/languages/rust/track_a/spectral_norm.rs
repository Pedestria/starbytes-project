use std::env;

fn a(i: usize, j: usize) -> f64 {
    let ij = i + j;
    1.0 / (((ij * (ij + 1) / 2) + i + 1) as f64)
}

fn multiply_av(v: &[f64]) -> Vec<f64> {
    let n = v.len();
    let mut out = vec![0.0; n];
    for i in 0..n {
        let mut s = 0.0;
        for j in 0..n {
            s += a(i, j) * v[j];
        }
        out[i] = s;
    }
    out
}

fn multiply_atv(v: &[f64]) -> Vec<f64> {
    let n = v.len();
    let mut out = vec![0.0; n];
    for i in 0..n {
        let mut s = 0.0;
        for j in 0..n {
            s += a(j, i) * v[j];
        }
        out[i] = s;
    }
    out
}

fn multiply_at_av(v: &[f64]) -> Vec<f64> {
    let av = multiply_av(v);
    multiply_atv(&av)
}

fn run(n: usize, iterations: usize) -> f64 {
    let mut u = vec![1.0; n];
    let mut v = vec![0.0; n];

    for _ in 0..iterations {
        v = multiply_at_av(&u);
        u = multiply_at_av(&v);
    }

    let mut v_bv = 0.0;
    let mut vv = 0.0;
    for i in 0..n {
        v_bv += u[i] * v[i];
        vv += v[i] * v[i];
    }

    (v_bv / vv).sqrt()
}

fn main() {
    let n = env::args()
        .nth(1)
        .and_then(|s| s.parse::<usize>().ok())
        .unwrap_or(200);
    let iterations = env::args()
        .nth(2)
        .and_then(|s| s.parse::<usize>().ok())
        .unwrap_or(10);

    println!("{:.9}", run(n, iterations));
}
