#include "envelope.hpp"

#include <nlohmann/json.hpp>

#include "encoding.hpp"
#include "errors.hpp"
#include "keyio.hpp"

namespace rsalab {

static const uint8_t kMagic[4] = {'R', 'S', 'A', 'E'};
static const uint8_t kVersion = 0x01;

std::string header_json(const Header& h) {
  std::string s = std::string("{\"mode\":\"") + (h.mode == Mode::Direct ? kModeDirect : kModeHybrid) +
                  "\",\"rsa_modulus\":" + std::to_string(h.rsa_modulus) + ",\"hash\":\"" + kHashName + "\"";
  if (h.mode == Mode::Hybrid) {
    s += ",\"wrapped_key\":\"" + base64_encode(h.wrapped_key) + "\",\"iv\":\"" + base64_encode(h.iv) + "\",\"tag\":\"" +
         base64_encode(h.tag) + "\"";
  }
  s += "}";
  return s;
}

Bytes header_aad(const Header& h) {
  std::string s = std::string("RSAE-AAD-v1|") + (h.mode == Mode::Direct ? kModeDirect : kModeHybrid) + "|" +
                  std::to_string(h.rsa_modulus) + "|" + kHashName + "|" + base64_encode(h.wrapped_key) + "|" + base64_encode(h.iv);
  return Bytes(s.begin(), s.end());
}

Bytes build_container(const Header& h, const Bytes& payload) {
  const std::string hj = header_json(h);
  Bytes out;
  out.reserve(9 + hj.size() + payload.size());
  out.insert(out.end(), kMagic, kMagic + 4);
  out.push_back(kVersion);
  const uint32_t n = static_cast<uint32_t>(hj.size());
  out.push_back(static_cast<uint8_t>(n >> 24));
  out.push_back(static_cast<uint8_t>(n >> 16));
  out.push_back(static_cast<uint8_t>(n >> 8));
  out.push_back(static_cast<uint8_t>(n));
  out.insert(out.end(), hj.begin(), hj.end());
  out.insert(out.end(), payload.begin(), payload.end());
  return out;
}

static bool get_b64(const nlohmann::json& j, const char* key, Bytes& out) {
  if (!j.contains(key) || !j[key].is_string()) return false;
  const auto d = base64_decode(j[key].get<std::string>());
  if (!d) return false;
  out = *d;
  return true;
}

Parsed parse_container(const Bytes& data) {
  if (data.size() < 9) crypto_failure();
  for (int i = 0; i < 4; ++i)
    if (data[i] != kMagic[i]) crypto_failure();
  if (data[4] != kVersion) crypto_failure();
  const std::size_t hlen = (static_cast<std::size_t>(data[5]) << 24) | (static_cast<std::size_t>(data[6]) << 16) |
                           (static_cast<std::size_t>(data[7]) << 8) | static_cast<std::size_t>(data[8]);
  if (hlen == 0 || hlen > kMaxHeaderLen || hlen > data.size() - 9) crypto_failure();
  const std::string text(reinterpret_cast<const char*>(data.data()) + 9, hlen);

  const nlohmann::json j = nlohmann::json::parse(text, nullptr, /*allow_exceptions=*/false);
  if (j.is_discarded() || !j.is_object()) crypto_failure();
  if (!j.contains("mode") || !j["mode"].is_string()) crypto_failure();
  if (!j.contains("rsa_modulus") || !j["rsa_modulus"].is_number_unsigned()) crypto_failure();
  if (!j.contains("hash") || !j["hash"].is_string() || j["hash"].get<std::string>() != kHashName) crypto_failure();

  Parsed p;
  const std::string mode = j["mode"].get<std::string>();
  p.header.rsa_modulus = j["rsa_modulus"].get<unsigned>();
  if (!is_supported_modulus_bits(p.header.rsa_modulus)) crypto_failure();
  if (mode == kModeDirect) {
    p.header.mode = Mode::Direct;
    if (j.size() != 3) crypto_failure();
  } else if (mode == kModeHybrid) {
    p.header.mode = Mode::Hybrid;
    if (j.size() != 6) crypto_failure();
    if (!get_b64(j, "wrapped_key", p.header.wrapped_key) || !get_b64(j, "iv", p.header.iv) || !get_b64(j, "tag", p.header.tag))
      crypto_failure();
    if (p.header.wrapped_key.size() != modulus_bytes(p.header.rsa_modulus) || p.header.iv.size() != kGcmIvLen ||
        p.header.tag.size() != kGcmTagLen)
      crypto_failure();
  } else {
    crypto_failure();
  }
  // Canonical form only: rejects extra whitespace, reordered keys, duplicated keys, non-canonical base64.
  if (header_json(p.header) != text) crypto_failure();
  p.payload.assign(data.begin() + 9 + static_cast<std::ptrdiff_t>(hlen), data.end());
  return p;
}

}  // namespace rsalab
