// RSA-OAEP with SHA-256 and MGF1-SHA-256 (RFC 8017 RSAES-OAEP) on top of Crypto++.
// CryptoPP::OAEP<SHA256> uses the SAME hash (SHA-256) for the label hash and for MGF1, which is exactly what the lab requires.
#pragma once
#include "cryptopp_all.hpp"
#include "keyio.hpp"
#include "util.hpp"

namespace rsalab {

constexpr std::size_t kHashLen = 32;  // SHA-256

// mLen <= k - 2*hLen - 2  (k = modulus size in bytes): 318 bytes for RSA-3072, 446 bytes for RSA-4096.
std::size_t max_plaintext_len(unsigned modulus_bits);

// Throws Usage error when msg is longer than max_plaintext_len. `label` may be empty (= the empty OAEP label L).
Bytes oaep_encrypt(const PublicKey& pub, const uint8_t* msg, std::size_t n, const Bytes& label, CryptoPP::RandomNumberGenerator& rng);

// Throws Error(Crypto) with the generic message for EVERY failure (wrong length, bad padding, wrong label, wrong key ...).
Bytes oaep_decrypt(const PrivateKey& priv, const uint8_t* ct, std::size_t n, const Bytes& label, CryptoPP::RandomNumberGenerator& rng);

}  // namespace rsalab
