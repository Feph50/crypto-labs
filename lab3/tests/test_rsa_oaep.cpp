#include <catch2/catch_test_macros.hpp>

#include "errors.hpp"
#include "rsa_oaep.hpp"
#include "test_support.hpp"

using namespace rsalab;
using namespace testsupport;

TEST_CASE("OAEP size limit: mLen <= k - 2*hLen - 2", "[oaep]") {
  REQUIRE(max_plaintext_len(3072) == 318);  // k = 384
  REQUIRE(max_plaintext_len(4096) == 446);  // k = 512
}

TEST_CASE("RSA-3072 OAEP round trip at the boundaries", "[oaep]") {
  CryptoPP::AutoSeededRandomPool rng;
  const auto& k = key3072();
  for (std::size_t n : {std::size_t(0), std::size_t(1), std::size_t(32), std::size_t(317), std::size_t(318)}) {
    const Bytes m = random_bytes(n);
    const Bytes ct = oaep_encrypt(k.pub, m.data(), m.size(), {}, rng);
    REQUIRE(ct.size() == 384);
    REQUIRE(oaep_decrypt(k.priv, ct.data(), ct.size(), {}, rng) == m);
  }
}

TEST_CASE("RSA-3072 OAEP rejects a message of mLen_max + 1", "[oaep][negative]") {
  CryptoPP::AutoSeededRandomPool rng;
  const Bytes m = random_bytes(319);
  REQUIRE_THROWS_AS(oaep_encrypt(key3072().pub, m.data(), m.size(), {}, rng), Error);
}

TEST_CASE("RSA-4096 OAEP round trip, including mLen_max", "[oaep][slow]") {
  CryptoPP::AutoSeededRandomPool rng;
  const auto& k = key4096();
  for (std::size_t n : {std::size_t(1), std::size_t(446)}) {
    const Bytes m = random_bytes(n);
    const Bytes ct = oaep_encrypt(k.pub, m.data(), m.size(), {}, rng);
    REQUIRE(ct.size() == 512);
    REQUIRE(oaep_decrypt(k.priv, ct.data(), ct.size(), {}, rng) == m);
  }
  const Bytes too_long = random_bytes(447);
  REQUIRE_THROWS_AS(oaep_encrypt(k.pub, too_long.data(), too_long.size(), {}, rng), Error);
}

TEST_CASE("OAEP label: matching label works, empty equals empty", "[oaep]") {
  CryptoPP::AutoSeededRandomPool rng;
  const auto& k = key3072();
  const Bytes m = bytes_of("label test");
  const Bytes label = bytes_of("context-A");
  const Bytes ct = oaep_encrypt(k.pub, m.data(), m.size(), label, rng);
  REQUIRE(oaep_decrypt(k.priv, ct.data(), ct.size(), label, rng) == m);
  const Bytes ct0 = oaep_encrypt(k.pub, m.data(), m.size(), {}, rng);
  REQUIRE(oaep_decrypt(k.priv, ct0.data(), ct0.size(), {}, rng) == m);
}

TEST_CASE("OAEP is randomised: two encryptions of one plaintext differ", "[oaep]") {
  CryptoPP::AutoSeededRandomPool rng;
  const Bytes m = bytes_of("same plaintext");
  const Bytes a = oaep_encrypt(key3072().pub, m.data(), m.size(), {}, rng);
  const Bytes b = oaep_encrypt(key3072().pub, m.data(), m.size(), {}, rng);
  REQUIRE(a != b);
}

TEST_CASE("OAEP decryption fails closed with the generic message", "[oaep][negative]") {
  CryptoPP::AutoSeededRandomPool rng;
  const auto& k = key3072();
  const Bytes m = bytes_of("secret");
  Bytes ct = oaep_encrypt(k.pub, m.data(), m.size(), {}, rng);
  ct[10] ^= 0x01;
  try {
    oaep_decrypt(k.priv, ct.data(), ct.size(), {}, rng);
    FAIL("a corrupted ciphertext was accepted");
  } catch (const Error& e) {
    REQUIRE(e.code() == ExitCode::Crypto);
    REQUIRE(std::string(e.what()) == kDecryptFailedMessage);
  }
  // wrong ciphertext length
  const Bytes shortct(100, 0x42);
  REQUIRE_THROWS_AS(oaep_decrypt(k.priv, shortct.data(), shortct.size(), {}, rng), Error);
}
