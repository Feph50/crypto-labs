#include <string>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

#include "cli.hpp"

static std::vector<std::string> get_args(int argc, char** argv) {
  std::vector<std::string> v;
#if defined(_WIN32)
  (void)argc;
  (void)argv;
  int n = 0;
  LPWSTR* w = CommandLineToArgvW(GetCommandLineW(), &n);  // real UTF-16 -> UTF-8 (not the ANSI code page)
  if (w) {
    for (int i = 0; i < n; ++i) {
      const int len = WideCharToMultiByte(CP_UTF8, 0, w[i], -1, nullptr, 0, nullptr, nullptr);
      std::string s(static_cast<std::size_t>(len > 0 ? len - 1 : 0), '\0');
      if (len > 1) WideCharToMultiByte(CP_UTF8, 0, w[i], -1, &s[0], len, nullptr, nullptr);
      v.push_back(s);
    }
    LocalFree(w);
    return v;
  }
#endif
  for (int i = 0; i < argc; ++i) v.emplace_back(argv[i]);
  return v;
}

int main(int argc, char** argv) { return rsalab::run_cli(get_args(argc, argv)); }
