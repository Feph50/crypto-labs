// rsabench - benchmark harness for Lab 3.
//
// Suites (all with the lab's protocol: warm-up, blocks of operations, N independent repetitions, one CSV row per repetition):
//   keygen   RSA key generation, 3072 and 4096 bits         (one key generation per repetition; random duration)
//   rsa      RSA-OAEP encrypt / decrypt of a 32-byte message, 3072 and 4096 bits
//   aesgcm   AES-256-GCM encrypt / decrypt throughput, 1 KiB / 1 MiB / 100 MiB
//   hybrid   hybrid envelope encrypt / decrypt (fresh AES key + AES-GCM + RSA-OAEP wrap/unwrap), 1 KiB / 1 MiB / 100 MiB, RSA-3072 and 4096
//
// Protocol: for every case a warm-up of --warmup seconds (at least one operation, results discarded); then --reps timed
// blocks. A block is `ops` operations, with ops chosen so that a block lasts about --target-sec seconds (at most --max-ops,
// at least 1); the CSV records the mean time per operation of the block. Keys, IVs and data come from AutoSeededRandomPool.
//
// CSV columns: timestamp_utc,suite,op,rsa_bits,size_bytes,rep,ops,time_ns_per_op,throughput_MBps   (MB = 10^6 bytes; only aesgcm/hybrid)
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "cryptopp_all.hpp"
#include "errors.hpp"
#include "hybrid.hpp"
#include "keyio.hpp"
#include "rsa_oaep.hpp"
#include "util.hpp"

#ifndef RSALAB_BUILD_TYPE
#define RSALAB_BUILD_TYPE "unknown"
#endif
#ifndef RSALAB_BUILD_FLAGS
#define RSALAB_BUILD_FLAGS ""
#endif

using namespace rsalab;
using Clock = std::chrono::steady_clock;

namespace {

struct Options {
  std::string csv, env_out;
  std::size_t reps = 30;
  std::size_t keygen_reps = 0;  // 0 = same as reps
  double warmup = 1.5, target = 1.5;
  std::size_t max_ops = 1000;
  std::vector<std::string> suites = {"keygen", "rsa", "aesgcm", "hybrid"};
  std::vector<unsigned> bits = {3072, 4096};
  std::vector<std::size_t> sizes = {1024, 1u << 20, 100u << 20};
  bool quick = false;
};

double seconds_since(Clock::time_point t0) { return std::chrono::duration<double>(Clock::now() - t0).count(); }

std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> v;
  std::stringstream ss(s);
  std::string t;
  while (std::getline(ss, t, ','))
    if (!t.empty()) v.push_back(t);
  return v;
}

std::size_t parse_size(const std::string& s) {
  std::size_t used = 0;
  const unsigned long long n = std::stoull(s, &used);
  const std::string unit = s.substr(used);
  if (unit.empty()) return static_cast<std::size_t>(n);
  if (unit == "KiB") return static_cast<std::size_t>(n << 10);
  if (unit == "MiB") return static_cast<std::size_t>(n << 20);
  if (unit == "GiB") return static_cast<std::size_t>(n << 30);
  throw std::invalid_argument("bad size '" + s + "' (use bytes, KiB, MiB or GiB)");
}

std::string size_label(std::size_t n) {
  if (n >= (1u << 20) && n % (1u << 20) == 0) return std::to_string(n >> 20) + " MiB";
  if (n >= 1024 && n % 1024 == 0) return std::to_string(n >> 10) + " KiB";
  return std::to_string(n) + " B";
}

struct Csv {
  std::ofstream f;
  void open(const std::string& path) {
    if (path.empty()) return;
    const auto p = to_path(path);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
    f.open(p, std::ios::trunc);
    if (!f) throw std::runtime_error("cannot create CSV file '" + path + "'");
    f << "timestamp_utc,suite,op,rsa_bits,size_bytes,rep,ops,time_ns_per_op,throughput_MBps\n";
  }
  void row(const std::string& suite, const std::string& op, unsigned bits, std::size_t size, std::size_t rep, std::size_t ops, double ns,
           bool with_throughput) {
    if (!f.is_open()) return;
    f << utc_now_iso8601() << ',' << suite << ',' << op << ',' << bits << ',' << size << ',' << rep << ',' << ops << ',';
    f.setf(std::ios::fixed);
    f.precision(1);
    f << ns << ',';
    if (with_throughput) {
      f.precision(3);
      f << (static_cast<double>(size) / (ns * 1e-9) / 1e6);
    }
    f << '\n';
    f.flush();
  }
};

