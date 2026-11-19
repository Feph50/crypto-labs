// Command-line front end of rsatool.
#pragma once
#include <string>
#include <vector>

namespace rsalab {

// argv[0] is the program name. Returns the process exit code (0 ok, 1 usage, 2 I/O or key file, 3 decryption failure, 4 KAT failure).
int run_cli(const std::vector<std::string>& argv);

}  // namespace rsalab
