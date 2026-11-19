#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <regex>

#include "encoding.hpp"
#include "errors.hpp"
#include "test_support.hpp"

using namespace rsalab;
using namespace testsupport;

TEST_CASE("generated keys: RSA-3072, e = 65537, modulus size", "[keyio]") {
  const auto& k = key3072();
  REQUIRE(k.bits == 3072);
  REQUIRE(modulus_bits(k.pub) == 3072);
  REQUIRE(modulus_bits(k.priv) == 3072);
  REQUIRE(k.pub.GetPublicExponent() == CryptoPP::Integer(65537));
}

TEST_CASE("only RSA >= 3072 is supported", "[keyio][negative]") {
  CryptoPP::AutoSeededRandomPool rng;
  REQUIRE(is_supported_modulus_bits(3072));
  REQUIRE(is_supported_modulus_bits(4096));
  REQUIRE_FALSE(is_supported_modulus_bits(2048));
  REQUIRE_FALSE(is_supported_modulus_bits(1024));
  REQUIRE_THROWS_AS(generate_keypair(2048, rng), Error);
  REQUIRE_THROWS_AS(generate_keypair(3071, rng), Error);
}

TEST_CASE("DER round trip (SPKI and PKCS#8)", "[keyio]") {
  const auto& k = key3072();
  const Bytes pub_der = public_key_to_der(k.pub);
  const Bytes priv_der = private_key_to_der(k.priv);
  const PublicKey pub2 = public_key_from_der(pub_der);
  const PrivateKey priv2 = private_key_from_der(priv_der);
  REQUIRE(pub2.GetModulus() == k.pub.GetModulus());
  REQUIRE(priv2.GetPrivateExponent() == k.priv.GetPrivateExponent());
  REQUIRE(pub_der[0] == 0x30);   // SEQUENCE
  REQUIRE(priv_der[0] == 0x30);
}

TEST_CASE("PEM armor: labels, 64-column lines, round trip with DER", "[keyio]") {
  const auto& k = key3072();
  const Bytes der = public_key_to_der(k.pub);
  const std::string pem = der_to_pem(der, "PUBLIC KEY");
  REQUIRE(pem.rfind("-----BEGIN PUBLIC KEY-----\n", 0) == 0);
  REQUIRE(pem.find("-----END PUBLIC KEY-----\n") != std::string::npos);
  std::size_t pos = pem.find('\n') + 1;
  while (true) {
    const std::size_t nl = pem.find('\n', pos);
    const std::string line = pem.substr(pos, nl - pos);
    if (line.rfind("-----END", 0) == 0) break;
    REQUIRE(line.size() <= 64);
    pos = nl + 1;
  }
  const auto back = pem_to_der(pem, "PUBLIC KEY");
  REQUIRE(back.has_value());
  REQUIRE(*back == der);
  // CRLF line endings and trailing spaces are tolerated
  std::string crlf;
  for (char c : pem) {
    if (c == '\n') crlf += " \r\n";
    else crlf += c;
  }
  REQUIRE(pem_to_der(crlf, "PUBLIC KEY").has_value());
}

TEST_CASE("PEM parser is strict", "[keyio][negative]") {
  const auto& k = key3072();
  const std::string pem = der_to_pem(public_key_to_der(k.pub), "PUBLIC KEY");
  REQUIRE_FALSE(pem_to_der(pem, "PRIVATE KEY").has_value());                 // wrong type
  REQUIRE_FALSE(pem_to_der(der_to_pem(Bytes{1, 2, 3}, "RSA PRIVATE KEY"), "PRIVATE KEY").has_value());
  REQUIRE_FALSE(pem_to_der(pem + "trailing garbage\n", "PUBLIC KEY").has_value());
  REQUIRE_FALSE(pem_to_der("garbage\n" + pem, "PUBLIC KEY").has_value());
  REQUIRE_FALSE(pem_to_der("-----BEGIN PUBLIC KEY-----\nProc-Type: 4,ENCRYPTED\nAAAA\n-----END PUBLIC KEY-----\n", "PUBLIC KEY").has_value());
  REQUIRE_FALSE(pem_to_der("-----BEGIN PUBLIC KEY-----\nAAAA\n", "PUBLIC KEY").has_value());  // no END
  REQUIRE_FALSE(pem_to_der("-----BEGIN PUBLIC KEY-----\n!!!!\n-----END PUBLIC KEY-----\n", "PUBLIC KEY").has_value());  // bad base64
}

