#include <catch2/catch_test_macros.hpp>

#include "encoding.hpp"
#include "envelope.hpp"
#include "errors.hpp"
#include "hybrid.hpp"
#include "test_support.hpp"

using namespace rsalab;
using namespace testsupport;

TEST_CASE("auto mode: <= 318 bytes is direct, 319 switches to hybrid (RSA-3072)", "[hybrid]") {
  CryptoPP::AutoSeededRandomPool rng;
  const auto& k = key3072();
  for (std::size_t n : {std::size_t(0), std::size_t(1), std::size_t(318), std::size_t(319), std::size_t(1000)}) {
    const Bytes m = random_bytes(n);
    const EncryptResult r = encrypt_message(k.pub, m.data(), m.size(), {}, false, rng);
    REQUIRE(r.mode == (n <= 318 ? Mode::Direct : Mode::Hybrid));
    REQUIRE(decrypt_message(k.priv, r.container, {}) == m);
  }
}

TEST_CASE("hybrid round trip: 1 KiB and 1 MiB, with and without a label", "[hybrid]") {
  CryptoPP::AutoSeededRandomPool rng;
  const auto& k = key3072();
  for (std::size_t n : {std::size_t(1024), std::size_t(1) << 20}) {
    const Bytes m = random_bytes(n);
    for (const char* lab : {"", "ctx"}) {
      const Bytes label = bytes_of(lab);
      const EncryptResult r = encrypt_message(k.pub, m.data(), m.size(), label, false, rng);
      REQUIRE(r.mode == Mode::Hybrid);
      REQUIRE(decrypt_message(k.priv, r.container, label) == m);
    }
  }
}

TEST_CASE("--force-hybrid works on a short message", "[hybrid]") {
  CryptoPP::AutoSeededRandomPool rng;
  const Bytes m = bytes_of("tiny");
  const EncryptResult r = encrypt_message(key3072().pub, m.data(), m.size(), {}, true, rng);
  REQUIRE(r.mode == Mode::Hybrid);
  REQUIRE(decrypt_message(key3072().priv, r.container, {}) == m);
}

TEST_CASE("RSA-4096 hybrid round trip", "[hybrid][slow]") {
  CryptoPP::AutoSeededRandomPool rng;
  const auto& k = key4096();
  const Bytes m = random_bytes(5000);
  const EncryptResult r = encrypt_message(k.pub, m.data(), m.size(), {}, false, rng);
  REQUIRE(r.mode == Mode::Hybrid);
  REQUIRE(decrypt_message(k.priv, r.container, {}) == m);
  const Bytes small = random_bytes(446);  // direct limit of RSA-4096
  REQUIRE(encrypt_message(k.pub, small.data(), small.size(), {}, false, rng).mode == Mode::Direct);
  const Bytes over = random_bytes(447);
  REQUIRE(encrypt_message(k.pub, over.data(), over.size(), {}, false, rng).mode == Mode::Hybrid);
}

TEST_CASE("envelope header has exactly the fields of the lab, in the documented form", "[hybrid][envelope]") {
  CryptoPP::AutoSeededRandomPool rng;
  const Bytes m = random_bytes(2000);
  const EncryptResult r = encrypt_message(key3072().pub, m.data(), m.size(), {}, false, rng);
  const Parsed p = parse_container(r.container);
  REQUIRE(p.header.mode == Mode::Hybrid);
  REQUIRE(p.header.rsa_modulus == 3072);
  REQUIRE(p.header.wrapped_key.size() == 384);
  REQUIRE(p.header.iv.size() == 12);   // 96-bit GCM IV
  REQUIRE(p.header.tag.size() == 16);  // 128-bit tag
  REQUIRE(p.payload.size() == m.size());  // GCM is length preserving
  const std::string hj = header_json(p.header);
  REQUIRE(hj.rfind("{\"mode\":\"RSA-OAEP-AES-GCM\",\"rsa_modulus\":3072,\"hash\":\"SHA-256\",\"wrapped_key\":\"", 0) == 0);
  REQUIRE(hj.find("\"iv\":\"") != std::string::npos);
  REQUIRE(hj.find("\"tag\":\"") != std::string::npos);
  REQUIRE(r.container[0] == 'R');
  REQUIRE(r.container[3] == 'E');
}

TEST_CASE("direct container header", "[hybrid][envelope]") {
  CryptoPP::AutoSeededRandomPool rng;
  const Bytes m = bytes_of("short");
  const EncryptResult r = encrypt_message(key3072().pub, m.data(), m.size(), {}, false, rng);
  const Parsed p = parse_container(r.container);
  REQUIRE(p.header.mode == Mode::Direct);
  REQUIRE(header_json(p.header) == "{\"mode\":\"RSA-OAEP\",\"rsa_modulus\":3072,\"hash\":\"SHA-256\"}");
  REQUIRE(p.payload.size() == 384);
}

TEST_CASE("two encryptions of the same data differ (fresh AES key, IV and OAEP randomness)", "[hybrid]") {
  CryptoPP::AutoSeededRandomPool rng;
  const Bytes m = random_bytes(4096);
  const EncryptResult a = encrypt_message(key3072().pub, m.data(), m.size(), {}, false, rng);
  const EncryptResult b = encrypt_message(key3072().pub, m.data(), m.size(), {}, false, rng);
  REQUIRE(a.container != b.container);
  const Parsed pa = parse_container(a.container), pb = parse_container(b.container);
  REQUIRE(pa.header.iv != pb.header.iv);
  REQUIRE(pa.header.wrapped_key != pb.header.wrapped_key);
  REQUIRE(pa.payload != pb.payload);
}

TEST_CASE("AES-256-GCM building block: round trip and tag check", "[hybrid][gcm]") {
  const Bytes key = random_bytes(32), iv = random_bytes(12), aad = bytes_of("aad"), msg = random_bytes(100);
  Bytes ct(msg.size()), tag(16), pt(msg.size());
  aes256_gcm_encrypt(key.data(), iv.data(), aad.data(), aad.size(), msg.data(), msg.size(), ct.data(), tag.data());
  REQUIRE(aes256_gcm_decrypt(key.data(), iv.data(), aad.data(), aad.size(), ct.data(), ct.size(), tag.data(), pt.data()));
  REQUIRE(pt == msg);
  Bytes badtag = tag;
  badtag[0] ^= 1;
  REQUIRE_FALSE(aes256_gcm_decrypt(key.data(), iv.data(), aad.data(), aad.size(), ct.data(), ct.size(), badtag.data(), pt.data()));
  const Bytes other_aad = bytes_of("aaX");
  REQUIRE_FALSE(aes256_gcm_decrypt(key.data(), iv.data(), other_aad.data(), other_aad.size(), ct.data(), ct.size(), tag.data(), pt.data()));
}
