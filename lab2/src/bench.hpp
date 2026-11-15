// Benchmark harness following the lab's performance protocol:
//   warm-up (1-2 s, for every case) -> N = 30..100 independent timed runs, each a block of ~1000 operations
//   -> mean / median / stddev / 95% CI of throughput (MB/s, 1 MB = 10^6 B) and latency (ms/op).
// Payloads, keys and IVs all come from the OS CSPRNG (BCryptGenRandom / getrandom).
#pragma once
#include <cstddef>
#include <ostream>
#include <string>
#include <vector>

#include "aes128.hpp"

using namespace std;

namespace aesp {

struct BenchOptions {
  std::vector<std::size_t> sizes = {1u << 20, 100u << 20};  // payload sizes in bytes
  std::vector<Backend> backends = {Backend::SBoxTable, Backend::TTable};
  std::string ref = "all";     // all | openssl | cryptopp | none
  std::size_t runs = 30;       // independent timed runs per case (>= 30 recommended)
  double warmup_sec = 1.5;     // 1-2 s per case
  double target_sec = 0.0;     // > 0: pick ops/run so one run lasts about this long (time based); 0 = use the data budget
  std::size_t budget_bytes = static_cast<std::size_t>(256) << 20;  // auto ops = min(1000, budget / payload size), >= 1
  std::size_t ops = 0;         // fixed ops per run (the protocol uses ~1000); 0 = auto (see budget_bytes)
  std::size_t key_bytes = 16;  // 16 / 24 / 32 -> AES-128 / AES-192 / AES-256 (CTR)
  std::size_t threads = 1;     // worker threads (each has a private buffer); throughput is the aggregate
  bool do_encrypt = true, do_decrypt = true;
  std::string csv_path;        // optional CSV output
};

void run_bench(const BenchOptions& opt, std::ostream& os);

}  // namespace aesp