TEST_CASE("key files: keygen writes PEM, DER and metadata; loading detects PEM or DER", "[keyio]") {
  const auto dir = temp_dir("keys");
  const std::string pub = (dir / "pub.pem").string(), priv = (dir / "priv.pem").string();
  const SavedFiles f = save_keypair(key3072(), pub, priv, false);
  REQUIRE(f.pub_der == (dir / "pub.der").string());
  REQUIRE(f.priv_der == (dir / "priv.der").string());
  REQUIRE(f.meta == (dir / "priv.meta.json").string());
  for (const std::string* p : {&f.pub_pem, &f.pub_der, &f.priv_pem, &f.priv_der, &f.meta}) REQUIRE(file_exists(*p));

  const PublicKey a = load_public_key(f.pub_pem), b = load_public_key(f.pub_der);
  REQUIRE(a.GetModulus() == b.GetModulus());
  REQUIRE(a.GetModulus() == key3072().pub.GetModulus());
  const PrivateKey c = load_private_key(f.priv_pem), d = load_private_key(f.priv_der);
  REQUIRE(c.GetPrivateExponent() == d.GetPrivateExponent());
  REQUIRE(c.GetModulus() == key3072().priv.GetModulus());

  // the PEM files carry the expected armor
  const Bytes pem_pub = read_file(f.pub_pem, 1 << 20), pem_priv = read_file(f.priv_pem, 1 << 20);
  REQUIRE(to_string(pem_pub).rfind("-----BEGIN PUBLIC KEY-----", 0) == 0);
  REQUIRE(to_string(pem_priv).rfind("-----BEGIN PRIVATE KEY-----", 0) == 0);
}

TEST_CASE("metadata JSON: exactly creation_time, modulus_bits, hash", "[keyio]") {
  const auto dir = temp_dir("meta");
  const SavedFiles f = save_keypair(key3072(), (dir / "p.pem").string(), (dir / "k.pem").string(), false);
  const auto j = nlohmann::json::parse(to_string(read_file(f.meta, 1 << 16)));
  REQUIRE(j.size() == 3);
  REQUIRE(j["modulus_bits"].is_number_unsigned());
  REQUIRE(j["modulus_bits"].get<unsigned>() == 3072);
  REQUIRE(j["hash"].get<std::string>() == "SHA-256");
  REQUIRE(std::regex_match(j["creation_time"].get<std::string>(), std::regex(R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$)")));
}

TEST_CASE("keygen refuses to overwrite existing key files without --force", "[keyio][negative]") {
  const auto dir = temp_dir("overwrite");
  const std::string pub = (dir / "pub.pem").string(), priv = (dir / "priv.pem").string();
  save_keypair(key3072(), pub, priv, false);
  REQUIRE_THROWS_AS(save_keypair(key3072_other(), pub, priv, false), Error);
  REQUIRE(load_public_key(pub).GetModulus() == key3072().pub.GetModulus());  // untouched
  save_keypair(key3072_other(), pub, priv, true);
  REQUIRE(load_public_key(pub).GetModulus() == key3072_other().pub.GetModulus());
  REQUIRE_THROWS_AS(save_keypair(key3072(), pub, pub, true), Error);          // same file for both keys
}

TEST_CASE("unsupported and malformed key files are rejected", "[keyio][negative]") {
  CryptoPP::AutoSeededRandomPool rng;
  const KeyPair weak = generate_keypair(2048, rng, /*allow_unsupported_for_test=*/true);
  const Bytes pub_der = public_key_to_der(weak.pub), priv_der = private_key_to_der(weak.priv);
  REQUIRE_THROWS_AS(public_key_from_der(pub_der), Error);   // 2048-bit: unsupported
  REQUIRE_THROWS_AS(private_key_from_der(priv_der), Error);
  REQUIRE_NOTHROW(public_key_from_der(pub_der, true));       // only the test-only flag lets it through

  const Bytes good = public_key_to_der(key3072().pub);
  Bytes trailing = good;
  trailing.push_back(0);
  REQUIRE_THROWS_AS(public_key_from_der(trailing), Error);
  Bytes truncated(good.begin(), good.end() - 5);
  REQUIRE_THROWS_AS(public_key_from_der(truncated), Error);
  REQUIRE_THROWS_AS(public_key_from_der(Bytes{0x30, 0x03, 0x01, 0x01, 0x00}), Error);
  REQUIRE_THROWS_AS(private_key_from_der(good), Error);     // a public key is not a private key
  REQUIRE_THROWS_AS(load_private_key((temp_dir("none") / "missing.pem").string()), Error);
}

TEST_CASE("path_with_extension", "[keyio]") {
  REQUIRE(path_with_extension("a/pub.pem", ".der") == "a/pub.der");
  REQUIRE(path_with_extension("a/PUB.PEM", ".der") == "a/PUB.der");
  REQUIRE(path_with_extension("a/key", ".der") == "a/key.der");
  REQUIRE(path_with_extension("priv.pem", ".meta.json") == "priv.meta.json");
}
