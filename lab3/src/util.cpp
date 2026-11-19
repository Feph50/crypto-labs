#include "util.hpp"

#include <cstdio>
#include <ctime>
#include <fstream>
#include <system_error>

#include "errors.hpp"

namespace rsalab {

namespace fs = std::filesystem;

void secure_zero(void* p, std::size_t n) noexcept {
  volatile uint8_t* v = static_cast<volatile uint8_t*>(p);
  while (n--) *v++ = 0;
}

fs::path to_path(const std::string& utf8) {
#if defined(_WIN32)
  return fs::u8path(utf8);
#else
  return fs::path(utf8);
#endif
}

bool file_exists(const std::string& path) {
  std::error_code ec;
  return fs::is_regular_file(to_path(path), ec);
}

Bytes read_file(const std::string& path, std::size_t max_bytes) {
  std::ifstream f(to_path(path), std::ios::binary);
  if (!f) io_error("cannot open file '" + path + "'");
  f.seekg(0, std::ios::end);
  const std::streamoff size = f.tellg();
  if (size < 0) io_error("cannot determine the size of '" + path + "'");
  if (static_cast<unsigned long long>(size) > max_bytes) io_error("file '" + path + "' is too large");
  f.seekg(0, std::ios::beg);
  Bytes b(static_cast<std::size_t>(size));
  if (!b.empty()) f.read(reinterpret_cast<char*>(b.data()), size);
  if (!f) io_error("read error on '" + path + "'");
  return b;
}

void write_file(const std::string& path, const uint8_t* data, std::size_t n, bool overwrite, bool private_file) {
  const fs::path out = to_path(path);
  std::error_code ec;
  if (fs::exists(out, ec) && !overwrite) io_error("output file '" + path + "' already exists (use --force to replace it)");
  if (out.has_parent_path()) fs::create_directories(out.parent_path(), ec);
  fs::path tmp = out;
  tmp += ".tmp";
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f) io_error("cannot create output file '" + path + "'");
#if !defined(_WIN32)
    if (private_file) fs::permissions(tmp, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace, ec);
#else
    (void)private_file;
#endif
    if (n) f.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(n));
    f.flush();
    if (!f) {
      f.close();
      fs::remove(tmp, ec);
      io_error("write error on '" + path + "'");
    }
  }
  fs::rename(tmp, out, ec);
  if (ec) {
    fs::remove(tmp, ec);
    io_error("cannot finalize output file '" + path + "'");
  }
}

std::string path_with_extension(const std::string& path, const std::string& new_ext) {
  const std::string pem = ".pem";
  if (path.size() >= pem.size()) {
    std::string tail = path.substr(path.size() - pem.size());
    for (char& c : tail) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (tail == pem) return path.substr(0, path.size() - pem.size()) + new_ext;
  }
  return path + new_ext;
}

std::string utc_now_iso8601() {
  const std::time_t t = std::time(nullptr);
  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif
  char buf[32];
  std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &tm);
  return buf;
}

}  // namespace rsalab
