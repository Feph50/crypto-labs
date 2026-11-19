// Strict hex / base64 codecs and the on-disk ciphertext encodings (raw | hex | base64).
// Decoders never throw: they return nullopt on ANY malformed input (fail closed); callers decide which error to raise.
#pragma once
#include <optional>
#include <string>
#include <string_view>

#include "util.hpp"

namespace rsalab {

enum class Encoding { Raw, Hex, Base64 };

Encoding parse_encoding(const std::string& name);  // throws Usage error for an unknown name

std::string hex_encode(const uint8_t* d, std::size_t n);
inline std::string hex_encode(const Bytes& b) { return hex_encode(b.data(), b.size()); }
// ASCII whitespace is ignored; odd length or any other character -> nullopt.
std::optional<Bytes> hex_decode(std::string_view s);

std::string base64_encode(const uint8_t* d, std::size_t n);  // standard alphabet, '=' padding, no line breaks
inline std::string base64_encode(const Bytes& b) { return base64_encode(b.data(), b.size()); }
// ASCII whitespace is ignored; padding is required, non-canonical trailing bits and stray characters -> nullopt.
std::optional<Bytes> base64_decode(std::string_view s);

// Ciphertext file bytes: raw = as is; hex/base64 = text followed by a newline.
Bytes encode_for_file(const Bytes& raw, Encoding e);
std::optional<Bytes> decode_from_file(const Bytes& file_bytes, Encoding e);

}  // namespace rsalab
