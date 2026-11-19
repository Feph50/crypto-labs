#include "encoding.hpp"

#include <cctype>

#include "errors.hpp"

namespace rsalab {

Encoding parse_encoding(const std::string& name) {
  if (name == "raw") return Encoding::Raw;
  if (name == "hex") return Encoding::Hex;
  if (name == "base64") return Encoding::Base64;
  usage_error("unknown encoding '" + name + "' (use raw, hex or base64)");
}

static bool is_ws(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

std::string hex_encode(const uint8_t* d, std::size_t n) {
  static const char* kHex = "0123456789abcdef";
  std::string s;
  s.reserve(n * 2);
  for (std::size_t i = 0; i < n; ++i) {
    s.push_back(kHex[d[i] >> 4]);
    s.push_back(kHex[d[i] & 0xF]);
  }
  return s;
}

static int hex_val(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

std::optional<Bytes> hex_decode(std::string_view s) {
  std::string clean;
  clean.reserve(s.size());
  for (char c : s)
    if (!is_ws(c)) clean.push_back(c);
  if (clean.size() % 2 != 0) return std::nullopt;
  Bytes out(clean.size() / 2);
  for (std::size_t i = 0; i < out.size(); ++i) {
    const int hi = hex_val(clean[2 * i]), lo = hex_val(clean[2 * i + 1]);
    if (hi < 0 || lo < 0) return std::nullopt;
    out[i] = static_cast<uint8_t>((hi << 4) | lo);
  }
  return out;
}

static const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode(const uint8_t* d, std::size_t n) {
  std::string s;
  s.reserve((n + 2) / 3 * 4);
  std::size_t i = 0;
  for (; i + 2 < n; i += 3) {
    const uint32_t v = (static_cast<uint32_t>(d[i]) << 16) | (static_cast<uint32_t>(d[i + 1]) << 8) | d[i + 2];
    s.push_back(kB64[(v >> 18) & 63]);
    s.push_back(kB64[(v >> 12) & 63]);
    s.push_back(kB64[(v >> 6) & 63]);
    s.push_back(kB64[v & 63]);
  }
  if (i + 1 == n) {
    const uint32_t v = static_cast<uint32_t>(d[i]) << 16;
    s.push_back(kB64[(v >> 18) & 63]);
    s.push_back(kB64[(v >> 12) & 63]);
    s += "==";
  } else if (i + 2 == n) {
    const uint32_t v = (static_cast<uint32_t>(d[i]) << 16) | (static_cast<uint32_t>(d[i + 1]) << 8);
    s.push_back(kB64[(v >> 18) & 63]);
    s.push_back(kB64[(v >> 12) & 63]);
    s.push_back(kB64[(v >> 6) & 63]);
    s.push_back('=');
  }
  return s;
}

static int b64_val(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

std::optional<Bytes> base64_decode(std::string_view s) {
  std::string clean;
  clean.reserve(s.size());
  for (char c : s)
    if (!is_ws(c)) clean.push_back(c);
  if (clean.size() % 4 != 0) return std::nullopt;
  Bytes out;
  out.reserve(clean.size() / 4 * 3);
  for (std::size_t i = 0; i < clean.size(); i += 4) {
    const bool last = i + 4 == clean.size();
    int v[4];
    int pad = 0;
    for (int j = 0; j < 4; ++j) {
      const char c = clean[i + j];
      if (c == '=') {
        if (!last || j < 2) return std::nullopt;
        ++pad;
        v[j] = 0;
      } else {
        if (pad) return std::nullopt;  // data after '='
        v[j] = b64_val(c);
        if (v[j] < 0) return std::nullopt;
      }
    }
    const uint32_t x = (static_cast<uint32_t>(v[0]) << 18) | (static_cast<uint32_t>(v[1]) << 12) |
                       (static_cast<uint32_t>(v[2]) << 6) | static_cast<uint32_t>(v[3]);
    if (pad == 2 && (x & 0xFFFF) != 0) return std::nullopt;  // non-canonical trailing bits
    if (pad == 1 && (x & 0xFF) != 0) return std::nullopt;
    out.push_back(static_cast<uint8_t>(x >> 16));
    if (pad < 2) out.push_back(static_cast<uint8_t>(x >> 8));
    if (pad < 1) out.push_back(static_cast<uint8_t>(x));
  }
  return out;
}

Bytes encode_for_file(const Bytes& raw, Encoding e) {
  if (e == Encoding::Raw) return raw;
  std::string t = (e == Encoding::Hex) ? hex_encode(raw) : base64_encode(raw);
  t.push_back('\n');
  return Bytes(t.begin(), t.end());
}

std::optional<Bytes> decode_from_file(const Bytes& file_bytes, Encoding e) {
  if (e == Encoding::Raw) return file_bytes;
  const std::string_view sv(reinterpret_cast<const char*>(file_bytes.data()), file_bytes.size());
  return e == Encoding::Hex ? hex_decode(sv) : base64_decode(sv);
}

}  // namespace rsalab
