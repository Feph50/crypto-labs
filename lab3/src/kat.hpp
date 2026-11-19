// Known Answer Tests from kat/vectors.json (built by scripts/make_kat.py from the official Project Wycheproof RSAES-OAEP
// vectors and the NIST CAVP AES-GCM vectors; see kat/SOURCES.md).
#pragma once
#include <cstddef>
#include <ostream>
#include <string>

namespace rsalab {

struct KatSummary {
  std::size_t total = 0, passed = 0, failed = 0;
  bool ok() const { return total > 0 && failed == 0; }
};

// Prints one PASS/FAIL line per test and a summary line. Throws Error(Io) if the file is missing or is not valid JSON
// (a broken KAT file must never report success).
KatSummary run_kat_file(const std::string& path, std::ostream& os);

}  // namespace rsalab
