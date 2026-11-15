// aestool -- AES-128-CTR (FIPS-197 / SP 800-38A) implemented from scratch.
// Every failure path exits non-zero with a short, non-leaky message (fail closed).
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "aes128.hpp"
#include "bench.hpp"
#include "ctr.hpp"
#include "errors.hpp"
#include "kat.hpp"
#include "util.hpp"
#include "xts.hpp"
#include "xval.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <fcntl.h>
#include <io.h>
#endif

using namespace std;
using namespace aesp;

namespace {

const char* kUsage = R"(aestool - AES-128 (FIPS-197) in pure C++, CTR mode (NIST SP 800-38A)

USAGE
  aestool encrypt --key FILE|--key-hex HEX (--in FILE | --text "...") [--mode ctr|xts] [options]
  aestool decrypt --key FILE|--key-hex HEX (--in FILE | --text ENCODED --in-encode hex|base64) [options]
  aestool keygen  [--bits 128|192|256|xts256|xts512] [--out FILE]   random key (OS CSPRNG)
  aestool ivgen   [--out FILE]            random IV: 96-bit nonce || 32-bit zero counter
  aestool --kat vectors.json [--verbose]  Known Answer Tests (PASS/FAIL per case + summary)
  aestool_ref xval [--ref all|openssl|cryptopp] [--cases N]   cross-validate vs reference libs
        (aestool_ref is the SAME tool plus the OpenSSL / Crypto++ reference code; the submitted `aestool`
         links no external cryptographic library, so `xval` and `bench --ref` need aestool_ref)
  aestool bench [--sizes 1MiB,100MiB] [--runs 30] [--backends table,ttable,computed]
                      [--ref all|openssl|cryptopp|none] [--dir enc|dec|both] [--ops N]
                      [--warmup SEC] [--bits 128|192|256] [--threads N] [--budget-mb MiB] [--target-sec SEC] [--csv FILE]
                      protocol: warm-up per case, block of ~1000 ops (min(1000, budget/size); --ops N to force), 30-100 runs

OPTIONS
  --mode ctr|xts        ctr (default): AES-CTR; the key length picks AES-128 (16 B), AES-192 (24 B) or AES-256 (32 B)
                        xts: XTS-AES (IEEE 1619), key = K1||K2 of 32 B (XTS-AES-128) or 64 B (XTS-AES-256),
                             IV = 16-byte tweak (data-unit number), input 16 B .. 16 MiB, ciphertext stealing
  --key FILE            raw key file                          --key-hex HEX   hex key (see --mode for the lengths)
  --iv FILE|HEX         IV: raw 16-byte file, or 32 hex chars  --iv-hex HEX    32 hex chars (same as --iv HEX)
  --nonce HEX           CTR only: 96-bit nonce (24 hex chars); IV = nonce || 32-bit zero counter
                        (encrypt: omitted -> random IV, saved to <out>.iv;
                         decrypt: omitted -> read <in>.iv)
  --in FILE | --text S  input (UTF-8 text via --text)         --out FILE      output file (raw)
  --encode hex|base64|raw      output encoding (default: hex on screen when no --out; raw in files)
  --in-encode hex|base64|raw   input encoding (default raw)
  --sbox table|computed|ttable|aesni  AES backend (default table = FIPS-197 byte-oriented S-box lookup;
                        aesni = hardware AES-NI, bonus, only if the CPU has it)
  --aead                rejected: CTR has no authentication (fail closed)
  --threads N           bench only: N concurrent workers, aggregate throughput (default 1)
  --force               overwrite existing output files       --quiet / --verbose
Exit codes: 0 ok, 2 usage, 3 invalid input, 4 I/O, 5 counter overflow, 6 KAT/xval failure, 1 internal.
CTR gives confidentiality ONLY: it does not detect tampering. Never reuse a (key, IV) pair.
)";

struct Args {
  std::string command;
  std::map<std::string, std::string> kv;
  std::set<std::string> flags;
  bool has(const std::string& k) const { return kv.count(k) != 0; }
  bool flag(const std::string& k) const { return flags.count(k) != 0; }
  std::string get(const std::string& k, const std::string& def = "") const {
    auto it = kv.find(k);
    return it == kv.end() ? def : it->second;
  }
};

