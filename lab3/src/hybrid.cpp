#include "hybrid.hpp"

#include "errors.hpp"

namespace rsalab {

static const uint8_t kEmpty[1] = {0};
static const uint8_t* nz(const uint8_t* p) { return p ? p : kEmpty; }  // Crypto++ is happier with non-null pointers

void aes256_gcm_encrypt(const uint8_t key[32], const uint8_t iv[12], const uint8_t* aad, std::size_t aad_len, const uint8_t* msg,
                        std::size_t n, uint8_t* ct, uint8_t tag[16]) {
  CryptoPP::GCM<CryptoPP::AES>::Encryption enc;
  enc.SetKeyWithIV(key, 32, iv, kGcmIvLen);
  uint8_t dummy[1];
  enc.EncryptAndAuthenticate(ct ? ct : dummy, tag, kGcmTagLen, iv, static_cast<int>(kGcmIvLen), nz(aad), aad_len, nz(msg), n);
}

bool aes256_gcm_decrypt(const uint8_t key[32], const uint8_t iv[12], const uint8_t* aad, std::size_t aad_len, const uint8_t* ct,
                        std::size_t n, const uint8_t tag[16], uint8_t* pt) {
  CryptoPP::GCM<CryptoPP::AES>::Decryption dec;
  dec.SetKeyWithIV(key, 32, iv, kGcmIvLen);
  uint8_t dummy[1];
  return dec.DecryptAndVerify(pt ? pt : dummy, tag, kGcmTagLen, iv, static_cast<int>(kGcmIvLen), nz(aad), aad_len, nz(ct), n);
}

EncryptResult encrypt_message(const PublicKey& pub, const uint8_t* msg, std::size_t n, const Bytes& label, bool force_hybrid,
                              CryptoPP::RandomNumberGenerator& rng) {
  const unsigned bits = modulus_bits(pub);
  if (!is_supported_modulus_bits(bits)) usage_error("unsupported key size (use 3072 or 4096)");
  Header h;
  h.rsa_modulus = bits;

  if (!force_hybrid && n <= max_plaintext_len(bits)) {
    h.mode = Mode::Direct;
    return {build_container(h, oaep_encrypt(pub, msg, n, label, rng)), Mode::Direct};
  }

  h.mode = Mode::Hybrid;
  CryptoPP::SecByteBlock key(kAesKeyLen);
  rng.GenerateBlock(key, key.size());
  h.iv.resize(kGcmIvLen);
  rng.GenerateBlock(h.iv.data(), h.iv.size());
  h.wrapped_key = oaep_encrypt(pub, key, key.size(), label, rng);
  h.tag.assign(kGcmTagLen, 0);  // not part of the AAD
  const Bytes aad = header_aad(h);
  Bytes ct(n);
  aes256_gcm_encrypt(key, h.iv.data(), aad.data(), aad.size(), msg, n, ct.data(), h.tag.data());
  return {build_container(h, ct), Mode::Hybrid};
}

Bytes decrypt_message(const PrivateKey& priv, const Bytes& container, const Bytes& label) {
  const unsigned bits = modulus_bits(priv);
  if (!is_supported_modulus_bits(bits)) crypto_failure();
  const Parsed p = parse_container(container);
  if (p.header.rsa_modulus != bits) crypto_failure();
  CryptoPP::AutoSeededRandomPool rng;

  if (p.header.mode == Mode::Direct) return oaep_decrypt(priv, p.payload.data(), p.payload.size(), label, rng);

  // Hybrid: unwrap the AES key, then authenticated decryption into memory; the caller gets the plaintext only on success.
  Bytes key = oaep_decrypt(priv, p.header.wrapped_key.data(), p.header.wrapped_key.size(), label, rng);
  WipeOnExit wipe_key(key);
  if (key.size() != kAesKeyLen) crypto_failure();
  const Bytes aad = header_aad(p.header);
  Bytes plain(p.payload.size());
  if (!aes256_gcm_decrypt(key.data(), p.header.iv.data(), aad.data(), aad.size(), p.payload.data(), p.payload.size(),
                          p.header.tag.data(), plain.data())) {
    secure_zero(plain);
    crypto_failure();
  }
  return plain;
}

}  // namespace rsalab
