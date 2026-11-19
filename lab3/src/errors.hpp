// Error type and process exit codes. Every failure is reported by throwing rsalab::Error; main() turns it into a short,
// non-leaky message and a non-zero exit code (fail closed).
#pragma once
#include <stdexcept>
#include <string>

namespace rsalab {

enum class ExitCode : int {
  Ok = 0,
  Usage = 1,     // bad command line / unsupported parameter
  Io = 2,        // file I/O problem, unreadable or unsupported key file
  Crypto = 3,    // decryption / authentication failure (ONE generic message for every cause)
  KatFailed = 4  // a Known Answer Test failed
};

class Error : public std::runtime_error {
 public:
  Error(ExitCode code, const std::string& msg) : std::runtime_error(msg), code_(code) {}
  ExitCode code() const noexcept { return code_; }

 private:
  ExitCode code_;
};

[[noreturn]] inline void usage_error(const std::string& m) { throw Error(ExitCode::Usage, m); }
[[noreturn]] inline void io_error(const std::string& m) { throw Error(ExitCode::Io, m); }

// The only message ever shown for a failed decryption: it must not reveal WHY it failed (padding, label, tag, header ...).
inline constexpr const char* kDecryptFailedMessage = "decryption failed.";
[[noreturn]] inline void crypto_failure() { throw Error(ExitCode::Crypto, kDecryptFailedMessage); }

}  // namespace rsalab