const std::set<std::string> kValueOpts = {"mode", "key", "key-hex", "iv", "iv-hex", "in", "text", "out", "encode",
                                          "in-encode", "kat", "sbox", "sizes", "runs", "warmup", "target-sec", "ops",
                                          "csv", "backends", "dir", "ref", "cases", "nonce", "threads", "bits", "budget-mb"};
const std::set<std::string> kFlagOpts = {"verbose", "quiet", "force", "help", "aead"};

std::vector<std::string> get_args(int argc, char** argv) {
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

Args parse_args(const std::vector<std::string>& argv) {
  Args a;
  std::size_t i = 1;
  if (i < argv.size() && argv[i].rfind("--", 0) != 0) a.command = argv[i++];
  for (; i < argv.size(); ++i) {
    const std::string& t = argv[i];
    if (t.rfind("--", 0) != 0) throw Error(ExitCode::Usage, "unexpected argument '" + t + "'");
    std::string name = t.substr(2), value;
    bool has_value = false;
    const auto eq = name.find('=');
    if (eq != std::string::npos) {
      value = name.substr(eq + 1);
      name = name.substr(0, eq);
      has_value = true;
    }
    if (kFlagOpts.count(name)) {
      if (has_value) throw Error(ExitCode::Usage, "option --" + name + " takes no value");
      a.flags.insert(name);
    } else if (kValueOpts.count(name)) {
      if (!has_value) {
        if (i + 1 >= argv.size()) throw Error(ExitCode::Usage, "option --" + name + " requires a value");
        value = argv[++i];
      }
      if (a.kv.count(name)) throw Error(ExitCode::Usage, "option --" + name + " given twice");
      a.kv[name] = value;
    } else {
      throw Error(ExitCode::Usage, "unknown option --" + name);
    }
  }
  return a;
}

[[noreturn]] void usage_error(const std::string& m) { throw Error(ExitCode::Usage, m); }

void set_binary_stdout() {
#if defined(_WIN32)
  _setmode(_fileno(stdout), _O_BINARY);
#endif
}

// Loads the key and checks its length: CTR accepts 16/24/32 bytes (AES-128/192/256), XTS 32/64 bytes.
Bytes load_key(const Args& a, bool xts) {
  if (a.has("key") == a.has("key-hex")) usage_error("provide exactly one of --key FILE or --key-hex HEX");
  Bytes k;
  if (a.has("key")) {
    try {
      k = read_file(a.get("key"), 4096);
    } catch (const Error& e) {
      if (e.code() == ExitCode::Io) throw Error(ExitCode::Io, "cannot read key file '" + a.get("key") + "'");
      throw;
    }
  } else {
    k = from_hex(a.get("key-hex"));
  }
  const bool ok = xts ? (k.size() == 32 || k.size() == 64) : (k.size() == 16 || k.size() == 24 || k.size() == 32);
  if (!ok) {
    secure_zero(k.data(), k.size());
    throw Error(ExitCode::InvalidInput,
                xts ? "invalid key length: XTS requires 32 bytes (XTS-AES-128) or 64 bytes (XTS-AES-256)"
                    : "invalid key length: AES requires 16, 24 or 32 bytes (32, 48 or 64 hex chars)");
  }
  return k;
}

std::vector<std::string> split_csv(const std::string& s) {
  std::vector<std::string> out;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, ',')) {
    if (item.empty()) usage_error("empty item in list '" + s + "'");
    out.push_back(item);
  }
  return out;
}

std::size_t parse_count(const std::string& s, const char* what) {
  const std::size_t v = parse_size(s);
  if (v == 0 && s != "0") usage_error(std::string("invalid ") + what);
  return v;
}

double parse_seconds(const std::string& s, const char* what) {
  char* end = nullptr;
  const double v = std::strtod(s.c_str(), &end);
  if (s.empty() || end == nullptr || *end != '\0' || !(v >= 0.0) || v > 3600.0)
    usage_error(std::string("invalid value for ") + what);
  return v;
}

Bytes make_iv_default() {  // 96-bit random nonce || 32-bit zero block counter
  Bytes iv(16, 0);
  secure_random(iv.data(), 12);
  return iv;
}

