// Small utilities: byte buffers, secure wiping, atomic file I/O, UTC time.
#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace rsalab {

using Bytes = std::vector<uint8_t>;

// Overwrites memory in a way the optimiser may not remove.
void secure_zero(void* p, std::size_t n) noexcept;
inline void secure_zero(Bytes& b) noexcept {
  if (!b.empty()) secure_zero(b.data(), b.size());
}

// Wipes a buffer when the scope ends (keys, plaintext temporaries).
struct WipeOnExit {
  Bytes& b;
  explicit WipeOnExit(Bytes& buf) : b(buf) {}
  ~WipeOnExit() { secure_zero(b); }
  WipeOnExit(const WipeOnExit&) = delete;
  WipeOnExit& operator=(const WipeOnExit&) = delete;
};

inline Bytes to_bytes(const std::string& s) { return Bytes(s.begin(), s.end()); }
inline std::string to_string(const Bytes& b) { return std::string(b.begin(), b.end()); }

// UTF-8 path -> std::filesystem::path (non-ASCII names work on Windows).
std::filesystem::path to_path(const std::string& utf8);
bool file_exists(const std::string& path);

// Reads a whole file (binary). Throws Error(Io) when it cannot be read or is larger than max_bytes.
Bytes read_file(const std::string& path, std::size_t max_bytes);

// Writes via "<path>.tmp" then renames, so a failed write never leaves a truncated file; refuses to replace an existing
// file unless `overwrite`. With `private_file` the permissions are 0600 on POSIX (no-op on Windows).
void write_file(const std::string& path, const uint8_t* data, std::size_t n, bool overwrite, bool private_file = false);
inline void write_file(const std::string& path, const Bytes& b, bool overwrite, bool private_file = false) {
  write_file(path, b.data(), b.size(), overwrite, private_file);
}

// "pub.pem" + ".der" -> "pub.der"; a path without ".pem" simply gets the extension appended ("key" -> "key.der").
std::string path_with_extension(const std::string& path, const std::string& new_ext);

// "2026-11-19T14:04:00Z"
std::string utc_now_iso8601();

}  // namespace rsalab
