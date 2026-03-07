use std::env;
use std::fs;

#[derive(Clone)]
enum Token {
    Literal(u8),
    Class(Vec<u8>),
}

fn normalize_sequence(path: &str) -> Result<(String, String), std::io::Error> {
    let raw = fs::read_to_string(path)?;
    let mut out = String::with_capacity(raw.len());
    for line in raw.lines() {
        let trimmed = line.trim();
        if !trimmed.is_empty() && !trimmed.starts_with('>') {
            out.push_str(&trimmed.to_ascii_uppercase());
        }
    }
    Ok((raw, out))
}

fn parse_variant(pattern: &str) -> Vec<Token> {
    let bytes = pattern.as_bytes();
    let mut out = Vec::new();
    let mut index = 0usize;
    while index < bytes.len() {
        if bytes[index] == b'[' {
            index += 1;
            let mut set = Vec::new();
            while index < bytes.len() && bytes[index] != b']' {
                set.push(bytes[index]);
                index += 1;
            }
            out.push(Token::Class(set));
            if index < bytes.len() && bytes[index] == b']' {
                index += 1;
            }
            continue;
        }
        out.push(Token::Literal(bytes[index]));
        index += 1;
    }
    out
}

fn parse_pattern(pattern: &str) -> Vec<Vec<Token>> {
    pattern.split('|').map(parse_variant).collect()
}

fn matches_variant(haystack: &[u8], start: usize, variant: &[Token]) -> bool {
    if start + variant.len() > haystack.len() {
        return false;
    }
    for (offset, token) in variant.iter().enumerate() {
        let value = haystack[start + offset];
        match token {
            Token::Literal(expected) => {
                if value != *expected {
                    return false;
                }
            }
            Token::Class(set) => {
                if !set.contains(&value) {
                    return false;
                }
            }
        }
    }
    true
}

fn count_pattern(sequence: &str, pattern: &str) -> usize {
    let variants = parse_pattern(pattern);
    let bytes = sequence.as_bytes();
    let mut count = 0usize;
    for index in 0..bytes.len() {
        if variants.iter().any(|variant| matches_variant(bytes, index, variant)) {
            count += 1;
        }
    }
    count
}

fn main() {
    let path = env::args()
        .nth(1)
        .unwrap_or_else(|| "benchmark/data/track_a/dna_input.fasta".to_string());
    let (raw, cleaned) = match normalize_sequence(&path) {
        Ok(value) => value,
        Err(err) => {
            eprintln!("{}", err);
            std::process::exit(1);
        }
    };

    let patterns = [
        "AGGGTAAA|TTTACCCT",
        "[CGT]GGGTAAA|TTTACCC[ACG]",
        "A[ACT]GGTAAA|TTTACC[AGT]T",
        "AG[ACT]GTAAA|TTTAC[AGT]CT",
        "AGG[ACT]TAAA|TTTA[AGT]CCT",
    ];

    println!("{}", raw.len());
    println!("{}", cleaned.len());
    for pattern in patterns {
        println!("{}", count_pattern(&cleaned, pattern));
    }

    let replaced = cleaned
        .replace("AGGGTAAA", "<A>")
        .replace("TTTACCCT", "<B>")
        .replace("CGGGTAAA", "<C>");
    println!("{}", replaced.len());
}
