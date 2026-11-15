// Unit tests for Lab 2 (pure AES-128 + CTR; bonus: AES-192/256, AES-NI, XTS-AES). Run with `ctest` or `./aes_tests`.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>

#include "aes128.hpp"
#include "aesni.hpp"
#include "ctr.hpp"
#include "errors.hpp"
#include "json.hpp"
#include "kat.hpp"
#include "stats.hpp"
#include "util.hpp"
#include "xts.hpp"
#include "xval.hpp"

#ifndef AESP_KAT_FILE
#define AESP_KAT_FILE "kat/vectors.json"
#endif

using namespace aesp;

namespace {

const std::array<Backend, 3> kAll = {Backend::SBoxTable, Backend::SBoxComputed, Backend::TTable};

Bytes H(const std::string& s) { return from_hex(s); }

bool throws_code(const std::function<void()>& f, ExitCode code) {
  try {
    f();
  } catch (const Error& e) {
    return e.code() == code;
  }
  return false;
}

std::filesystem::path temp_file(const std::string& name) {
  return std::filesystem::temp_directory_path() / ("aesp_test_" + name);
}

}  // namespace

// ------------------------------------------------------------------ AES core
TEST_CASE("S-box: table and computed agree, is a bijection, no fixed points", "[aes][sbox]") {
  std::array<bool, 256> seen{};
  for (int x = 0; x < 256; ++x) {
    const uint8_t b = static_cast<uint8_t>(x);
    REQUIRE(sbox_table(b) == sbox_computed(b));
    REQUIRE(inv_sbox_computed(sbox_table(b)) == b);
    REQUIRE(sbox_table(b) != b);
    seen[sbox_table(b)] = true;
  }
  REQUIRE(std::all_of(seen.begin(), seen.end(), [](bool v) { return v; }));
  REQUIRE(sbox_table(0x53) == 0xED);  // FIPS-197 §5.1.1 example
}

TEST_CASE("KeyExpansion matches FIPS-197 Appendix A.1", "[aes][keyschedule]") {
  const Bytes key = H("2b7e151628aed2a6abf7158809cf4f3c");
  for (Backend be : kAll) {
    Aes128 aes(key.data(), key.size(), be);
    const auto& rk = aes.round_keys();
    REQUIRE(to_hex(rk.data() + 16, 16) == "a0fafe1788542cb123a339392a6c7605");   // w4..w7
    REQUIRE(to_hex(rk.data() + 160, 16) == "d014f9a8c9ee2589e13f0cc8b6630ca6");  // w40..w43
  }
}

TEST_CASE("FIPS-197 Appendix C.1 encrypt/decrypt on every backend", "[aes][kat]") {
  const Bytes key = H("000102030405060708090a0b0c0d0e0f"), pt = H("00112233445566778899aabbccddeeff");
  const std::string ct = "69c4e0d86a7b0430d8cdb78070b4c55a";
  for (Backend be : kAll) {
    Aes128 aes(key.data(), key.size(), be);
    uint8_t out[16], back[16];
    aes.encrypt_block(pt.data(), out);
    REQUIRE(to_hex(out, 16) == ct);
    aes.decrypt_block(out, back);
    REQUIRE(to_hex(back, 16) == to_hex(pt));
  }
}

TEST_CASE("In-place block operations work (in == out)", "[aes]") {
  const Bytes key = H("2b7e151628aed2a6abf7158809cf4f3c");
  Bytes blk = H("3243f6a8885a308d313198a2e0370734");
  for (Backend be : kAll) {
    Aes128 aes(key.data(), key.size(), be);
    Bytes b = blk;
    aes.encrypt_block(b.data(), b.data());
    REQUIRE(to_hex(b) == "3925841d02dc09fbdc118597196a0b32");
    aes.decrypt_block(b.data(), b.data());
    REQUIRE(b == blk);
  }
}

TEST_CASE("Random blocks: all backends agree and decrypt inverts encrypt", "[aes]") {
  for (int i = 0; i < 20; ++i) {
    const Bytes key = random_bytes(16), pt = random_bytes(16);
    Aes128 a(key.data(), 16, Backend::SBoxTable), b(key.data(), 16, Backend::SBoxComputed),
        c(key.data(), 16, Backend::TTable);
    uint8_t x[16], y[16], z[16], d[16];
    a.encrypt_block(pt.data(), x);
    b.encrypt_block(pt.data(), y);
    c.encrypt_block(pt.data(), z);
    REQUIRE(std::equal(x, x + 16, y));
    REQUIRE(std::equal(x, x + 16, z));
    c.decrypt_block(z, d);
    REQUIRE(std::equal(d, d + 16, pt.begin()));
  }
}

