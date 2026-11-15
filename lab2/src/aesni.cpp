#include "aesni.hpp"

#include "errors.hpp"

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#define AESP_X86 1
#include <immintrin.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#endif

#if defined(AESP_X86) && (defined(__GNUC__) || defined(__clang__))
// Enable the AES-NI instruction set for these functions only, so the rest of the program still
// runs on CPUs without it (no global -maes).
#define AESP_TARGET __attribute__((target("aes,sse2")))
#else
#define AESP_TARGET
#endif

namespace aesp {
namespace aesni {

#if defined(AESP_X86)

bool available() {
#if defined(_MSC_VER)
  int info[4] = {0, 0, 0, 0};
  __cpuid(info, 1);
  return (info[2] & (1 << 25)) != 0;
#else
  __builtin_cpu_init();
  return __builtin_cpu_supports("aes") != 0;
#endif
}

static bool cached() {  // CPU detection once, not on every block
  static const bool v = available();
  return v;
}

AESP_TARGET void encrypt_block(const uint8_t* rk, std::size_t rounds, const uint8_t in[16], uint8_t out[16]) {
  if (!cached()) throw Error(ExitCode::Failure, "AES-NI is not available on this CPU");
  __m128i s = _mm_xor_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(in)),
                            _mm_loadu_si128(reinterpret_cast<const __m128i*>(rk)));
  for (std::size_t r = 1; r < rounds; ++r)
    s = _mm_aesenc_si128(s, _mm_loadu_si128(reinterpret_cast<const __m128i*>(rk + 16 * r)));
  s = _mm_aesenclast_si128(s, _mm_loadu_si128(reinterpret_cast<const __m128i*>(rk + 16 * rounds)));
  _mm_storeu_si128(reinterpret_cast<__m128i*>(out), s);
}

AESP_TARGET void make_decrypt_keys(const uint8_t* rk, std::size_t rounds, uint8_t* dk) {
  if (!cached()) throw Error(ExitCode::Failure, "AES-NI is not available on this CPU");
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dk),
                   _mm_loadu_si128(reinterpret_cast<const __m128i*>(rk + 16 * rounds)));
  for (std::size_t i = 1; i < rounds; ++i)
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dk + 16 * i),
                     _mm_aesimc_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(rk + 16 * (rounds - i)))));
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dk + 16 * rounds),
                   _mm_loadu_si128(reinterpret_cast<const __m128i*>(rk)));
}

AESP_TARGET void decrypt_block(const uint8_t* dk, std::size_t rounds, const uint8_t in[16], uint8_t out[16]) {
  if (!cached()) throw Error(ExitCode::Failure, "AES-NI is not available on this CPU");
  __m128i s = _mm_xor_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(in)),
                            _mm_loadu_si128(reinterpret_cast<const __m128i*>(dk)));
  for (std::size_t r = 1; r < rounds; ++r)
    s = _mm_aesdec_si128(s, _mm_loadu_si128(reinterpret_cast<const __m128i*>(dk + 16 * r)));
  s = _mm_aesdeclast_si128(s, _mm_loadu_si128(reinterpret_cast<const __m128i*>(dk + 16 * rounds)));
  _mm_storeu_si128(reinterpret_cast<__m128i*>(out), s);
}

#else  // not an x86 target: the backend is simply unavailable

bool available() { return false; }
void encrypt_block(const uint8_t*, std::size_t, const uint8_t*, uint8_t*) {
  throw Error(ExitCode::Failure, "AES-NI is not available on this platform");
}
void make_decrypt_keys(const uint8_t*, std::size_t, uint8_t*) {
  throw Error(ExitCode::Failure, "AES-NI is not available on this platform");
}
void decrypt_block(const uint8_t*, std::size_t, const uint8_t*, uint8_t*) {
  throw Error(ExitCode::Failure, "AES-NI is not available on this platform");
}

#endif

}  // namespace aesni
}  // namespace aesp
