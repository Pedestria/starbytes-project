use std::env;

const SOURCE: &[u8] = b"GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGG";
const BASES: [u8; 5] = [b'A', b'C', b'G', b'T', b'N'];
const WEIGHTS: [usize; 5] = [270, 120, 120, 270, 220];

fn count_repeat(length: usize) -> [usize; 5] {
    let mut counts = [0usize; 5];
    for i in 0..length {
        match SOURCE[i % SOURCE.len()] {
            b'A' => counts[0] += 1,
            b'C' => counts[1] += 1,
            b'G' => counts[2] += 1,
            b'T' => counts[3] += 1,
            _ => counts[4] += 1,
        }
    }
    counts
}

fn choose_base(value: usize) -> u8 {
    let mut total = 0usize;
    for (index, weight) in WEIGHTS.iter().enumerate() {
        total += weight;
        if value < total {
            return BASES[index];
        }
    }
    b'N'
}

fn count_random(length: usize, seed: usize) -> [usize; 5] {
    let mut counts = [0usize; 5];
    let mut state = seed;
    for _ in 0..length {
        state = (state * 3877 + 29573) % 139968;
        match choose_base(state % 1000) {
            b'A' => counts[0] += 1,
            b'C' => counts[1] += 1,
            b'G' => counts[2] += 1,
            b'T' => counts[3] += 1,
            _ => counts[4] += 1,
        }
    }
    counts
}

fn main() {
    let repeat_length = env::args()
        .nth(1)
        .and_then(|s| s.parse::<usize>().ok())
        .unwrap_or(4000);
    let random_length = env::args()
        .nth(2)
        .and_then(|s| s.parse::<usize>().ok())
        .unwrap_or(4000);
    let seed = env::args()
        .nth(3)
        .and_then(|s| s.parse::<usize>().ok())
        .unwrap_or(42);

    let repeat_counts = count_repeat(repeat_length);
    let random_counts = count_random(random_length, seed);

    println!("{}", repeat_length);
    println!("{}", random_length);
    println!("{}", repeat_counts[0]);
    println!("{}", repeat_counts[1]);
    println!("{}", repeat_counts[2]);
    println!("{}", repeat_counts[3]);
    println!("{}", random_counts[0]);
    println!("{}", random_counts[1]);
    println!("{}", random_counts[2]);
    println!("{}", random_counts[3]);
    println!("{}", random_counts[4]);
}