TEST_CASE("Invalid key length is rejected (fail closed)", "[aes][negative]") {
  for (std::size_t n : {0u, 1u, 15u, 17u, 24u, 32u}) {
    Bytes k(n, 0x11);
    REQUIRE(throws_code([&] { Aes128 a(k.data(), k.size()); }, ExitCode::InvalidInput));
  }
}

// ------------------------------------------------------------------ KAT file
TEST_CASE("KAT runner passes on the shipped vector file", "[kat]") {
  std::ostringstream os;
  const KatSummary s = run_kat_file(AESP_KAT_FILE, false, os);
  INFO(os.str());
  REQUIRE(s.total >= 20);
  REQUIRE(s.failed == 0);
  REQUIRE(s.ok());
}

TEST_CASE("KAT runner detects a wrong expected value and malformed files", "[kat][negative]") {
  const auto p = temp_file("bad_kat.json");
  {
    std::ofstream f(p);
    f << R"({"aes128_block":[{"id":"bad","source":"x","key":"000102030405060708090a0b0c0d0e0f",
        "plaintext":"00112233445566778899aabbccddeeff","ciphertext":"00000000000000000000000000000000"}]})";
  }
  std::ostringstream os;
  const KatSummary s = run_kat_file(p.string(), false, os);
  REQUIRE(s.failed == 1);
  REQUIRE_FALSE(s.ok());

  {
    std::ofstream f(p, std::ios::trunc);
    f << R"({"aes128_block":[{"id":"x","key":"zz","plaintext":"","ciphertext":""}]})";
  }
  REQUIRE(throws_code([&] { std::ostringstream o; run_kat_file(p.string(), false, o); }, ExitCode::InvalidInput));
  {
    std::ofstream f(p, std::ios::trunc);
    f << "{ not json";
  }
  REQUIRE(throws_code([&] { std::ostringstream o; run_kat_file(p.string(), false, o); }, ExitCode::InvalidInput));
  {
    std::ofstream f(p, std::ios::trunc);
    f << "{}";
  }
  std::ostringstream o2;
  REQUIRE_FALSE(run_kat_file(p.string(), false, o2).ok());  // zero tests must not count as success
  std::filesystem::remove(p);
}

