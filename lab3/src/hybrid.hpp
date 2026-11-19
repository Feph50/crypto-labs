// Message-level API: automatic direct / hybrid (envelope) encryption and the matching decryption.
#pragma once
#include "cryptopp_all.hpp"
#include "envelope.hpp"
#include "keyio.hpp"
#include "rsa_oaep.hpp"
#include "util.hpp"

namespace rsalab {

struct EncryptResult {
  Bytes container;
  Mode mode;
};

// msg <= max_plaintext_len(bits): Direct RSA-OAEP. Longer (or `force_hybrid`): Hybrid = random AES-256 key, AES-256-GCM
// (96-bit IV, 128-bit tag) over the data, key wrapped with RSA-OAEP. `label` is the optional OAEP label.
EncryptResult encrypt_message(const PublicKey& pub, const uint8_t* msg, std::size_t n, const Bytes& label, bool force_hybrid,
                              CryptoPP::RandomNumberGenerator& rng);

// Fail closed: the plaintext is released only after everything verified. Every failure throws Error(Crypto) with the
// same generic message.
Bytes decrypt_message(const PrivateKey& priv, const Bytes& container, const Bytes& label);

// AES-256-GCM building blocks (also used by the KAT runner and the benchmark). `ct` and `pt` must hold n bytes, `tag` 16 bytes.
void aes256_gcm_encrypt(const uint8_t key[32], const uint8_t iv[12], const uint8_t* aad, std::size_t aad_len, const uint8_t* msg,
                        std::size_t n, uint8_t* ct, uint8_t tag[16]);
// Returns false when the authentication tag does not verify (the tag comparison is done by Crypto++, in constant time).
bool aes256_gcm_decrypt(const uint8_t key[32], const uint8_t iv[12], const uint8_t* aad, std::size_t aad_len, const uint8_t* ct,
                        std::size_t n, const uint8_t tag[16], uint8_t* pt);

}  // namespace rsalab
