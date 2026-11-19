// Negative tests: every tampering / misuse must be rejected (fail closed) with the SAME generic message and exit code,
// and must not release any plaintext.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "encoding.hpp"
#include "envelope.hpp"
#include "errors.hpp"
#include "hybrid.hpp"
#include "test_support.hpp"

using namespace rsalab;
using namespace testsupport;

namespace {

void expect_generic_failure(const PrivateKey& k, const Bytes& container, const Bytes& label = {}) {
  try {
    const Bytes plain = decrypt_message(k, container, label);
    FAIL("a tampered or invalid ciphertext was accepted");
  } catch (const Error& e) {
    REQUIRE(e.code() == ExitCode::Crypto);
    const std::string what = e.what();
    REQUIRE(what == kDecryptFailedMessage);
    for (const char* leak : {"padding", "tag", "label", "OAEP", "mac", "MAC", "header", "magic", "length", "base64"})
      REQUIRE(what.find(leak) == std::string::npos);
  }
}

struct Pieces {
  std::string header;
  Bytes payload;
};

Pieces split(const Bytes& c) {
  const std::size_t hl = (std::size_t(c[5]) << 24) | (std::size_t(c[6]) << 16) | (std::size_t(c[7]) << 8) | c[8];
  return {std::string(c.begin() + 9, c.begin() + 9 + static_cast<std::ptrdiff_t>(hl)), Bytes(c.begin() + 9 + static_cast<std::ptrdiff_t>(hl), c.end())};
}

Bytes join(const std::string& header, const Bytes& payload) {
  Bytes out = {'R', 'S', 'A', 'E', 0x01};
  const uint32_t n = static_cast<uint32_t>(header.size());
  out.push_back(uint8_t(n >> 24));
  out.push_back(uint8_t(n >> 16));
  out.push_back(uint8_t(n >> 8));
  out.push_back(uint8_t(n));
  out.insert(out.end(), header.begin(), header.end());
  out.insert(out.end(), payload.begin(), payload.end());
  return out;
}

Bytes make(const Bytes& msg, const Bytes& label, bool force_hybrid, const PublicKey& pub = key3072().pub) {
  CryptoPP::AutoSeededRandomPool rng;
  return encrypt_message(pub, msg.data(), msg.size(), label, force_hybrid, rng).container;
}

}  // namespace

TEST_CASE("positive control: untouched ciphertexts decrypt", "[negative]") {
  const Bytes m = random_bytes(3000), label = bytes_of("L");
  REQUIRE(decrypt_message(key3072().priv, make(m, label, false), label) == m);
  REQUIRE(decrypt_message(key3072().priv, make(bytes_of("x"), label, false), label) == bytes_of("x"));
}

TEST_CASE("altered RSA ciphertext (direct mode) fails", "[negative]") {
  Bytes c = make(bytes_of("direct message"), {}, false);
  Pieces p = split(c);
  for (std::size_t pos : {std::size_t(0), std::size_t(100), p.payload.size() - 1}) {
    Bytes bad = p.payload;
    bad[pos] ^= 0x01;
    expect_generic_failure(key3072().priv, join(p.header, bad));
  }
}

TEST_CASE("altered AES-GCM ciphertext fails (tag failure)", "[negative]") {
  const Bytes c = make(random_bytes(5000), {}, false);
  const Pieces p = split(c);
  for (std::size_t pos : {std::size_t(0), std::size_t(2500), p.payload.size() - 1}) {
    Bytes bad = p.payload;
    bad[pos] ^= 0x80;
    expect_generic_failure(key3072().priv, join(p.header, bad));
  }
}

TEST_CASE("altered tag fails", "[negative]") {
  const Bytes c = make(random_bytes(2000), {}, false);
  Parsed p = parse_container(c);
  p.header.tag[0] ^= 0x01;
  expect_generic_failure(key3072().priv, build_container(p.header, p.payload));
  p = parse_container(c);
  p.header.tag.back() ^= 0x01;
  expect_generic_failure(key3072().priv, build_container(p.header, p.payload));
}

TEST_CASE("altered wrapped key fails", "[negative]") {
  Parsed p = parse_container(make(random_bytes(2000), {}, false));
  p.header.wrapped_key[7] ^= 0x01;
  expect_generic_failure(key3072().priv, build_container(p.header, p.payload));
}

TEST_CASE("wrong private key fails (direct and hybrid)", "[negative]") {
  const Bytes direct = make(bytes_of("for key A"), {}, false), hybrid = make(random_bytes(4000), {}, false);
  expect_generic_failure(key3072_other().priv, direct);
  expect_generic_failure(key3072_other().priv, hybrid);
}

