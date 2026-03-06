use std::env;

struct Node {
    item: i32,
    left: Option<Box<Node>>,
    right: Option<Box<Node>>,
}

fn make_tree(item: i32, depth: i32) -> Node {
    if depth == 0 {
        return Node {
            item,
            left: None,
            right: None,
        };
    }
    let child_depth = depth - 1;
    Node {
        item,
        left: Some(Box::new(make_tree(item * 2 - 1, child_depth))),
        right: Some(Box::new(make_tree(item * 2, child_depth))),
    }
}

fn check_tree(node: &Node) -> i32 {
    let left = node.left.as_ref().map_or(0, |n| check_tree(n));
    let right = node.right.as_ref().map_or(0, |n| check_tree(n));
    node.item + left - right
}

fn run(max_depth: i32) -> i32 {
    let min_depth = 4;
    let stretch_depth = max_depth + 1;

    let mut checksum = check_tree(&make_tree(0, stretch_depth));
    let long_lived_tree = make_tree(0, max_depth);

    let mut depth = min_depth;
    while depth <= max_depth {
        let iterations = 1 << (max_depth - depth + min_depth);
        let mut i = 1;
        while i <= iterations {
            checksum += check_tree(&make_tree(i, depth));
            checksum += check_tree(&make_tree(-i, depth));
            i += 1;
        }
        depth += 2;
    }

    checksum + check_tree(&long_lived_tree)
}

fn main() {
    let max_depth = env::args()
        .nth(1)
        .and_then(|s| s.parse::<i32>().ok())
        .unwrap_or(12);
    println!("{}", run(max_depth));
}
