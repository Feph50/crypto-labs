#include "test_support.hpp"

#include <cstdio>

#include "cryptopp_all.hpp"

namespace testsupport {

static rsalab::KeyPair make(unsigned bits) {
  CryptoPP::AutoSeededRandomPool rng;
  return rsalab::generate_keypair(bits, rng);
}

const rsalab::KeyPair& key3072() {
  static const rsalab::KeyPair k = make(3072);
  return k;
}

const rsalab::KeyPair& key3072_other() {
  static const rsalab::KeyPair k = make(3072);
  return k;
}

const rsalab::KeyPair& key4096() {
  static const rsalab::KeyPair k = make(4096);
  return k;
}

rsalab::Bytes random_bytes(std::size_t n) {
  CryptoPP::AutoSeededRandomPool rng;
  rsalab::Bytes b(n);
  if (n) rng.GenerateBlock(b.data(), n);
  return b;
}

rsalab::Bytes bytes_of(const std::string& s) { return rsalab::Bytes(s.begin(), s.end()); }

std::filesystem::path temp_dir(const std::string& name) {
  namespace fs = std::filesystem;
  CryptoPP::AutoSeededRandomPool rng;
  uint8_t r[4];
  rng.GenerateBlock(r, sizeof r);
  char hex[9];
  for (int i = 0; i < 4; ++i) std::snprintf(hex + 2 * i, 3, "%02x", r[i]);
  const fs::path p = fs::temp_directory_path() / ("rsalab_" + name + "_" + hex);
  std::error_code ec;
  fs::remove_all(p, ec);
  fs::create_directories(p);
  return p;
}

}  // namespace testsupport