std::size_t keygen_bytes(const std::string& bits) {
  if (bits == "128") return 16;
  if (bits == "192") return 24;
  if (bits == "256") return 32;
  if (bits == "xts256") return 32;  // XTS-AES-128: two 128-bit keys
  if (bits == "xts512") return 64;  // XTS-AES-256: two 256-bit keys
  usage_error("--bits must be 128|192|256|xts256|xts512");
}

int cmd_rand(const Args& a, bool is_iv) {
  if (is_iv && a.has("bits")) usage_error("--bits is only valid for keygen");
  Bytes b = is_iv ? make_iv_default() : random_bytes(keygen_bytes(a.get("bits", "128")));
  WipeOnExit w(b);
  if (a.has("out")) {
    write_file(a.get("out"), b.data(), b.size(), a.flag("force"));
    if (!a.flag("quiet")) std::fprintf(stderr, "wrote %zu random bytes to %s\n", b.size(), a.get("out").c_str());
  } else {
    std::cout << to_hex(b) << "\n";
  }
  return 0;
}

int cmd_crypt(const Args& a, bool encrypt) {
  const std::string mode = a.get("mode", "ctr");
  if (mode != "ctr" && mode != "xts") usage_error("unsupported mode '" + mode + "': this tool implements ctr and xts");
  const bool xts_mode = mode == "xts";
  const bool quiet = a.flag("quiet"), verbose = a.flag("verbose"), force = a.flag("force");

  Bytes key = load_key(a, xts_mode);
  WipeOnExit wipe(key);
  const Backend be = parse_backend(a.get("sbox", "table"));
  std::unique_ptr<Aes> aes_ctr;
  std::unique_ptr<Xts> aes_xts;
  if (xts_mode)
    aes_xts.reset(new Xts(key.data(), key.size(), be));  // rejects K1 == K2
  else
    aes_ctr.reset(new Aes(key.data(), key.size(), be));

  if (a.has("in") == a.has("text")) usage_error("provide exactly one of --in FILE or --text STRING");
  const Encoding in_enc = parse_encoding(a.get("in-encode", "raw"));
  const bool has_out = a.has("out");
  // Spec: on-screen ciphertext defaults to hex; file output defaults to raw binary.
  const Encoding out_enc = parse_encoding(a.get("encode", (encrypt && !has_out) ? "hex" : "raw"));
  if (a.has("text") && !encrypt && in_enc == Encoding::Raw)
    usage_error("decrypt --text needs --in-encode hex|base64 (ciphertext is binary)");
  if (encrypt && !has_out && out_enc == Encoding::Raw)
    usage_error("raw ciphertext cannot be printed to the screen; use --out FILE or --encode hex|base64");

  // ---- IV / nonce
  if (a.flag("aead")) usage_error("--aead is not supported: CTR/XTS provide confidentiality only, no authentication tag");
  if (int(a.has("iv")) + int(a.has("iv-hex")) + int(a.has("nonce")) > 1)
    usage_error("provide only one of --iv / --iv-hex / --nonce");
  Bytes iv;
  bool generated = false;
  if (a.has("nonce") && xts_mode) usage_error("--nonce is for CTR; XTS takes a 16-byte tweak via --iv / --iv-hex");
  if (a.has("nonce")) {
    Bytes n = from_hex(a.get("nonce"));
    if (n.size() != 12) throw Error(ExitCode::InvalidInput, "invalid nonce length: expected exactly 12 bytes (24 hex chars)");
    iv = n;
    iv.resize(16, 0);  // 96-bit nonce || 32-bit zero block counter (big-endian)
  } else if (a.has("iv")) {
    const std::string v = a.get("iv");
    if (file_exists(v)) {
      iv = read_file_exact(v, 16, "IV");
    } else {
      iv = from_hex(v);  // not a file: must be hex (throws InvalidInput otherwise)
      if (iv.size() != 16) throw Error(ExitCode::InvalidInput, "invalid IV length: expected a 16-byte file or exactly 32 hex chars");
    }
  } else if (a.has("iv-hex")) {
    iv = from_hex(a.get("iv-hex"));
    if (iv.size() != 16) throw Error(ExitCode::InvalidInput, "invalid IV length: expected exactly 16 bytes (32 hex chars)");
  } else if (encrypt) {
    iv = xts_mode ? random_bytes(16) : make_iv_default();  // XTS: random 16-byte tweak
    generated = true;
  } else {
    const std::string side = a.has("in") ? a.get("in") + ".iv" : "";
    if (side.empty() || !file_exists(side)) usage_error("decrypt requires --iv/--iv-hex/--nonce (or an <in>.iv sidecar file)");
    iv = read_file_exact(side, 16, "IV sidecar");
  }
  if (encrypt && !generated && !quiet)
    std::fprintf(stderr, "WARNING: IV supplied manually. Reusing a (key, IV) pair destroys confidentiality (two-time pad).\n");
  if (!encrypt && !quiet)
    std::fprintf(stderr, "NOTE: %s is unauthenticated - tampering with the ciphertext cannot be detected.\n",
                 xts_mode ? "XTS" : "CTR");

  // ---- pre-flight overwrite checks (so we never end up with ciphertext but no IV sidecar)
  const std::string sidecar = has_out ? a.get("out") + ".iv" : "";
  if (has_out && !force) {
    if (file_exists(a.get("out"))) throw Error(ExitCode::Io, "output file '" + a.get("out") + "' already exists (use --force)");
    if (encrypt && generated && file_exists(sidecar))
      throw Error(ExitCode::Io, "IV sidecar '" + sidecar + "' already exists (use --force)");
  }

  if (verbose)
    std::fprintf(stderr, "backend=%s mode=%s cipher=%s iv=%s\n", backend_name(be), mode.c_str(),
                 xts_mode ? (key.size() == 32 ? "XTS-AES-128" : "XTS-AES-256")
                          : (key.size() == 16 ? "AES-128" : key.size() == 24 ? "AES-192" : "AES-256"),
                 to_hex(iv).c_str());

  std::size_t processed = 0;
  const bool streaming = !xts_mode && a.has("in") && in_enc == Encoding::Raw && has_out && out_enc == Encoding::Raw;
  if (streaming) {
    ctr_crypt_file(*aes_ctr, iv.data(), a.get("in"), a.get("out"), force);
    processed = static_cast<std::size_t>(std::filesystem::file_size(to_path(a.get("out"))));
  } else {
    const std::size_t kMaxMem = xts_mode ? kXtsMaxDataUnit : (static_cast<std::size_t>(1) << 31);
    Bytes data;
    if (a.has("text")) {
      data = decode_text(a.get("text"), in_enc);
    } else {
      Bytes file = read_file(a.get("in"), kMaxMem);
      data = (in_enc == Encoding::Raw) ? file : decode_text(std::string(file.begin(), file.end()), in_enc);
    }
    Bytes result;
    if (xts_mode) {  // one data unit: ciphertext stealing handles lengths that are not a multiple of 16
      result.resize(data.size());
      if (encrypt)
        aes_xts->encrypt(iv.data(), data.data(), result.data(), data.size());
      else
        aes_xts->decrypt(iv.data(), data.data(), result.data(), data.size());
    } else {
      result = ctr_crypt(*aes_ctr, iv.data(), data);
    }
    processed = result.size();
    if (has_out) {
      if (out_enc == Encoding::Raw) {
        write_file(a.get("out"), result.data(), result.size(), force);
      } else {
        const std::string text = encode_bytes(result, out_enc) + "\n";
        write_file(a.get("out"), reinterpret_cast<const uint8_t*>(text.data()), text.size(), force);
      }
    } else {
      if (encrypt) std::cout << "iv=" << to_hex(iv) << "\n";
      if (out_enc == Encoding::Raw) {
        set_binary_stdout();
        std::fwrite(result.data(), 1, result.size(), stdout);
        std::fflush(stdout);
      } else {
        std::cout << (encrypt ? "ciphertext(" : "plaintext(") << (out_enc == Encoding::Hex ? "hex" : "base64")
                  << ")=" << encode_bytes(result, out_enc) << "\n";
      }
    }
    secure_zero(data.data(), data.size());
    if (!encrypt) secure_zero(result.data(), result.size());
  }

  if (has_out && encrypt && generated) {
    write_file(sidecar, iv.data(), iv.size(), force);
    std::fprintf(stderr, "generated IV: %s (saved to %s)\n", to_hex(iv).c_str(), sidecar.c_str());
  }
  if (has_out && !quiet)
    std::fprintf(stderr, "%s %zu bytes -> %s\n", encrypt ? "encrypted" : "decrypted", processed, a.get("out").c_str());
  return 0;
}