// ------------------------------------------------------------------ CTR
TEST_CASE("NIST SP 800-38A F.5.1 CTR-AES128 (4 blocks, all backends)", "[ctr][kat]") {
  const Bytes key = H("2b7e151628aed2a6abf7158809cf4f3c"), iv = H("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
  const Bytes pt = H("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51"
                     "30c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710");
  const Bytes ct = H("874d6191b620e3261bef6864990db6ce9806f66b7970fdff8617187bb9fffdff"
                     "5ae4df3edbd5d35e5b4f09020db03eab1e031dda2fbe03d1792170a0f3009cee");
  for (Backend be : kAll) {
    Aes128 aes(key.data(), key.size(), be);
    REQUIRE(ctr_crypt(aes, iv.data(), pt) == ct);
    REQUIRE(ctr_crypt(aes, iv.data(), ct) == pt);
  }
}

TEST_CASE("CTR: arbitrary length, chunking and empty input", "[ctr]") {
  const Bytes key = random_bytes(16), iv = random_bytes(16);
  Aes128 aes(key.data(), key.size(), Backend::TTable);
  REQUIRE(ctr_crypt(aes, iv.data(), Bytes{}).empty());
  for (std::size_t len : {1u, 15u, 16u, 17u, 31u, 100u, 1000u}) {
    const Bytes pt = random_bytes(len);
    const Bytes ct = ctr_crypt(aes, iv.data(), pt);
    REQUIRE(ct.size() == len);  // no padding
    REQUIRE(ctr_crypt(aes, iv.data(), ct) == pt);
    for (std::size_t chunk : {1u, 5u, 16u, 33u}) {
      CtrStream s(aes, iv.data());
      Bytes out(len);
      for (std::size_t off = 0; off < len; off += chunk)
        s.process(pt.data() + off, out.data() + off, std::min<std::size_t>(chunk, len - off));
      REQUIRE(out == ct);
    }
  }
}

TEST_CASE("CTR counter is a 128-bit big-endian integer with carry", "[ctr][counter]") {
  const Bytes key = random_bytes(16);
  Aes128 aes(key.data(), key.size());
  const Bytes iv = H("0000000000000000000000000000ffff");  // ...ffff -> ...010000 -> ...010001
  const Bytes ks = ctr_crypt(aes, iv.data(), Bytes(48, 0));  // keystream = AES(counter blocks)
  const char* ctrs[3] = {"0000000000000000000000000000ffff", "00000000000000000000000000010000",
                         "00000000000000000000000000010001"};
  for (int i = 0; i < 3; ++i) {
    Bytes blk = H(ctrs[i]), expect(16);
    aes.encrypt_block(blk.data(), expect.data());
    REQUIRE(std::equal(expect.begin(), expect.end(), ks.begin() + 16 * i));
  }
  uint8_t c[16];
  std::fill(c, c + 16, static_cast<uint8_t>(0xFF));
  REQUIRE(ctr_increment(c));  // wraps to zero
  REQUIRE(std::all_of(c, c + 16, [](uint8_t v) { return v == 0; }));
}

TEST_CASE("CTR counter overflow is detected and fails closed", "[ctr][counter][negative]") {
  const Bytes key = random_bytes(16);
  Aes128 aes(key.data(), key.size());
  const Bytes ff(16, 0xFF);
  Bytes one(16, 0), two(17, 0);
  REQUIRE_NOTHROW(ctr_crypt(aes, ff.data(), one));  // last valid block
  REQUIRE(throws_code([&] { ctr_crypt(aes, ff.data(), two); }, ExitCode::CounterOverflow));
  Bytes fe(16, 0xFF);
  fe[15] = 0xFE;
  REQUIRE_NOTHROW(ctr_crypt(aes, fe.data(), Bytes(32, 0)));
  REQUIRE(throws_code([&] { ctr_crypt(aes, fe.data(), Bytes(33, 0)); }, ExitCode::CounterOverflow));
}

TEST_CASE("Negative: wrong key / wrong IV give wrong plaintext", "[ctr][negative]") {
  const Bytes key = random_bytes(16), key2 = random_bytes(16), iv = random_bytes(16), iv2 = random_bytes(16);
  const Bytes pt = random_bytes(200);
  Aes128 a(key.data(), 16), wrong(key2.data(), 16);
  const Bytes ct = ctr_crypt(a, iv.data(), pt);
  REQUIRE(ctr_crypt(wrong, iv.data(), ct) != pt);
  REQUIRE(ctr_crypt(a, iv2.data(), ct) != pt);
  REQUIRE(ctr_crypt(a, iv.data(), ct) == pt);
}

TEST_CASE("Negative: tampering is NOT detected; bit flips are malleable (CTR has no integrity)", "[ctr][negative]") {
  const Bytes key = random_bytes(16), iv = random_bytes(16);
  Aes128 aes(key.data(), 16);
  const Bytes pt = random_bytes(64);
  Bytes ct = ctr_crypt(aes, iv.data(), pt);
  ct[20] ^= 0x04;  // attacker flips one ciphertext bit
  const Bytes dec = ctr_crypt(aes, iv.data(), ct);  // no error is raised...
  for (std::size_t i = 0; i < pt.size(); ++i)
    REQUIRE(dec[i] == (i == 20 ? static_cast<uint8_t>(pt[i] ^ 0x04) : pt[i]));  // ...and exactly that bit flips
}

TEST_CASE("Two-time pad: IV reuse leaks P1 xor P2", "[ctr][security]") {
  const Bytes key = random_bytes(16), iv = random_bytes(16);
  Aes128 aes(key.data(), 16);
  const std::string a = "ATTACK AT DAWN!!", b = "RETREAT AT NOON!";
  const Bytes c1 = ctr_crypt(aes, iv.data(), Bytes(a.begin(), a.end()));
  const Bytes c2 = ctr_crypt(aes, iv.data(), Bytes(b.begin(), b.end()));  // catastrophic reuse
  for (std::size_t i = 0; i < a.size(); ++i)
    REQUIRE((c1[i] ^ c2[i]) == (static_cast<uint8_t>(a[i]) ^ static_cast<uint8_t>(b[i])));
}

// ------------------------------------------------------------------ file streaming
TEST_CASE("Streaming file encrypt/decrypt round trip (odd size > chunk) and failure cleanup", "[ctr][file]") {
  namespace fs = std::filesystem;
  const fs::path in = temp_file("in.bin"), enc = temp_file("enc.bin"), dec = temp_file("dec.bin");
  fs::remove(enc);
  fs::remove(dec);
  const Bytes data = random_bytes((1u << 20) * 2 + 12345);
  write_file(in.string(), data.data(), data.size(), true);
  const Bytes key = random_bytes(16), iv = random_bytes(16);
  Aes128 aes(key.data(), 16, Backend::TTable);
  ctr_crypt_file(aes, iv.data(), in.string(), enc.string(), false);
  ctr_crypt_file(aes, iv.data(), enc.string(), dec.string(), false);
  REQUIRE(read_file(dec.string(), 1u << 30) == data);
  REQUIRE(read_file(enc.string(), 1u << 30) == ctr_crypt(aes, iv.data(), data));

  // refuses to overwrite, refuses in==out, missing input -> I/O error and no partial output
  REQUIRE(throws_code([&] { ctr_crypt_file(aes, iv.data(), in.string(), enc.string(), false); }, ExitCode::Io));
  REQUIRE(throws_code([&] { ctr_crypt_file(aes, iv.data(), in.string(), in.string(), true); }, ExitCode::InvalidInput));
  const fs::path missing = temp_file("missing.bin"), out2 = temp_file("out2.bin");
  fs::remove(out2);
  REQUIRE(throws_code([&] { ctr_crypt_file(aes, iv.data(), missing.string(), out2.string(), false); }, ExitCode::Io));
  REQUIRE_FALSE(fs::exists(out2));

  // overflow in the middle of a stream must not leave output behind
  const Bytes ff(16, 0xFF);
  Aes128 aes2(key.data(), 16);
  REQUIRE(throws_code([&] { ctr_crypt_file(aes2, ff.data(), in.string(), out2.string(), true); }, ExitCode::CounterOverflow));
  REQUIRE_FALSE(fs::exists(out2));
  REQUIRE_FALSE(fs::exists(fs::path(out2.string() + ".tmp")));
  for (const auto& p : {in, enc, dec}) fs::remove(p);
}

// ------------------------------------------------------------------ codecs / parsers / stats
TEST_CASE("Hex and base64 codecs: round trip and strict rejection", "[util][negative]") {
  const Bytes b = random_bytes(37);
  REQUIRE(from_hex(to_hex(b)) == b);
  REQUIRE(from_base64(to_base64(b.data(), b.size())) == b);
  REQUIRE(to_base64(reinterpret_cast<const uint8_t*>("Man"), 3) == "TWFu");
  REQUIRE(to_base64(reinterpret_cast<const uint8_t*>("Ma"), 2) == "TWE=");
  REQUIRE(to_base64(reinterpret_cast<const uint8_t*>("M"), 1) == "TQ==");
  for (const char* bad : {"abc", "zz", "0g", " 00"}) REQUIRE(throws_code([&] { from_hex(bad); }, ExitCode::InvalidInput));
  for (const char* bad : {"TWF", "TW=u", "T===", "TWFu!", "=WFu", "TQ==TQ=="})
    REQUIRE(throws_code([&] { from_base64(bad); }, ExitCode::InvalidInput));
  REQUIRE_THROWS_AS(parse_encoding("utf16"), Error);
}

TEST_CASE("Key/IV files must be exactly 16 bytes", "[util][negative]") {
  const auto p = temp_file("key.bin");
  for (std::size_t n : {0u, 15u, 17u, 32u}) {
    const Bytes b(n, 0x42);
    write_file(p.string(), b.data(), b.size(), true);
    REQUIRE(throws_code([&] { read_file_exact(p.string(), 16, "key"); }, ExitCode::InvalidInput));
  }
  const Bytes ok(16, 0x42);
  write_file(p.string(), ok.data(), ok.size(), true);
  REQUIRE(read_file_exact(p.string(), 16, "key") == ok);
  REQUIRE(throws_code([&] { read_file_exact(temp_file("nope").string(), 16, "key"); }, ExitCode::Io));
  std::filesystem::remove(p);
}

TEST_CASE("Secure RNG returns fresh bytes", "[util][rng]") {
  const Bytes a = random_bytes(32), b = random_bytes(32);
  REQUIRE(a != b);
  REQUIRE(a != Bytes(32, 0));
}

TEST_CASE("parse_size", "[util]") {
  REQUIRE(parse_size("1MiB") == (1u << 20));
  REQUIRE(parse_size("16") == 16);
  REQUIRE(parse_size("2GiB") == (2ull << 30));
  REQUIRE_THROWS_AS(parse_size("1MB"), Error);
  REQUIRE_THROWS_AS(parse_size("-1"), Error);
  REQUIRE_THROWS_AS(parse_size(""), Error);
}

TEST_CASE("JSON parser: valid and malformed", "[json][negative]") {
  const Json j = parse_json(R"({"a":[1,2.5,true,null],"s":"x\u00e9\n","o":{}})");
  REQUIRE(j.at("a").array.size() == 4);
  REQUIRE(j.str("s") == "x\xC3\xA9\n");
  for (const char* bad : {"", "{", "{\"a\":}", "[1,]", "{\"a\":1,\"a\":2}", "{} x", "\"\\ud800\"", "[01]", "{\"a\" 1}"})
    REQUIRE(throws_code([&] { parse_json(bad); }, ExitCode::InvalidInput));
  std::string deep(200, '[');
  REQUIRE(throws_code([&] { parse_json(deep); }, ExitCode::InvalidInput));
}

TEST_CASE("Statistics: mean, median, stddev, 95% CI", "[stats]") {
  const Stats s = compute_stats({2, 4, 4, 4, 5, 5, 7, 9});
  REQUIRE(s.mean == 5.0);
  REQUIRE(s.median == 4.5);
  REQUIRE(s.stddev > 2.13);
  REQUIRE(s.stddev < 2.14);
  REQUIRE(s.ci95_lo < s.mean);
  REQUIRE(s.ci95_hi > s.mean);
  REQUIRE(t_critical_95(29) == 2.045);
}

// ------------------------------------------------------------------ cross-validation (optional libs)
TEST_CASE("Cross-validation against reference libraries (if compiled in)", "[xval]") {
  const auto refs = available_references("all");
  if (refs.empty()) SUCCEED("no reference library available in this build");
  std::ostringstream os;
  if (!refs.empty()) {
    const XvalSummary s = run_xval("all", 20, os);
    INFO(os.str());
    REQUIRE(s.total > 0);
    REQUIRE(s.failed == 0);
  }
}

// ------------------------------------------------------------------ BONUS: AES-192 / AES-256
namespace {
std::vector<Backend> bonus_backends() {
  std::vector<Backend> v(kAll.begin(), kAll.end());
  if (aesni::available()) v.push_back(Backend::AesNi);
  return v;
}
}  // namespace

TEST_CASE("BONUS FIPS-197 Appendix C.2 / C.3 (AES-192, AES-256) on every backend", "[bonus][aes][kat]") {
  const Bytes pt = H("00112233445566778899aabbccddeeff");
  struct V { const char* key; const char* ct; };
  const V vs[] = {{"000102030405060708090a0b0c0d0e0f1011121314151617", "dda97ca4864cdfe06eaf70a0ec0d7191"},
                  {"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", "8ea2b7ca516745bfeafc49904b496089"}};
  for (const V& v : vs)
    for (Backend be : bonus_backends()) {
      const Bytes key = H(v.key);
      Aes aes(key.data(), key.size(), be);
      uint8_t out[16];
      aes.encrypt_block(pt.data(), out);
      REQUIRE(to_hex(out, 16) == v.ct);
      aes.decrypt_block(out, out);
      REQUIRE(to_hex(out, 16) == to_hex(pt.data(), 16));
    }
}

TEST_CASE("BONUS key schedule: FIPS-197 A.2 (AES-192) and A.3 (AES-256) round keys", "[bonus][aes][keyschedule]") {
  {  // A.2: key 8e73b0f7..., w[6] = fe0c91f7, last word w[51] = 01002202
    const Bytes key = H("8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b");
    Aes aes(key.data(), key.size());
    REQUIRE(aes.rounds() == 12);
    const auto& rk = aes.round_keys();
    REQUIRE(to_hex(rk.data() + 24, 4) == "fe0c91f7");
    REQUIRE(to_hex(rk.data() + 4 * 51, 4) == "01002202");
  }
  {  // A.3: key 603deb10..., w[8] = 9ba35411, w[12] = a8b09c1a (extra SubWord), last word w[59] = 706c631e
    const Bytes key = H("603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4");
    Aes aes(key.data(), key.size());
    REQUIRE(aes.rounds() == 14);
    const auto& rk = aes.round_keys();
    REQUIRE(to_hex(rk.data() + 32, 4) == "9ba35411");
    REQUIRE(to_hex(rk.data() + 48, 4) == "a8b09c1a");
    REQUIRE(to_hex(rk.data() + 4 * 59, 4) == "706c631e");
  }
}

TEST_CASE("BONUS Aes accepts only 16/24/32-byte keys; Aes128 stays strictly 16", "[bonus][aes][negative]") {
  for (std::size_t n : {0u, 1u, 15u, 17u, 23u, 25u, 31u, 33u, 64u}) {
    Bytes k(n, 0x11);
    REQUIRE(throws_code([&] { Aes a(k.data(), k.size()); }, ExitCode::InvalidInput));
  }
  for (std::size_t n : {16u, 24u, 32u}) {
    Bytes k(n, 0x11);
    REQUIRE_NOTHROW(Aes(k.data(), k.size()));
  }
  Bytes k24(24, 0x11);
  REQUIRE(throws_code([&] { Aes128 a(k24.data(), k24.size()); }, ExitCode::InvalidInput));
}

TEST_CASE("BONUS NIST SP 800-38A F.5.3 / F.5.5 CTR-AES192/256 (4 blocks, all backends)", "[bonus][ctr][kat]") {
  const Bytes iv = H("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
  const Bytes pt = H("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e5130c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710");
  const Bytes k192 = H("8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b");
  const Bytes k256 = H("603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4");
  const std::string c192 = "1abc932417521ca24f2b0459fe7e6e0b090339ec0aa6faefd5ccc2c6f4ce8e941e36b26bd1ebc670d1bd1d665620abf74f78a7f6d29809585a97daec58c6b050";
  const std::string c256 = "601ec313775789a5b7a7f504bbf3d228f443e3ca4d62b59aca84e990cacaf5c52b0930daa23de94ce87017ba2d84988ddfc9c58db67aada613c2dd08457941a6";
  for (Backend be : bonus_backends()) {
    Aes a(k192.data(), k192.size(), be), b(k256.data(), k256.size(), be);
    REQUIRE(to_hex(ctr_crypt(a, iv.data(), pt)) == c192);
    REQUIRE(to_hex(ctr_crypt(b, iv.data(), pt)) == c256);
  }
}

// ------------------------------------------------------------------ BONUS: AES-NI
TEST_CASE("BONUS AES-NI agrees with the software implementation (random keys/blocks, 128/192/256)", "[bonus][aesni]") {
  if (!aesni::available()) {
    SUCCEED("AES-NI not available on this CPU: backend correctly reported as unavailable");
    const Bytes key(16, 0x42);
    REQUIRE(throws_code([&] { Aes a(key.data(), key.size(), Backend::AesNi); }, ExitCode::InvalidInput));
    return;
  }
  for (std::size_t klen : {16u, 24u, 32u})
    for (int i = 0; i < 200; ++i) {
      const Bytes key = random_bytes(klen), pt = random_bytes(16);
      Aes sw(key.data(), klen, Backend::SBoxTable), hw(key.data(), klen, Backend::AesNi);
      uint8_t a[16], b[16], d[16];
      sw.encrypt_block(pt.data(), a);
      hw.encrypt_block(pt.data(), b);
      REQUIRE(to_hex(a, 16) == to_hex(b, 16));
      hw.decrypt_block(b, d);
      REQUIRE(to_hex(d, 16) == to_hex(pt.data(), 16));
    }
}

// ------------------------------------------------------------------ BONUS: XTS-AES
TEST_CASE("BONUS XTS-AES: IEEE 1619-2007 vectors 1, 2, 3 and 15 (ciphertext stealing)", "[bonus][xts][kat]") {
  struct S { std::string key, tweak, pt, ct; bool equal_keys; };
  const std::string t33 = "33333333330000000000000000000000", p44(64, '4');
  const std::vector<S> sv = {
      {std::string(64, '0'), std::string(32, '0'), std::string(64, '0'),
       "917cf69ebd68b2ec9b9fe9a3eadda692cd43d2f59598ed858c02c2652fbf922e", true},
      {std::string(32, '1') + std::string(32, '2'), t33, p44,
       "c454185e6a16936e39334038acef838bfb186fff7480adc4289382ecd6d394f0", false},
      {"fffefdfcfbfaf9f8f7f6f5f4f3f2f1f0" + std::string(32, '2'), t33, p44,
       "af85336b597afc1a900b2eb21ec949d292df4c047e0b21532186a5971a227a89", false},
      {"fffefdfcfbfaf9f8f7f6f5f4f3f2f1f0bfbebdbcbbbab9b8b7b6b5b4b3b2b1b0", "9a785634120000000000000000000000",
       "000102030405060708090a0b0c0d0e0f10", "6c1625db4671522d3d7599601de7ca09ed", false},
  };
  for (const S& v : sv)
    for (Backend be : bonus_backends()) {
      const Bytes key = H(v.key), tw = H(v.tweak), pt = H(v.pt), ct = H(v.ct);
      const Xts x(key.data(), key.size(), be, v.equal_keys);
      Bytes out(pt.size());
      x.encrypt(tw.data(), pt.data(), out.data(), pt.size());
      REQUIRE(out == ct);
      x.decrypt(tw.data(), ct.data(), out.data(), ct.size());
      REQUIRE(out == pt);
    }
}

TEST_CASE("BONUS XTS-AES: round trip for every length 16..130 (in place too), tweak matters", "[bonus][xts]") {
  for (std::size_t klen : {32u, 64u}) {
    const Bytes key = random_bytes(klen), tweak = random_bytes(16);
    const Xts x(key.data(), klen);
    for (std::size_t len = 16; len <= 130; ++len) {
      const Bytes pt = random_bytes(len);
      Bytes ct(len), back(len);
      x.encrypt(tweak.data(), pt.data(), ct.data(), len);
      REQUIRE(ct != pt);
      x.decrypt(tweak.data(), ct.data(), back.data(), len);
      REQUIRE(back == pt);
      Bytes inplace = pt;  // in == out
      x.encrypt(tweak.data(), inplace.data(), inplace.data(), len);
      REQUIRE(inplace == ct);
      x.decrypt(tweak.data(), inplace.data(), inplace.data(), len);
      REQUIRE(inplace == pt);
    }
    const Bytes pt = random_bytes(48);
    Bytes c1(48), c2(48);
    Bytes tweak2 = tweak;
    tweak2[0] ^= 1;
    x.encrypt(tweak.data(), pt.data(), c1.data(), 48);
    x.encrypt(tweak2.data(), pt.data(), c2.data(), 48);
    REQUIRE(c1 != c2);
  }
}

TEST_CASE("BONUS XTS-AES: GF(2^128) multiply-by-alpha (IEEE 1619 section 5.2)", "[bonus][xts]") {
  uint8_t t[16] = {1};  // little-endian 1 -> x
  xts_mul_alpha(t);
  REQUIRE(t[0] == 2);
  uint8_t u[16] = {0};
  u[15] = 0x80;  // top bit set: carry folds in 0x87
  xts_mul_alpha(u);
  REQUIRE(u[0] == 0x87);
  REQUIRE(u[15] == 0x00);
}

TEST_CASE("BONUS XTS-AES: fails closed on bad keys and data-unit sizes", "[bonus][xts][negative]") {
  for (std::size_t n : {0u, 16u, 24u, 31u, 33u, 48u, 63u, 65u})
    REQUIRE(throws_code([&] { Bytes k(n, 0x5A); Xts x(k.data(), k.size()); }, ExitCode::InvalidInput));
  const Bytes same(32, 0x7E);  // K1 == K2 is forbidden (IEEE 1619-2018 / SP 800-38E)
  REQUIRE(throws_code([&] { Xts x(same.data(), same.size()); }, ExitCode::InvalidInput));
  const Bytes key = random_bytes(32), tw = random_bytes(16);
  const Xts x(key.data(), key.size());
  Bytes buf(32);
  for (std::size_t len : {0u, 1u, 15u})
    REQUIRE(throws_code([&] { x.encrypt(tw.data(), buf.data(), buf.data(), len); }, ExitCode::InvalidInput));
  REQUIRE(throws_code([&] { x.decrypt(tw.data(), buf.data(), buf.data(), kXtsMaxDataUnit + 1); }, ExitCode::InvalidInput));
}
