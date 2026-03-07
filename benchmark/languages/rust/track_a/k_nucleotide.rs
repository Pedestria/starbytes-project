use std::collections::HashMap;
use std::env;
use std::fs;

fn normalize_sequence(path: &str) -> Result<String, std::io::Error> {
    let raw = fs::read_to_string(path)?;
    let mut out = String::with_capacity(raw.len());
    for line in raw.lines() {
        let trimmed = line.trim();
        if !trimmed.is_empty() && !trimmed.starts_with('>') {
            out.push_str(&trimmed.to_ascii_uppercase());
        }
    }
    Ok(out)
}

fn count_kmers(sequence: &str, k: usize) -> HashMap<String, usize> {
    let mut counts = HashMap::new();
    if sequence.len() < k {
        return counts;
    }
    for i in 0..=(sequence.len() - k) {
        let token = &sequence[i..(i + k)];
        *counts.entry(token.to_string()).or_insert(0) += 1;
    }
    counts
}

fn main() {
    let path = env::args()
        .nth(1)
        .unwrap_or_else(|| "benchmark/data/track_a/dna_input.fasta".to_string());
    let sequence = match normalize_sequence(&path) {
        Ok(value) => value,
        Err(err) => {
            eprintln!("{}", err);
            std::process::exit(1);
        }
    };

    let freq1 = count_kmers(&sequence, 1);
    let freq2 = count_kmers(&sequence, 2);
    let freq3 = count_kmers(&sequence, 3);
    let freq4 = count_kmers(&sequence, 4);
    let freq6 = count_kmers(&sequence, 6);
    let freq8 = count_kmers(&sequence, 8);

    println!("{}", sequence.len());
    println!("{}", freq1.len());
    println!("{}", freq2.len());
    println!("{}", freq3.len());
    println!("{}", freq4.len());
    println!("{}", freq6.len());
    println!("{}", freq8.len());
    println!("{}", freq3.get("GGT").copied().unwrap_or(0));
    println!("{}", freq4.get("GGTA").copied().unwrap_or(0));
    println!("{}", freq6.get("GGTATT").copied().unwrap_or(0));
    println!("{}", freq8.get("AGGGTAAA").copied().unwrap_or(0));
}
