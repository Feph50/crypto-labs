// AES block cipher, implemented from scratch per FIPS-197 (no external crypto library).
//
// Interchangeable SubBytes/round backends are provided so the lab report can compare them
// (table vs computed S-box, memory footprint, cache behaviour, hardware acceleration):
//
//   SBoxTable    byte-oriented reference implementation, 256-byte S-box lookup (FIPS-197 style)
//   SBoxComputed no lookup tables: S(x) = affine(x^254) using branch-free GF(2^8) arithmetic
//                (no secret-dependent memory index or branch; ~100x slower)
//   TTable       4 x 1 KiB T-tables, 32-bit words (fast; classic cache-timing target)
//   AesNi        BONUS: x86 AES-NI instructions (runtime CPU detection; constant-time)
//
// The key schedule is always FIPS-197 §5.2 (KeyExpansion), using the backend's S-box.
//
// Classes:
//   Aes     BONUS: AES-128 / AES-192 / AES-256 (key of 16 / 24 / 32 bytes selects Nk = 4/6/8, Nr = 10/12/14)
//   Aes128  the lab's core cipher: an Aes that accepts ONLY 16-byte keys (fail closed otherwise)
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

using namespace std;

namespace aesp {

enum class Backend { SBoxTable, SBoxComputed, TTable, AesNi };

const char* backend_name(Backend b);
// Accepts: table | sbox-table | computed | sbox-computed | ttable | aesni   (throws Error on others)
Backend parse_backend(const std::string& s);

uint8_t sbox_table(uint8_t x);         // FIPS-197 Figure 7 (table lookup)
uint8_t sbox_computed(uint8_t x);      // same function, computed without tables
uint8_t inv_sbox_computed(uint8_t x);  // inverse S-box, computed without tables

class Aes {
 public:
  static constexpr std::size_t kBlockSize = 16;
  static constexpr std::size_t kMaxRounds = 14;
  static constexpr std::size_t kMaxRoundKeyBytes = kBlockSize * (kMaxRounds + 1);  // 240

  // Throws Error(InvalidInput) unless key_len is 16, 24 or 32; Error(InvalidInput) if the AesNi
  // backend is requested but this CPU/build has no AES-NI.
  Aes(const uint8_t* key, std::size_t key_len, Backend backend = Backend::SBoxTable);
  Aes(const Aes&) = default;
  Aes& operator=(const Aes&) = default;
  ~Aes();  // wipes the round keys

  // Cipher / InvCipher of FIPS-197 §5.1 / §5.3. `in` and `out` may alias.
  void encrypt_block(const uint8_t in[16], uint8_t out[16]) const;
  void decrypt_block(const uint8_t in[16], uint8_t out[16]) const;

  Backend backend() const noexcept { return backend_; }
  std::size_t key_size() const noexcept { return key_len_; }
  std::size_t rounds() const noexcept { return rounds_; }
  const std::array<uint8_t, kMaxRoundKeyBytes>& round_keys() const noexcept { return rk_; }  // 16*(rounds+1) used

 private:
  std::array<uint8_t, kMaxRoundKeyBytes> rk_{};
  std::array<uint32_t, 4 * (kMaxRounds + 1)> rkw_{};  // big-endian words of rk_ (for the T-table backend)
  std::array<uint8_t, kMaxRoundKeyBytes> dk_{};       // equivalent-inverse-cipher keys (AES-NI decrypt only)
  std::size_t key_len_ = 0, rounds_ = 0;
  Backend backend_;
};

class Aes128 : public Aes {
 public:
  static constexpr std::size_t kKeySize = 16;
  static constexpr std::size_t kRounds = 10;
  static constexpr std::size_t kRoundKeyBytes = kBlockSize * (kRounds + 1);  // 176

  // Throws Error(InvalidInput) unless key_len == 16.
  Aes128(const uint8_t* key, std::size_t key_len, Backend backend = Backend::SBoxTable);
};

}  // namespace aesp