struct Summary {
  double mean = 0, median = 0, sd = 0;
};
Summary summarize(std::vector<double> v) {
  Summary s;
  if (v.empty()) return s;
  double sum = 0;
  for (double x : v) sum += x;
  s.mean = sum / static_cast<double>(v.size());
  double ss = 0;
  for (double x : v) ss += (x - s.mean) * (x - s.mean);
  s.sd = v.size() > 1 ? std::sqrt(ss / static_cast<double>(v.size() - 1)) : 0;
  std::sort(v.begin(), v.end());
  s.median = v.size() % 2 ? v[v.size() / 2] : 0.5 * (v[v.size() / 2 - 1] + v[v.size() / 2]);
  return s;
}

volatile uint8_t g_sink = 0;  // keeps the optimiser from discarding the work

using Op = std::function<void()>;

// One benchmark case: warm-up, then `reps` blocks; returns the per-operation times (ns) of the blocks.
void run_case(Csv& csv, const Options& o, const std::string& suite, const std::string& op_name, unsigned bits, std::size_t size,
              bool throughput, const Op& op, std::size_t reps, bool single_op) {
  std::size_t ops = 1;
  if (!single_op) {
    std::size_t w = 0;
    const auto t0 = Clock::now();
    do {
      op();
      ++w;
    } while (seconds_since(t0) < o.warmup);
    const double est = seconds_since(t0) / static_cast<double>(w);
    ops = static_cast<std::size_t>(std::max(1.0, std::min(static_cast<double>(o.max_ops), std::floor(o.target / est))));
  } else {
    op();  // warm-up: one operation, discarded
  }
  std::vector<double> ns;
  for (std::size_t r = 0; r < reps; ++r) {
    const auto t0 = Clock::now();
    for (std::size_t i = 0; i < ops; ++i) op();
    const double per_op = seconds_since(t0) * 1e9 / static_cast<double>(ops);
    ns.push_back(per_op);
    csv.row(suite, op_name, bits, size, r + 1, ops, per_op, throughput);
  }
  const Summary s = summarize(ns);
  const std::string bits_label = bits ? "RSA-" + std::to_string(bits) : "-";
  std::printf("%-8s %-8s %-9s %-9s ops/block %-5zu mean %12.3f us  median %12.3f us  sd %10.3f us", suite.c_str(), op_name.c_str(),
              bits_label.c_str(), size ? size_label(size).c_str() : "-", ops, s.mean / 1e3, s.median / 1e3, s.sd / 1e3);
  if (throughput) std::printf("  %9.1f MB/s", static_cast<double>(size) / (s.mean * 1e-9) / 1e6);
  std::printf("\n");
  std::fflush(stdout);
}

std::string env_text() {
  std::ostringstream os;
  os << "# rsabench environment\n";
  const char* cpu = std::getenv("PROCESSOR_IDENTIFIER");
  std::string cpu_name;
#if defined(_WIN32)
  {
    char buf[256] = {0};
    DWORD len = sizeof buf;
    if (RegGetValueA(HKEY_LOCAL_MACHINE, R"(HARDWARE\DESCRIPTION\System\CentralProcessor\0)", "ProcessorNameString", RRF_RT_REG_SZ, nullptr, buf,
                     &len) == ERROR_SUCCESS)
      cpu_name = buf;
    if (!cpu_name.empty()) {
      os << "# CPU        : " << cpu_name << '\n';
      cpu = nullptr;
    }
  }
#endif
#if defined(__linux__)
  {
    std::ifstream f("/proc/cpuinfo");
    std::string line;
    while (std::getline(f, line))
      if (line.rfind("model name", 0) == 0) {
        cpu = nullptr;
        os << "# CPU        : " << line.substr(line.find(':') + 2) << "\n";
        break;
      }
  }
#endif
  if (cpu) os << "# CPU        : " << cpu << "\n";
  os << "# Threads    : " << std::thread::hardware_concurrency() << " logical\n";
#if defined(_WIN32)
  os << "# OS         : Windows\n";
#elif defined(__linux__)
  os << "# OS         : Linux\n";
#else
  os << "# OS         : other\n";
#endif
#if defined(_MSC_VER)
  os << "# Compiler   : MSVC " << _MSC_VER << "\n";
#elif defined(__clang__)
  os << "# Compiler   : Clang " << __clang_version__ << "\n";
#elif defined(__GNUC__)
  os << "# Compiler   : GCC " << __VERSION__ << "\n";
#endif
  os << "# Build      : " << RSALAB_BUILD_TYPE << "  flags: " << RSALAB_BUILD_FLAGS << "\n";
  os << "# Crypto++   : " << CRYPTOPP_VERSION / 100 << "." << (CRYPTOPP_VERSION / 10) % 10 << "." << CRYPTOPP_VERSION % 10;
#if CRYPTOPP_BOOL_X86 || CRYPTOPP_BOOL_X32 || CRYPTOPP_BOOL_X64
  os << "  (AES-NI " << (CryptoPP::HasAESNI() ? "yes" : "no") << ", CLMUL " << (CryptoPP::HasCLMUL() ? "yes" : "no") << ", SSE4.1 "
     << (CryptoPP::HasSSE41() ? "yes" : "no") << ")";
#endif
  os << "\n";
  os << "# Reminder   : set the CPU governor to 'performance' (Linux) / 'High performance' (Windows) for fair runs\n";
  return os.str();
}

