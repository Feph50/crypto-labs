// Container / envelope format shared by both modes, so that `decrypt` recognises the mode without guessing:
//
//   "RSAE" | version=0x01 | header_len (u32, big-endian) | header (UTF-8 JSON, <= 64 KiB) | payload
//
//   Direct : header {"mode":"RSA-OAEP","rsa_modulus":3072,"hash":"SHA-256"}                       payload = RSA-OAEP ciphertext (k bytes)
//   Hybrid : header {"mode":"RSA-OAEP-AES-GCM","rsa_modulus":3072,"hash":"SHA-256",
//                    "wrapped_key":"<b64>","iv":"<b64>","tag":"<b64>"}                              payload = AES-256-GCM ciphertext
//
// The header is accepted only in this exact canonical form (key order, no spaces, no extra fields): a parsed header is
// re-serialised and compared byte for byte. In hybrid mode the header fields (except the tag) are also the AES-GCM AAD,
// so any edit of the header makes the authentication fail even before the RSA key is used.
#pragma once
#include <string>

#include "util.hpp"

namespace rsalab {

enum class Mode { Direct, Hybrid };

inline constexpr const char* kModeDirect = "RSA-OAEP";
inline constexpr const char* kModeHybrid = "RSA-OAEP-AES-GCM";
inline constexpr const char* kHashName = "SHA-256";
inline constexpr std::size_t kMaxHeaderLen = 65536;
inline constexpr std::size_t kGcmIvLen = 12;
inline constexpr std::size_t kGcmTagLen = 16;
inline constexpr std::size_t kAesKeyLen = 32;

struct Header {
  Mode mode = Mode::Direct;
  unsigned rsa_modulus = 0;
  Bytes wrapped_key, iv, tag;  // hybrid only
};

struct Parsed {
  Header header;
  Bytes payload;
};

std::string header_json(const Header& h);  // canonical text
// "RSAE-AAD-v1|" + mode + "|" + rsa_modulus + "|" + hash + "|" + wrapped_key_b64 + "|" + iv_b64   (tag is not included)
Bytes header_aad(const Header& h);

Bytes build_container(const Header& h, const Bytes& payload);
// Strict. Throws Error(Crypto) with the generic message on ANY malformed input.
Parsed parse_container(const Bytes& data);

}  // namespace rsalab
