// BONUS: XTS-AES (IEEE Std 1619-2007, also NIST SP 800-38E) built on the from-scratch Aes.
//
//   * Two independent keys: K1 encrypts the data blocks, K2 encrypts the tweak (the "sector number").
//     A key argument of 32 bytes = XTS-AES-128 (2 x 16), 64 bytes = XTS-AES-256 (2 x 32).
//   * Tweak: 16 bytes (the data-unit number). T_0 = AES_K2(tweak); T_{j+1} = T_j * alpha in GF(2^128)
//     with the polynomial x^128 + x^7 + x^2 + x + 1; bytes are little-endian (byte 0 = least
//     significant), exactly as the standard specifies.
//   * Block i:  C_i = AES_K1(P_i xor T_i) xor T_i.  The data unit may have any length >= 16 bytes;
//     a trailing partial block uses ciphertext stealing (no padding, ciphertext length == plaintext length).
//   * Limit: one data unit holds at most 2^20 blocks (16 MiB) -- longer inputs are rejected (fail closed).
//
// SECURITY: XTS gives confidentiality for storage with no integrity. An attacker who can write the
// disk can scramble a 16-byte block unpredictably or swap/replay whole sectors; identical plaintext
// at the same sector position under the same key gives identical ciphertext (no randomisation).
// Never use XTS for network messages; use an AEAD instead.
#pragma once
#include <cstddef>
#include <cstdint>

#include "aes128.hpp"
#include "util.hpp"

using namespace std;

namespace aesp {

constexpr std::size_t kXtsMaxDataUnit = static_cast<std::size_t>(1) << 24;  // 2^20 blocks * 16 bytes

class Xts {
 public:
  // key_len must be 32 (XTS-AES-128) or 64 (XTS-AES-256) -- else Error(InvalidInput). K1 and K2 must
  // differ (IEEE 1619-2018 / SP 800-38E requirement) -- else Error(InvalidInput), unless
  // allow_equal_keys is set (only for the IEEE 1619-2007 test vector 1, which uses K1 == K2).
  // The AES backend applies to both keys; AES-NI is allowed.
  Xts(const uint8_t* key, std::size_t key_len, Backend backend = Backend::SBoxTable, bool allow_equal_keys = false);

  // Encrypts/decrypts one data unit. len must be in [16, kXtsMaxDataUnit]. in == out is allowed.
  void encrypt(const uint8_t tweak[16], const uint8_t* in, uint8_t* out, std::size_t len) const;
  void decrypt(const uint8_t tweak[16], const uint8_t* in, uint8_t* out, std::size_t len) const;

 private:
  Aes k1_, k2_;
};

// Multiplies the 128-bit little-endian value t by alpha (x) in GF(2^128); exposed for tests.
void xts_mul_alpha(uint8_t t[16]);

}  // namespace aesp