const char* kUsage = R"(rsabench - Lab 3 benchmark: RSA key generation, RSA-OAEP, AES-256-GCM, hybrid envelope

USAGE
  rsabench --out results.csv [--env-out env.txt] [--reps 30] [--keygen-reps N] [--warmup 1.5] [--target-sec 1.5] [--max-ops 1000]
           [--suites keygen,rsa,aesgcm,hybrid] [--bits 3072,4096] [--sizes 1KiB,1MiB,100MiB] [--quick]
  --quick  smoke test: 3 repetitions, short warm-up, sizes 1KiB,1MiB.
Protocol: warm-up per case, then --reps blocks of ops operations (a block lasts ~--target-sec, at most --max-ops operations);
key generation is timed one key per repetition. One CSV row per repetition.
)";

}  // namespace

int main(int argc, char** argv) {
  Options o;
  try {
    for (int i = 1; i < argc; ++i) {
      const std::string a = argv[i];
      auto next = [&]() -> std::string {
        if (i + 1 >= argc) throw std::invalid_argument("option " + a + " needs a value");
        return argv[++i];
      };
      if (a == "--help" || a == "-h") {
        std::cout << kUsage;
        return 0;
      } else if (a == "--out") o.csv = next();
      else if (a == "--env-out") o.env_out = next();
      else if (a == "--reps") o.reps = std::stoul(next());
      else if (a == "--keygen-reps") o.keygen_reps = std::stoul(next());
      else if (a == "--warmup") o.warmup = std::stod(next());
      else if (a == "--target-sec") o.target = std::stod(next());
      else if (a == "--max-ops") o.max_ops = std::stoul(next());
      else if (a == "--suites") o.suites = split(next());
      else if (a == "--bits") {
        o.bits.clear();
        for (const auto& s : split(next())) o.bits.push_back(static_cast<unsigned>(std::stoul(s)));
      } else if (a == "--sizes") {
        o.sizes.clear();
        for (const auto& s : split(next())) o.sizes.push_back(parse_size(s));
      } else if (a == "--quick") o.quick = true;
      else throw std::invalid_argument("unknown option " + a);
    }
    if (o.quick) {
      o.reps = 3;
      o.warmup = 0.1;
      o.target = 0.2;
      o.sizes = {1024, 1u << 20};
    }
    if (o.reps < 2) throw std::invalid_argument("--reps must be >= 2");
    for (unsigned b : o.bits)
      if (!is_supported_modulus_bits(b)) throw std::invalid_argument("unsupported key size " + std::to_string(b));
  } catch (const std::exception& e) {
    std::fprintf(stderr, "Error: %s\n%s", e.what(), kUsage);
    return 1;
  }
  if (o.reps < 30 && !o.quick) std::fprintf(stderr, "WARNING: the protocol uses 30-100 repetitions per case (got %zu)\n", o.reps);

  const std::string env = env_text();
  std::cout << env;
  std::printf("# Protocol   : warm-up %.2f s, %zu reps, block ~%.2f s (max %zu ops), keygen: 1 key per rep\n\n", o.warmup, o.reps, o.target, o.max_ops);
  if (!o.env_out.empty()) {
    try {
      write_file(o.env_out, reinterpret_cast<const uint8_t*>(env.data()), env.size(), true);
    } catch (const std::exception& e) {
      std::fprintf(stderr, "Error: %s\n", e.what());
      return 2;
    }
  }

  try {
    Csv csv;
    csv.open(o.csv);
    CryptoPP::AutoSeededRandomPool rng;
    auto has = [&](const char* s) { return std::find(o.suites.begin(), o.suites.end(), s) != o.suites.end(); };

    if (has("keygen")) {
      for (unsigned b : o.bits) {
        run_case(csv, o, "keygen", "keygen", b, 0, false, [&] {
          CryptoPP::AutoSeededRandomPool r;
          const KeyPair kp = generate_keypair(b, r);
          g_sink = static_cast<uint8_t>(g_sink ^ static_cast<uint8_t>(kp.pub.GetModulus().GetByte(0)));
        }, o.keygen_reps ? o.keygen_reps : o.reps, true);
      }
    }

    std::map<unsigned, KeyPair> keys;
    auto key_for = [&](unsigned b) -> const KeyPair& {
      auto it = keys.find(b);
      if (it == keys.end()) it = keys.emplace(b, generate_keypair(b, rng)).first;  // key generation here is not timed
      return it->second;
    };

    if (has("rsa")) {
      for (unsigned b : o.bits) {
        const KeyPair& kp = key_for(b);
        Bytes msg(32);
        rng.GenerateBlock(msg.data(), msg.size());
        const Bytes ct = oaep_encrypt(kp.pub, msg.data(), msg.size(), {}, rng);
        run_case(csv, o, "rsa", "encrypt", b, 0, false, [&] {
          const Bytes c = oaep_encrypt(kp.pub, msg.data(), msg.size(), {}, rng);
          g_sink = static_cast<uint8_t>(g_sink ^ c[0]);
        }, o.reps, false);
        run_case(csv, o, "rsa", "decrypt", b, 0, false, [&] {
          const Bytes p = oaep_decrypt(kp.priv, ct.data(), ct.size(), {}, rng);
          g_sink = static_cast<uint8_t>(g_sink ^ p[0]);
        }, o.reps, false);
      }
    }

    if (has("aesgcm")) {
      for (std::size_t n : o.sizes) {
        Bytes key(32), iv(12), msg(n), ct(n), pt(n), tag(16);
        rng.GenerateBlock(key.data(), key.size());
        rng.GenerateBlock(iv.data(), iv.size());
        rng.GenerateBlock(msg.data(), msg.size());
        // The IV is reused across repetitions only because this is a benchmark of the primitive, not a protocol.
        run_case(csv, o, "aesgcm", "encrypt", 0, n, true, [&] {
          aes256_gcm_encrypt(key.data(), iv.data(), nullptr, 0, msg.data(), n, ct.data(), tag.data());
          g_sink = static_cast<uint8_t>(g_sink ^ tag[0]);
        }, o.reps, false);
        aes256_gcm_encrypt(key.data(), iv.data(), nullptr, 0, msg.data(), n, ct.data(), tag.data());
        run_case(csv, o, "aesgcm", "decrypt", 0, n, true, [&] {
          if (!aes256_gcm_decrypt(key.data(), iv.data(), nullptr, 0, ct.data(), n, tag.data(), pt.data())) throw std::runtime_error("self-check failed");
          g_sink = static_cast<uint8_t>(g_sink ^ pt[0]);
        }, o.reps, false);
      }
    }

    if (has("hybrid")) {
      for (unsigned b : o.bits) {
        const KeyPair& kp = key_for(b);
        for (std::size_t n : o.sizes) {
          Bytes msg(n);
          rng.GenerateBlock(msg.data(), msg.size());
          const Bytes container = encrypt_message(kp.pub, msg.data(), msg.size(), {}, true, rng).container;
          run_case(csv, o, "hybrid", "encrypt", b, n, true, [&] {
            const EncryptResult r = encrypt_message(kp.pub, msg.data(), msg.size(), {}, true, rng);
            g_sink = static_cast<uint8_t>(g_sink ^ r.container.back());
          }, o.reps, false);
          run_case(csv, o, "hybrid", "decrypt", b, n, true, [&] {
            const Bytes p = decrypt_message(kp.priv, container, {});
            if (!p.empty()) g_sink = static_cast<uint8_t>(g_sink ^ p[0]);
          }, o.reps, false);
        }
      }
    }
  } catch (const std::exception& e) {
    std::fprintf(stderr, "Error: %s\n", e.what());
    return 2;
  }
  if (!o.csv.empty()) std::printf("\n# CSV written to %s\n", o.csv.c_str());
  return 0;
}
