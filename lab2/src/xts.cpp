#include "xts.hpp"

#include <cstring>

#include "errors.hpp"

using namespace std;

namespace aesp {
namespace {

const uint8_t* check_xts_key(const uint8_t* key, std::size_t key_len) {
  if (key == nullptr || (key_len != 32 && key_len != 64))
    throw Error(ExitCode::InvalidInput, "invalid XTS key length: need 32 bytes (XTS-AES-128) or 64 bytes (XTS-AES-256)");
  return key;
}

inline void xor16(const uint8_t* a, const uint8_t* b, uint8_t* out) {
  for (int i = 0; i < 16; ++i) out[i] = static_cast<uint8_t>(a[i] ^ b[i]);
}

// C = E_K1(P xor T) xor T   (or the decrypt analogue)
inline void xts_block(const Aes& k1, bool encrypt, const uint8_t t[16], const uint8_t in[16], uint8_t out[16]) {
  uint8_t x[16];
  xor16(in, t, x);
  if (encrypt)
    k1.encrypt_block(x, x);
  else
    k1.decrypt_block(x, x);
  xor16(x, t, out);
  secure_zero(x, sizeof x);
}

}  // namespace

void xts_mul_alpha(uint8_t t[16]) {
  // 128-bit little-endian shift left by one; if a bit falls off the top, fold in x^7+x^2+x+1 = 0x87.
  const uint8_t carry = static_cast<uint8_t>(t[15] >> 7);
  for (int i = 15; i > 0; --i) t[i] = static_cast<uint8_t>((t[i] << 1) | (t[i - 1] >> 7));
  t[0] = static_cast<uint8_t>((t[0] << 1) ^ (0x87 & static_cast<uint8_t>(0 - carry)));  // branch-free
}

Xts::Xts(const uint8_t* key, std::size_t key_len, Backend backend, bool allow_equal_keys)
    : k1_(check_xts_key(key, key_len), key_len / 2, backend), k2_(key + key_len / 2, key_len / 2, backend) {
  if (!allow_equal_keys && std::memcmp(key, key + key_len / 2, key_len / 2) == 0)
    throw Error(ExitCode::InvalidInput, "XTS keys K1 and K2 must be different");
}

void Xts::encrypt(const uint8_t tweak[16], const uint8_t* in, uint8_t* out, std::size_t len) const {
  if (len < 16 || len > kXtsMaxDataUnit)
    throw Error(ExitCode::InvalidInput, "XTS data unit must be between 16 bytes and 16 MiB");
  const std::size_t full = len / 16, rem = len % 16;
  uint8_t t[16];
  k2_.encrypt_block(tweak, t);

  const std::size_t plain_blocks = rem == 0 ? full : full - 1;  // blocks handled without stealing
  for (std::size_t j = 0; j < plain_blocks; ++j) {
    xts_block(k1_, true, t, in + 16 * j, out + 16 * j);
    xts_mul_alpha(t);
  }
  if (rem != 0) {
    // Ciphertext stealing (IEEE 1619 §5.3.2). Copy the last full block + tail first (in may alias out).
    uint8_t last[32];
    std::memcpy(last, in + 16 * plain_blocks, 16 + rem);
    uint8_t cc[16], cp[16], cm1[16];
    xts_block(k1_, true, t, last, cc);                    // CC = enc(P_{m-1}, T_{m-1})
    std::memcpy(cp, last + 16, rem);                      // CP = P_m || tail of CC
    std::memcpy(cp + rem, cc + rem, 16 - rem);
    xts_mul_alpha(t);
    xts_block(k1_, true, t, cp, cm1);                     // C_{m-1} = enc(CP, T_m)
    std::memcpy(out + 16 * plain_blocks, cm1, 16);
    std::memcpy(out + 16 * plain_blocks + 16, cc, rem);   // C_m = first `rem` bytes of CC
    secure_zero(last, sizeof last);
    secure_zero(cc, sizeof cc);
    secure_zero(cp, sizeof cp);
  }
  secure_zero(t, sizeof t);
}

void Xts::decrypt(const uint8_t tweak[16], const uint8_t* in, uint8_t* out, std::size_t len) const {
  if (len < 16 || len > kXtsMaxDataUnit)
    throw Error(ExitCode::InvalidInput, "XTS data unit must be between 16 bytes and 16 MiB");
  const std::size_t full = len / 16, rem = len % 16;
  uint8_t t[16];
  k2_.encrypt_block(tweak, t);  // the tweak is always ENcrypted, also when decrypting

  const std::size_t plain_blocks = rem == 0 ? full : full - 1;
  for (std::size_t j = 0; j < plain_blocks; ++j) {
    xts_block(k1_, false, t, in + 16 * j, out + 16 * j);
    xts_mul_alpha(t);
  }
  if (rem != 0) {
    uint8_t last[32];
    std::memcpy(last, in + 16 * plain_blocks, 16 + rem);
    uint8_t t_m1[16], pp[16], cc[16], pm1[16];
    std::memcpy(t_m1, t, 16);
    xts_mul_alpha(t);                                     // t = T_m
    xts_block(k1_, false, t, last, pp);                   // PP = dec(C_{m-1}, T_m)
    std::memcpy(cc, last + 16, rem);                      // CC = C_m || tail of PP
    std::memcpy(cc + rem, pp + rem, 16 - rem);
    xts_block(k1_, false, t_m1, cc, pm1);                 // P_{m-1} = dec(CC, T_{m-1})
    std::memcpy(out + 16 * plain_blocks, pm1, 16);
    std::memcpy(out + 16 * plain_blocks + 16, pp, rem);   // P_m = first `rem` bytes of PP
    secure_zero(last, sizeof last);
    secure_zero(pp, sizeof pp);
    secure_zero(cc, sizeof cc);
    secure_zero(pm1, sizeof pm1);
  }
  secure_zero(t, sizeof t);
}

}  // namespace aesp
