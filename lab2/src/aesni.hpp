// BONUS: AES-NI backend (x86/x86-64 AESENC/AESDEC instructions).
//
// AES-NI is used ONLY here (the lab forbids hardware AES outside the bonus). The key schedule is
// still the from-scratch FIPS-197 KeyExpansion done by `Aes`; this module only executes the
// rounds, so the instructions are independent of the software S-box code and the two can be
// cross-checked against each other (see tests and KAT).
//
// Constant time: AESENC/AESDEC perform SubBytes in dedicated hardware with no data-dependent
// memory access or branch, so unlike the table/T-table backends they are not exposed to
// cache-timing attacks (Bernstein 2005, Osvik-Shamir-Tromer 2006).
#pragma once
#include <cstddef>
#include <cstdint>

namespace aesp {
namespace aesni {

// True only if this binary was built for x86/x86-64 AND the CPU reports the AES feature flag
// (CPUID.01H:ECX bit 25) at run time.
bool available();

// rk: FIPS-197 expanded key (16*(rounds+1) bytes). in/out may alias. Throws Error(Failure) if !available().
void encrypt_block(const uint8_t* rk, std::size_t rounds, const uint8_t in[16], uint8_t out[16]);

// Builds the "equivalent inverse cipher" keys (FIPS-197 §5.3.5) needed by AESDEC.
void make_decrypt_keys(const uint8_t* rk, std::size_t rounds, uint8_t* dk);
void decrypt_block(const uint8_t* dk, std::size_t rounds, const uint8_t in[16], uint8_t out[16]);

}  // namespace aesni
}  // namespace aesp
