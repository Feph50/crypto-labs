#include "keyio.hpp"

#include <nlohmann/json.hpp>

#include "encoding.hpp"
#include "errors.hpp"

namespace rsalab {

bool is_supported_modulus_bits(unsigned bits) { return bits == 3072 || bits == 4096; }

unsigned modulus_bits(const PublicKey& k) { return static_cast<unsigned>(k.GetModulus().BitCount()); }
unsigned modulus_bits(const PrivateKey& k) { return static_cast<unsigned>(k.GetModulus().BitCount()); }

KeyPair generate_keypair(unsigned bits, CryptoPP::RandomNumberGenerator& rng, bool allow_unsupported_for_test) {
  if (!allow_unsupported_for_test && !is_supported_modulus_bits(bits))
    usage_error("unsupported key size " + std::to_string(bits) + " (use 3072 or 4096)");
  CryptoPP::InvertibleRSAFunction params;
  params.Initialize(rng, bits, CryptoPP::Integer(65537));  // Crypto++ defaults to e = 17: ask for 65537 explicitly
  KeyPair kp;
  kp.priv = PrivateKey(params);
  kp.pub = PublicKey(params);
  kp.bits = bits;
  return kp;
}

static Bytes queue_to_bytes(CryptoPP::ByteQueue& q) {
  Bytes out(static_cast<std::size_t>(q.MaxRetrievable()));
  if (!out.empty()) q.Get(out.data(), out.size());
  return out;
}

Bytes public_key_to_der(const PublicKey& k) {
  CryptoPP::ByteQueue q;
  k.Save(q);
  return queue_to_bytes(q);
}

Bytes private_key_to_der(const PrivateKey& k) {
  CryptoPP::ByteQueue q;
  k.Save(q);
  return queue_to_bytes(q);
}

std::string der_to_pem(const Bytes& der, const std::string& label) {
  const std::string b64 = base64_encode(der);
  std::string pem = "-----BEGIN " + label + "-----\n";
  for (std::size_t i = 0; i < b64.size(); i += 64) pem += b64.substr(i, 64) + "\n";
  pem += "-----END " + label + "-----\n";
  return pem;
}

std::optional<Bytes> pem_to_der(const std::string& text, const std::string& label) {
  const std::string begin = "-----BEGIN " + label + "-----", end = "-----END " + label + "-----";
  std::string body;
  int stage = 0;  // 0 before BEGIN, 1 inside, 2 after END
  std::size_t pos = 0;
  while (pos <= text.size()) {
    std::size_t nl = text.find('\n', pos);
    if (nl == std::string::npos) nl = text.size();
    std::string line = text.substr(pos, nl - pos);
    pos = nl + 1;
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
    std::size_t lead = 0;
    while (lead < line.size() && (line[lead] == ' ' || line[lead] == '\t')) ++lead;
    line = line.substr(lead);
    if (line.empty()) continue;
    if (stage == 0) {
      if (line != begin) return std::nullopt;  // wrong label (e.g. "RSA PRIVATE KEY") or garbage before the armor
      stage = 1;
    } else if (stage == 1) {
      if (line == end) {
        stage = 2;
      } else {
        if (line.find(':') != std::string::npos || line.rfind("-----", 0) == 0) return std::nullopt;  // headers / other armor
        body += line;
      }
    } else {
      return std::nullopt;  // anything after END
    }
  }
  if (stage != 2 || body.empty()) return std::nullopt;
  return base64_decode(body);
}

static void check_supported_or_throw(unsigned bits, bool allow_unsupported_for_test) {
  if (!allow_unsupported_for_test && !is_supported_modulus_bits(bits)) io_error("invalid or unsupported key file");
}

PublicKey public_key_from_der(const Bytes& der, bool allow_unsupported_for_test) {
  try {
    CryptoPP::ByteQueue q;
    q.Put(der.data(), der.size());
    q.MessageEnd();
    PublicKey k;
    k.Load(q);
    if (!q.IsEmpty()) io_error("invalid or unsupported key file");  // trailing bytes
    CryptoPP::AutoSeededRandomPool rng;
    if (!k.Validate(rng, 2)) io_error("invalid or unsupported key file");
    check_supported_or_throw(modulus_bits(k), allow_unsupported_for_test);
    return k;
  } catch (const CryptoPP::Exception&) {
    io_error("invalid or unsupported key file");
  }
}

PrivateKey private_key_from_der(const Bytes& der, bool allow_unsupported_for_test) {
  try {
    CryptoPP::ByteQueue q;
    q.Put(der.data(), der.size());
    q.MessageEnd();
    PrivateKey k;
    k.Load(q);
    if (!q.IsEmpty()) io_error("invalid or unsupported key file");
    CryptoPP::AutoSeededRandomPool rng;
    if (!k.Validate(rng, 1)) io_error("invalid or unsupported key file");
    check_supported_or_throw(modulus_bits(k), allow_unsupported_for_test);
    return k;
  } catch (const CryptoPP::Exception&) {
    io_error("invalid or unsupported key file");
  }
}

static bool looks_like_pem(const Bytes& b) {
  std::size_t i = 0;
  while (i < b.size() && (b[i] == ' ' || b[i] == '\t' || b[i] == '\r' || b[i] == '\n')) ++i;
  static const char* kMarker = "-----BEGIN";
  for (std::size_t j = 0; kMarker[j]; ++j)
    if (i + j >= b.size() || b[i + j] != static_cast<uint8_t>(kMarker[j])) return false;
  return true;
}

constexpr std::size_t kMaxKeyFile = 1u << 20;

PublicKey load_public_key(const std::string& path) {
  const Bytes data = read_file(path, kMaxKeyFile);
  if (looks_like_pem(data)) {
    const auto der = pem_to_der(to_string(data), "PUBLIC KEY");
    if (!der) io_error("invalid or unsupported key file");
    return public_key_from_der(*der);
  }
  return public_key_from_der(data);
}

PrivateKey load_private_key(const std::string& path) {
  Bytes data = read_file(path, kMaxKeyFile);
  WipeOnExit wipe(data);
  if (looks_like_pem(data)) {
    auto der = pem_to_der(to_string(data), "PRIVATE KEY");
    if (!der) io_error("invalid or unsupported key file");
    WipeOnExit wipe_der(*der);
    return private_key_from_der(*der);
  }
  return private_key_from_der(data);
}

std::string metadata_json(unsigned bits, const std::string& creation_time_utc) {
  nlohmann::ordered_json j;
  j["creation_time"] = creation_time_utc;
  j["modulus_bits"] = bits;
  j["hash"] = "SHA-256";
  return j.dump(2) + "\n";
}

SavedFiles save_keypair(const KeyPair& kp, const std::string& pub_path, const std::string& priv_path, bool overwrite) {
  SavedFiles f;
  f.pub_pem = pub_path;
  f.pub_der = path_with_extension(pub_path, ".der");
  f.priv_pem = priv_path;
  f.priv_der = path_with_extension(priv_path, ".der");
  f.meta = path_with_extension(priv_path, ".meta.json");
  if (f.pub_pem == f.priv_pem || f.pub_der == f.priv_der) usage_error("--pub and --priv must be different files");
  if (!overwrite) {
    for (const std::string* p : {&f.pub_pem, &f.pub_der, &f.priv_pem, &f.priv_der, &f.meta})
      if (file_exists(*p)) io_error("output file '" + *p + "' already exists (use --force to replace it)");
  }
  const Bytes pub_der = public_key_to_der(kp.pub);
  Bytes priv_der = private_key_to_der(kp.priv);
  WipeOnExit wipe(priv_der);
  std::string priv_pem = der_to_pem(priv_der, "PRIVATE KEY");
  const std::string pub_pem = der_to_pem(pub_der, "PUBLIC KEY");
  const std::string meta = metadata_json(kp.bits, utc_now_iso8601());
  auto put = [&](const std::string& path, const std::string& s, bool priv) {
    write_file(path, reinterpret_cast<const uint8_t*>(s.data()), s.size(), overwrite, priv);
  };
  put(f.pub_pem, pub_pem, false);
  write_file(f.pub_der, pub_der, overwrite, false);
  put(f.priv_pem, priv_pem, true);
  write_file(f.priv_der, priv_der, overwrite, true);
  put(f.meta, meta, false);
  secure_zero(&priv_pem[0], priv_pem.size());
  return f;
}

}  // namespace rsalab
