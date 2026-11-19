// Shared helpers for the unit tests: cached RSA keys (key generation is the slow part) and small utilities.
#pragma once
#include <filesystem>
#include <string>

#include "keyio.hpp"
#include "util.hpp"

namespace testsupport {

const rsalab::KeyPair& key3072();        // one RSA-3072 key shared by all tests
const rsalab::KeyPair& key3072_other();  // a second, different RSA-3072 key ("wrong key" tests)
const rsalab::KeyPair& key4096();        // RSA-4096 (slower to generate; created on first use)

rsalab::Bytes random_bytes(std::size_t n);  // from AutoSeededRandomPool
rsalab::Bytes bytes_of(const std::string& s);

// A fresh empty directory under the system temp directory (removed by the OS / next run).
std::filesystem::path temp_dir(const std::string& name);

}  // namespace testsupport
