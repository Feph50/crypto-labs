#include "kat.hpp"

#include <nlohmann/json.hpp>

#include "encoding.hpp"
#include "errors.hpp"
#include "hybrid.hpp"
#include "keyio.hpp"
#include "rsa_oaep.hpp"

namespace rsalab {

namespace {

using json = nlohmann::json;

struct Reporter {
  std::ostream& os;
  KatSummary s;
  void result(const std::string& name, bool ok) {
    ++s.total;
    ok ? ++s.passed : ++s.failed;
    os << (ok ? "PASS  " : "FAIL  ") << name << "\n";
  }
};

std::optional<Bytes> field(const json& t, const char* key) {
  if (!t.is_object() || !t.contains(key) || !t[key].is_string()) return std::nullopt;
  return hex_decode(t[key].get<std::string>());
}

std::string id_of(const json& t) { return t.is_object() && t.contains("id") && t["id"].is_string() ? t["id"].get<std::string>() : "?"; }

void run_oaep(const json& sec, Reporter& rep) {
  const std::string section = "rsa_oaep_decrypt";
  const auto kdata = field(sec, "private_key_pkcs8_hex");
  if (!kdata) {
    rep.result(section + "/private-key", false);
    return;
  }
  std::optional<PrivateKey> key;
  try {
    key = private_key_from_der(*kdata);
  } catch (const Error&) {
  }
  if (!key) {
    rep.result(section + "/private-key", false);
    return;
  }
  CryptoPP::AutoSeededRandomPool rng;
  for (const json& t : sec["tests"]) {
    const std::string name = section + "/" + id_of(t);
    const auto ct = field(t, "ct_hex"), label = field(t, "label_hex"), msg = field(t, "msg_hex");
    if (!ct || !label || !msg || !t.contains("result")) {
      rep.result(name, false);
      continue;
    }
    const bool expect_valid = t["result"].get<std::string>() == "valid";
    bool decrypted = false;
    Bytes plain;
    try {
      plain = oaep_decrypt(*key, ct->data(), ct->size(), *label, rng);
      decrypted = true;
    } catch (const Error&) {
    }
    const bool ok = expect_valid ? (decrypted && plain == *msg) : !decrypted;
    rep.result(name + (expect_valid ? " (valid: must decrypt)" : " (invalid: must be rejected)"), ok);
  }
}

void run_gcm(const json& sec, Reporter& rep) {
  for (const json& t : sec["tests"]) {
    const std::string name = "aes256_gcm/" + id_of(t);
    const auto key = field(t, "key_hex"), iv = field(t, "iv_hex"), aad = field(t, "aad_hex"), pt = field(t, "pt_hex"),
               ct = field(t, "ct_hex"), tag = field(t, "tag_hex");
    if (!key || !iv || !aad || !pt || !ct || !tag || key->size() != 32 || iv->size() != 12 || tag->size() != 16 || pt->size() != ct->size()) {
      rep.result(name, false);
      continue;
    }
    Bytes got_ct(pt->size()), got_tag(16), got_pt(ct->size());
    aes256_gcm_encrypt(key->data(), iv->data(), aad->data(), aad->size(), pt->data(), pt->size(), got_ct.data(), got_tag.data());
    const bool enc_ok = got_ct == *ct && got_tag == *tag;
    const bool dec_ok = aes256_gcm_decrypt(key->data(), iv->data(), aad->data(), aad->size(), ct->data(), ct->size(), tag->data(), got_pt.data()) &&
                        got_pt == *pt;
    rep.result(name + " (encrypt + decrypt)", enc_ok && dec_ok);
  }
}

void run_gcm_reject(const json& sec, Reporter& rep) {
  for (const json& t : sec["tests"]) {
    const std::string name = "aes256_gcm_reject/" + id_of(t);
    const auto key = field(t, "key_hex"), iv = field(t, "iv_hex"), aad = field(t, "aad_hex"), ct = field(t, "ct_hex"), tag = field(t, "tag_hex");
    if (!key || !iv || !aad || !ct || !tag || key->size() != 32 || iv->size() != 12 || tag->size() != 16) {
      rep.result(name, false);
      continue;
    }
    Bytes pt(ct->size());
    const bool accepted = aes256_gcm_decrypt(key->data(), iv->data(), aad->data(), aad->size(), ct->data(), ct->size(), tag->data(), pt.data());
    rep.result(name + " (authentication must fail)", !accepted);
  }
}

}  // namespace

KatSummary run_kat_file(const std::string& path, std::ostream& os) {
  const Bytes raw = read_file(path, 64u << 20);
  json root = json::parse(raw.begin(), raw.end(), nullptr, /*allow_exceptions=*/false);
  if (root.is_discarded() || !root.is_object()) io_error("KAT file is not valid JSON");
  Reporter rep{os, {}};
  for (const char* name : {"rsa_oaep_decrypt", "aes256_gcm", "aes256_gcm_reject"}) {
    if (!root.contains(name) || !root[name].is_object() || !root[name].contains("tests") || !root[name]["tests"].is_array())
      io_error(std::string("KAT file: section '") + name + "' is missing or malformed");
  }
  run_oaep(root["rsa_oaep_decrypt"], rep);
  run_gcm(root["aes256_gcm"], rep);
  run_gcm_reject(root["aes256_gcm_reject"], rep);
  os << "\nSummary: " << rep.s.total << " tests, " << rep.s.passed << " passed, " << rep.s.failed << " failed\n";
  return rep.s;
}

}  // namespace rsalab
