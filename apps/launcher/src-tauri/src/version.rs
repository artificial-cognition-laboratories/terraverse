// Release versions in order: "0.3.0-beta.2" < "0.3.0" < "0.3.1". Numbers
// compare as numbers, and a pre-release comes before its release.

use std::cmp::Ordering;

fn parts(text: &str) -> Vec<Part> {
    text.split(['.', '-'])
        .map(|p| p.parse::<u64>().map_or_else(|_| Part::Word(p.to_owned()), Part::Number))
        .collect()
}

#[derive(PartialEq, Eq, PartialOrd, Ord)]
enum Part {
    Number(u64), // a number sorts before a word, as semver has it
    Word(String),
}

pub fn compare(a: &str, b: &str) -> Ordering {
    let (a, b) = (a.trim_start_matches('v'), b.trim_start_matches('v'));
    let (a_core, a_pre) = a.split_once('-').map_or((a, None), |(c, p)| (c, Some(p)));
    let (b_core, b_pre) = b.split_once('-').map_or((b, None), |(c, p)| (c, Some(p)));
    parts(a_core).cmp(&parts(b_core)).then_with(|| match (a_pre, b_pre) {
        (None, None) => Ordering::Equal,
        (None, Some(_)) => Ordering::Greater,
        (Some(_), None) => Ordering::Less,
        (Some(a), Some(b)) => parts(a).cmp(&parts(b)),
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn orders_releases() {
        assert_eq!(compare("0.1.10", "0.1.9"), Ordering::Greater);
        assert_eq!(compare("v0.1.1", "0.1.1"), Ordering::Equal);
        assert_eq!(compare("0.3.0-beta.2", "0.3.0"), Ordering::Less);
        assert_eq!(compare("0.3.0-beta.10", "0.3.0-beta.2"), Ordering::Greater);
        assert_eq!(compare("0.3.0-beta.1", "0.2.9"), Ordering::Greater);
    }
}