TEST_CASE("wrong or missing OAEP label fails (direct and hybrid)", "[negative]") {
  const Bytes label = bytes_of("right-label");
  const Bytes direct = make(bytes_of("labelled"), label, false), hybrid = make(random_bytes(4000), label, false);
  for (const Bytes* c : {&direct, &hybrid}) {
    expect_generic_failure(key3072().priv, *c, bytes_of("wrong-label"));
    expect_generic_failure(key3072().priv, *c, {});                       // label missing
    expect_generic_failure(key3072().priv, *c, bytes_of("right-labe"));   // prefix of the label
    REQUIRE_NOTHROW(decrypt_message(key3072().priv, *c, label));          // control
  }
  // and the other way round: encrypted without a label, decrypted with one
  expect_generic_failure(key3072().priv, make(bytes_of("no label"), {}, false), bytes_of("unexpected"));
}

TEST_CASE("tampered envelope header fails", "[negative]") {
  const Bytes c = make(random_bytes(3000), {}, false);
  const Pieces p = split(c);
  const Parsed parsed = parse_container(c);
  auto replace = [](std::string s, const std::string& from, const std::string& to) {
    const std::size_t i = s.find(from);
    REQUIRE(i != std::string::npos);
    return s.replace(i, from.size(), to);
  };
  // changed iv / wrapped_key (valid base64, same length)
  Parsed t = parsed;
  t.header.iv[3] ^= 0x10;
  expect_generic_failure(key3072().priv, build_container(t.header, t.payload));
  t = parsed;
  t.header.wrapped_key[200] ^= 0x04;
  expect_generic_failure(key3072().priv, build_container(t.header, t.payload));
  // changed modulus field, mode field, hash field
  expect_generic_failure(key3072().priv, join(replace(p.header, "3072", "4096"), p.payload));
  expect_generic_failure(key3072().priv, join(replace(p.header, "RSA-OAEP-AES-GCM", "RSA-OAEP"), p.payload));
  expect_generic_failure(key3072().priv, join(replace(p.header, "SHA-256", "SHA-512"), p.payload));
  // formatting changes that a lax parser would accept: whitespace, extra field, reordered keys
  expect_generic_failure(key3072().priv, join(replace(p.header, ",\"hash\"", ", \"hash\""), p.payload));
  expect_generic_failure(key3072().priv, join(p.header.substr(0, p.header.size() - 1) + ",\"extra\":1}", p.payload));
  expect_generic_failure(key3072().priv, join(replace(replace(p.header, "\"mode\":\"RSA-OAEP-AES-GCM\",", ""), "\"hash\":\"SHA-256\"", "\"mode\":\"RSA-OAEP-AES-GCM\",\"hash\":\"SHA-256\""), p.payload));
  // not JSON at all, JSON of the wrong type
  expect_generic_failure(key3072().priv, join("not json", p.payload));
  expect_generic_failure(key3072().priv, join("[1,2,3]", p.payload));
  expect_generic_failure(key3072().priv, join("{}", p.payload));
  // base64 damage
  expect_generic_failure(key3072().priv, join(replace(p.header, "\"iv\":\"", "\"iv\":\"!"), p.payload));
}

TEST_CASE("bad header_len, magic and version fail", "[negative]") {
  const Bytes c = make(random_bytes(500), {}, false);
  for (std::uint32_t bogus : {0u, 1u, 65537u, 0xFFFFFFFFu, static_cast<std::uint32_t>(c.size())}) {
    Bytes bad = c;
    bad[5] = uint8_t(bogus >> 24);
    bad[6] = uint8_t(bogus >> 16);
    bad[7] = uint8_t(bogus >> 8);
    bad[8] = uint8_t(bogus);
    expect_generic_failure(key3072().priv, bad);
  }
  Bytes bad = c;
  bad[0] = 'X';
  expect_generic_failure(key3072().priv, bad);
  bad = c;
  bad[4] = 0x02;  // unknown version
  expect_generic_failure(key3072().priv, bad);
}

TEST_CASE("truncated or extended envelopes fail", "[negative]") {
  for (bool hybrid : {false, true}) {
    const Bytes c = hybrid ? make(random_bytes(2000), {}, false) : make(bytes_of("short"), {}, false);
    expect_generic_failure(key3072().priv, Bytes());
    expect_generic_failure(key3072().priv, Bytes(c.begin(), c.begin() + 4));
    expect_generic_failure(key3072().priv, Bytes(c.begin(), c.begin() + 9));
    expect_generic_failure(key3072().priv, Bytes(c.begin(), c.end() - 1));
    expect_generic_failure(key3072().priv, Bytes(c.begin(), c.begin() + static_cast<std::ptrdiff_t>(c.size() / 2)));
    Bytes extended = c;
    extended.push_back(0x00);
    expect_generic_failure(key3072().priv, extended);
  }
}

TEST_CASE("an envelope for another key size is rejected", "[negative][slow]") {
  const Bytes c4096 = make(bytes_of("for 4096"), {}, false, key4096().pub);
  expect_generic_failure(key3072().priv, c4096);  // rsa_modulus 4096 does not match the 3072-bit private key
}

TEST_CASE("random garbage never decrypts", "[negative]") {
  for (std::size_t n : {std::size_t(1), std::size_t(9), std::size_t(384), std::size_t(1000)}) expect_generic_failure(key3072().priv, random_bytes(n));
}
