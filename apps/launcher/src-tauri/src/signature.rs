// Release manifests are signed by CI with Ed25519 (.github/workflows/release.yml);
// the game trusts the same public key (release/release.cpp: UPDATE_KEY).

use base64::Engine;
use ed25519_dalek::{Signature, Verifier, VerifyingKey};

/// The key the game checks updates against: one file, read by both.
pub const UPDATE_KEY: &str = include_str!("../../../../scripts/release/update-key.pub");

/// An Ed25519 public key in PEM: the last 32 bytes of its SubjectPublicKeyInfo.
fn key(pem: &str) -> Option<VerifyingKey> {
    let body: String = pem.lines().filter(|l| !l.starts_with("-----")).collect();
    let der = base64::engine::general_purpose::STANDARD.decode(body.trim()).ok()?;
    // 12 bytes of algorithm header (1.3.101.112), then the key
    if der.len() != 44 {
        return None;
    }
    VerifyingKey::from_bytes(der[12..].try_into().ok()?).ok()
}

/// Does this raw signature (openssl pkeyutl -out) cover exactly these bytes?
pub fn verify(bytes: &[u8], signature: &[u8], pem: &str) -> bool {
    let (Some(key), Ok(signature)) = (key(pem), Signature::from_slice(signature)) else {
        return false;
    };
    key.verify(bytes, &signature).is_ok()
}

#[cfg(test)]
mod tests {
    use super::*;

    // The game's test vector (unittest/test_release.cpp): a throwaway key and one signature made with it.
    const KEY: &str = "-----BEGIN PUBLIC KEY-----\nMCowBQYDK2VwAyEAVhPpcp2Lnp+8HxPU66ZtJL870nhPCl5XWcWwDwdq66g=\n-----END PUBLIC KEY-----\n";
    const MANIFEST: &[u8] = b"{\"version\":\"1.2.3\"}\n";
    const SIGNATURE: &str = "hixukWzthgzTjltqRPO77V4r/IVUOqobvdT+3sTS3KDUtqFM1IDgvp/vboRJv6kPdXyM1o6Ve/55JZKxupejAw==";

    fn raw() -> Vec<u8> {
        base64::engine::general_purpose::STANDARD.decode(SIGNATURE).unwrap()
    }

    #[test]
    fn verifies_what_ci_signs() {
        assert!(verify(MANIFEST, &raw(), KEY));
        assert!(!verify(b"{\"version\":\"6.6.6\"}\n", &raw(), KEY));
        assert!(!verify(MANIFEST, &raw(), UPDATE_KEY));
        assert!(!verify(MANIFEST, b"short", KEY));
        assert!(!verify(MANIFEST, &raw(), "not a key"));
    }

    #[test]
    fn the_update_key_reads() {
        assert!(key(UPDATE_KEY).is_some());
    }
}
