#include "cli.hpp"

#include <cstdio>
#include <iostream>
#include <map>
#include <set>
#include <sstream>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

#include "cryptopp_all.hpp"
#include "encoding.hpp"
#include "errors.hpp"
#include "hybrid.hpp"
#include "kat.hpp"
#include "keyio.hpp"
#include "util.hpp"

namespace rsalab {

namespace {

const char* kUsage = R"(rsatool - RSA-OAEP (SHA-256, MGF1-SHA-256) and hybrid envelope encryption (RSA-OAEP + AES-256-GCM), Crypto++

USAGE
  rsatool keygen  [--bits 3072|4096] --pub pub.pem --priv priv.pem [--force]
  rsatool encrypt (--in FILE | --text "...") --pub pub.pem [--out FILE] [--label TEXT | --label-hex HEX]
                  [--encode raw|hex|base64] [--force-hybrid] [--force]
  rsatool decrypt --in FILE --priv priv.pem [--out FILE] [--label TEXT | --label-hex HEX] [--encode raw|hex|base64] [--force]
  rsatool --kat kat/vectors.json            Known Answer Tests (PASS/FAIL per case + summary)

NOTES
  keygen   writes pub.pem (SPKI), priv.pem (PKCS#8), pub.der, priv.der and priv.meta.json
           ({"creation_time","modulus_bits","hash"}); private files are 0600 on POSIX. Default size: 3072 bits (e = 65537).
  encrypt  messages up to 318 bytes (RSA-3072) / 446 bytes (RSA-4096) use direct RSA-OAEP; longer ones switch to the hybrid
           envelope automatically (--force-hybrid forces it). --encode is the format of the ciphertext FILE (default raw;
           with no --out the ciphertext is printed as hex unless --encode is given).
  decrypt  must be given the same --label that was used to encrypt. --encode is the format of the ciphertext file read.
           The plaintext is written only after everything verified; any failure prints ONE generic message and exit code 3.
  --verbose prints the selected mode and sizes (never keys, plaintext or labels).    --quiet silences the success message.
Exit codes: 0 ok, 1 usage, 2 I/O or unreadable/unsupported key file, 3 decryption failed, 4 KAT failed.
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

const std::set<std::string> kValueOpts = {"bits", "pub", "priv", "in", "text", "out", "label", "label-hex", "encode", "kat"};
const std::set<std::string> kFlagOpts = {"force-hybrid", "force", "verbose", "quiet", "help"};

Args parse_args(const std::vector<std::string>& argv) {
  Args a;
  std::size_t i = 1;
  if (i < argv.size() && argv[i].rfind("--", 0) != 0) a.command = argv[i++];
  for (; i < argv.size(); ++i) {
    const std::string& t = argv[i];
    if (t.rfind("--", 0) != 0) usage_error("unexpected argument '" + t + "'");
    std::string name = t.substr(2), value;
    bool has_value = false;
    const auto eq = name.find('=');
    if (eq != std::string::npos) {
      value = name.substr(eq + 1);
      name = name.substr(0, eq);
      has_value = true;
    }
    if (kFlagOpts.count(name)) {
      if (has_value) usage_error("option --" + name + " takes no value");
      a.flags.insert(name);
    } else if (kValueOpts.count(name)) {
      if (!has_value) {
        if (i + 1 >= argv.size()) usage_error("option --" + name + " requires a value");
        value = argv[++i];
      }
      if (a.kv.count(name)) usage_error("option --" + name + " given twice");
      a.kv[name] = value;
    } else {
      usage_error("unknown option --" + name);
    }
  }
  return a;
}

void set_binary_stdout() {
#if defined(_WIN32)
  _setmode(_fileno(stdout), _O_BINARY);
#endif
}

void write_stdout(const uint8_t* d, std::size_t n) {
  set_binary_stdout();
  if (n) std::fwrite(d, 1, n, stdout);
  std::fflush(stdout);
}

constexpr std::size_t kMaxInput = 1u << 30;  // 1 GiB

Bytes get_label(const Args& a) {
  if (a.has("label") && a.has("label-hex")) usage_error("provide only one of --label / --label-hex");
  if (a.has("label")) return to_bytes(a.get("label"));
  if (a.has("label-hex")) {
    const auto b = hex_decode(a.get("label-hex"));
    if (!b) usage_error("--label-hex is not valid hex");
    return *b;
  }
  return {};
}

int cmd_keygen(const Args& a) {
  if (!a.has("pub") || !a.has("priv")) usage_error("keygen needs --pub and --priv");
  unsigned bits = 3072;
  if (a.has("bits")) {
    try {
      std::size_t used = 0;
      const unsigned long v = std::stoul(a.get("bits"), &used);
      if (used != a.get("bits").size()) throw std::invalid_argument("x");
      bits = static_cast<unsigned>(v);
    } catch (const std::exception&) {
      usage_error("--bits must be a number (3072 or 4096)");
    }
  }
  if (!is_supported_modulus_bits(bits)) usage_error("unsupported key size " + std::to_string(bits) + " (use 3072 or 4096)");
  CryptoPP::AutoSeededRandomPool rng;
  const KeyPair kp = generate_keypair(bits, rng);
  const SavedFiles f = save_keypair(kp, a.get("pub"), a.get("priv"), a.flag("force"));
  if (!a.flag("quiet"))
    std::fprintf(stderr, "generated RSA-%u key pair: %s, %s, %s, %s, %s\n", bits, f.pub_pem.c_str(), f.pub_der.c_str(), f.priv_pem.c_str(),
                 f.priv_der.c_str(), f.meta.c_str());
  return 0;
}

int cmd_encrypt(const Args& a) {
  if (a.has("in") == a.has("text")) usage_error("provide exactly one of --in FILE or --text TEXT");
  if (!a.has("pub")) usage_error("encrypt needs --pub");
  const bool has_out = a.has("out");
  Encoding enc = a.has("encode") ? parse_encoding(a.get("encode")) : (has_out ? Encoding::Raw : Encoding::Hex);
  const Bytes label = get_label(a);
  const PublicKey pub = load_public_key(a.get("pub"));
  Bytes msg = a.has("in") ? read_file(a.get("in"), kMaxInput) : to_bytes(a.get("text"));
  WipeOnExit wipe(msg);
  CryptoPP::AutoSeededRandomPool rng;
  const EncryptResult r = encrypt_message(pub, msg.data(), msg.size(), label, a.flag("force-hybrid"), rng);
  const Bytes file_bytes = encode_for_file(r.container, enc);
  if (a.flag("verbose"))
    std::fprintf(stderr, "mode: %s; RSA-%u; plaintext %zu bytes; output %zu bytes\n", r.mode == Mode::Direct ? "direct RSA-OAEP" : "hybrid RSA-OAEP + AES-256-GCM",
                 modulus_bits(pub), msg.size(), file_bytes.size());
  if (has_out) {
    write_file(a.get("out"), file_bytes, a.flag("force"));
    if (!a.flag("quiet")) std::fprintf(stderr, "encrypted %zu bytes -> %s\n", msg.size(), a.get("out").c_str());
  } else {
    write_stdout(file_bytes.data(), file_bytes.size());
  }
  return 0;
}

int cmd_decrypt(const Args& a) {
  if (!a.has("in")) usage_error("decrypt needs --in");
  if (!a.has("priv")) usage_error("decrypt needs --priv");
  const Encoding enc = a.has("encode") ? parse_encoding(a.get("encode")) : Encoding::Raw;
  const Bytes label = get_label(a);
  const PrivateKey priv = load_private_key(a.get("priv"));
  const Bytes file_bytes = read_file(a.get("in"), kMaxInput);
  const auto container = decode_from_file(file_bytes, enc);
  if (!container) crypto_failure();  // malformed hex/base64: same generic failure as everything else
  Bytes plain = decrypt_message(priv, *container, label);
  WipeOnExit wipe(plain);
  if (a.flag("verbose")) std::fprintf(stderr, "plaintext %zu bytes\n", plain.size());
  if (a.has("out")) {
    write_file(a.get("out"), plain, a.flag("force"));
    if (!a.flag("quiet")) std::fprintf(stderr, "decrypted %zu bytes -> %s\n", plain.size(), a.get("out").c_str());
  } else {
    write_stdout(plain.data(), plain.size());
  }
  return 0;
}

int cmd_kat(const Args& a) {
  std::ostringstream ss;
  const KatSummary s = run_kat_file(a.get("kat"), ss);
  std::cout << ss.str() << std::flush;
  return s.ok() ? 0 : static_cast<int>(ExitCode::KatFailed);
}

}  // namespace

int run_cli(const std::vector<std::string>& argv) {
  Args a;
  try {
    a = parse_args(argv);
  } catch (const Error& e) {
    std::fprintf(stderr, "Error: %s\n", e.what());
    return static_cast<int>(e.code());
  }
  if (a.flag("help") || (a.command.empty() && !a.has("kat"))) {
    std::cout << kUsage;
    return a.flag("help") ? 0 : static_cast<int>(ExitCode::Usage);
  }
  const bool is_decrypt = a.command == "decrypt";
  try {
    if (a.command.empty() && a.has("kat")) return cmd_kat(a);
    if (a.command == "keygen") return cmd_keygen(a);
    if (a.command == "encrypt") return cmd_encrypt(a);
    if (a.command == "decrypt") return cmd_decrypt(a);
    usage_error("unknown command '" + a.command + "'");
  } catch (const Error& e) {
    if (e.code() == ExitCode::Crypto)
      std::fprintf(stderr, "Error: %s\n", kDecryptFailedMessage);  // never e.what(): one fixed text for every cause
    else
      std::fprintf(stderr, "Error: %s\n", e.what());
    return static_cast<int>(e.code());
  } catch (const std::exception& e) {
    // Unexpected library/runtime failure: while decrypting it is reported exactly like any other decryption failure
    // (no detail); for the other commands the library message is shown, it cannot reveal secrets.
    if (is_decrypt) {
      std::fprintf(stderr, "Error: %s\n", kDecryptFailedMessage);
      return static_cast<int>(ExitCode::Crypto);
    }
    std::fprintf(stderr, "Error: operation failed (%s).\n", e.what());
    return static_cast<int>(ExitCode::Usage);
  }
}

}  // namespace rsalab