int cmd_kat(const Args& a) {
  const std::string kat_path = resolve_input_path(a.get("kat")).string();
  std::ostringstream ss;
  const KatSummary s = run_kat_file(kat_path, a.flag("verbose"), ss);
  const std::string out_str = ss.str();
  std::cout << out_str << std::flush;
  if (a.has("out"))  // a file is written only when the user asks for it (redirect stdout otherwise)
    write_file(a.get("out"), reinterpret_cast<const uint8_t*>(out_str.data()), out_str.size(), true);
  return s.ok() ? 0 : static_cast<int>(ExitCode::KatFailed);
}

int cmd_xval(const Args& a) {
  std::ostringstream ss;
  const XvalSummary s = run_xval(a.get("ref", "all"), parse_count(a.get("cases", "50"), "--cases"), ss);
  const std::string out_str = ss.str();
  std::cout << out_str << std::flush;
  if (a.has("out"))
    write_file(a.get("out"), reinterpret_cast<const uint8_t*>(out_str.data()), out_str.size(), true);
  return s.failed == 0 ? 0 : static_cast<int>(ExitCode::KatFailed);
}

int cmd_bench(const Args& a) {
  BenchOptions o;
  if (a.has("sizes")) {
    o.sizes.clear();
    for (const auto& s : split_csv(a.get("sizes"))) {
      const std::size_t n = parse_size(s);
      if (n == 0) usage_error("--sizes entries must be > 0");
      o.sizes.push_back(n);
    }
  }
  if (a.has("backends")) {
    o.backends.clear();
    for (const auto& s : split_csv(a.get("backends"))) o.backends.push_back(parse_backend(s));
  }
  o.ref = a.get("ref", "all");
  if (a.has("threads")) o.threads = parse_count(a.get("threads"), "--threads");
  if (a.has("budget-mb")) o.budget_bytes = parse_count(a.get("budget-mb"), "--budget-mb") << 20;
  if (a.has("bits")) {
    const std::string b = a.get("bits");
    if (b == "128") o.key_bytes = 16;
    else if (b == "192") o.key_bytes = 24;
    else if (b == "256") o.key_bytes = 32;
    else usage_error("--bits must be 128|192|256");
  }
  if (a.has("runs")) o.runs = parse_count(a.get("runs"), "--runs");
  if (a.has("ops")) o.ops = parse_count(a.get("ops"), "--ops");
  if (a.has("warmup")) o.warmup_sec = parse_seconds(a.get("warmup"), "--warmup");
  if (a.has("target-sec")) o.target_sec = parse_seconds(a.get("target-sec"), "--target-sec");
  if (a.has("dir")) {
    const std::string d = a.get("dir");
    if (d == "enc") o.do_decrypt = false;
    else if (d == "dec") o.do_encrypt = false;
    else if (d != "both") usage_error("--dir must be enc|dec|both");
  }
  o.csv_path = a.get("csv");  // empty = no CSV file (the table is printed to stdout)
  run_bench(o, std::cout);
  return 0;
}

int run(const std::vector<std::string>& argv) {
  const Args a = parse_args(argv);
  if (a.flag("help") || (a.command.empty() && !a.has("kat"))) {
    std::cout << kUsage;
    return a.flag("help") ? 0 : static_cast<int>(ExitCode::Usage);
  }
  if (a.command.empty() && a.has("kat")) return cmd_kat(a);
  if (a.command == "encrypt") return cmd_crypt(a, true);
  if (a.command == "decrypt") return cmd_crypt(a, false);
  if (a.command == "keygen") return cmd_rand(a, false);
  if (a.command == "ivgen") return cmd_rand(a, true);
  if (a.command == "xval") return cmd_xval(a);
  if (a.command == "bench") return cmd_bench(a);
  usage_error("unknown command '" + a.command + "'");
}

}  // namespace

int main(int argc, char** argv) {
  try {
    return run(get_args(argc, argv));
  } catch (const Error& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    return static_cast<int>(e.code());
  } catch (const std::exception&) {
    std::fprintf(stderr, "error: internal failure\n");  // deliberately non-leaky
    return static_cast<int>(ExitCode::Failure);
  }
}
