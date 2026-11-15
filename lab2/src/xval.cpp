#include "xval.hpp"

#include <algorithm>
#include <memory>

#include "aes128.hpp"
#include "ctr.hpp"
#include "errors.hpp"
#include "util.hpp"
#include "aesni.hpp"
#include "xts.hpp"

#ifdef AESP_HAVE_OPENSSL
#include <openssl/evp.h>
#endif
#ifdef AESP_HAVE_CRYPTOPP
#include <aes.h>
#include <modes.h>
#endif

using namespace std;

namespace aesp {

#ifdef AESP_HAVE_OPENSSL
static void openssl_ctr(const uint8_t* key, const uint8_t* iv, const uint8_t* in, uint8_t* out, std::size_t n) {
  std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
  if (!ctx || EVP_EncryptInit_ex(ctx.get(), EVP_aes_128_ctr(), nullptr, key, iv) != 1)
    throw Error(ExitCode::Failure, "OpenSSL reference init failed");
  std::size_t off = 0;
  while (off < n) {
    const int chunk = static_cast<int>(std::min<std::size_t>(n - off, 1u << 30));
    int outl = 0;
    if (EVP_EncryptUpdate(ctx.get(), out + off, &outl, in + off, chunk) != 1 || outl != chunk)
      throw Error(ExitCode::Failure, "OpenSSL reference update failed");
    off += static_cast<std::size_t>(chunk);
  }
}
#endif

#ifdef AESP_HAVE_OPENSSL
static const EVP_CIPHER* openssl_ctr_cipher(std::size_t key_len) {
  return key_len == 16 ? EVP_aes_128_ctr() : key_len == 24 ? EVP_aes_192_ctr() : EVP_aes_256_ctr();
}
static void openssl_ctr_any(const uint8_t* key, std::size_t key_len, const uint8_t* iv, const uint8_t* in, uint8_t* out,
                            std::size_t n) {
  std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
  if (!ctx || EVP_EncryptInit_ex(ctx.get(), openssl_ctr_cipher(key_len), nullptr, key, iv) != 1)
    throw Error(ExitCode::Failure, "OpenSSL reference init failed");
  std::size_t off = 0;
  while (off < n) {
    const int chunk = static_cast<int>(std::min<std::size_t>(n - off, 1u << 30));
    int outl = 0;
    if (EVP_EncryptUpdate(ctx.get(), out + off, &outl, in + off, chunk) != 1 || outl != chunk)
      throw Error(ExitCode::Failure, "OpenSSL reference update failed");
    off += static_cast<std::size_t>(chunk);
  }
}
// OpenSSL XTS (single data unit, ciphertext stealing included). Supports 32- and 64-byte keys.
static void openssl_xts(const uint8_t* key, std::size_t key_len, const uint8_t* tweak, const uint8_t* in, uint8_t* out,
                        std::size_t n, bool encrypt) {
  std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
  const EVP_CIPHER* c = key_len == 32 ? EVP_aes_128_xts() : EVP_aes_256_xts();
  int outl = 0, fin = 0;
  if (!ctx || EVP_CipherInit_ex(ctx.get(), c, nullptr, key, tweak, encrypt ? 1 : 0) != 1 ||
      EVP_CipherUpdate(ctx.get(), out, &outl, in, static_cast<int>(n)) != 1 ||
      EVP_CipherFinal_ex(ctx.get(), out + outl, &fin) != 1 || static_cast<std::size_t>(outl + fin) != n)
    throw Error(ExitCode::Failure, "OpenSSL XTS reference failed");
}
#endif

#ifdef AESP_HAVE_CRYPTOPP
static void cryptopp_ctr(const uint8_t* key, const uint8_t* iv, const uint8_t* in, uint8_t* out, std::size_t n) {
  CryptoPP::CTR_Mode<CryptoPP::AES>::Encryption enc(key, 16, iv);
  if (n) enc.ProcessData(out, in, n);
}
#endif

std::vector<RefImpl> available_references(const std::string& which) {
  if (which != "all" && which != "openssl" && which != "cryptopp" && which != "none")
    throw Error(ExitCode::Usage, "unknown reference '" + which + "' (use all|openssl|cryptopp|none)");
  std::vector<RefImpl> refs;
  const bool all = which == "all";
#ifdef AESP_HAVE_OPENSSL
  if (all || which == "openssl") refs.push_back({"openssl", openssl_ctr, openssl_ctr_any, openssl_xts});
#endif
#ifdef AESP_HAVE_CRYPTOPP
  if (all || which == "cryptopp") refs.push_back({"cryptopp", cryptopp_ctr, nullptr, nullptr});
#endif
  if (!all && which != "none" && refs.empty())
    throw Error(ExitCode::Usage, "reference '" + which + "' was not compiled into this build");
  return refs;
}

XvalSummary run_xval(const std::string& which, std::size_t random_cases, std::ostream& os) {
  const std::vector<RefImpl> refs = available_references(which);
  if (refs.empty()) throw Error(ExitCode::Usage, "no reference implementation available in this build (need OpenSSL or Crypto++)");

  static const std::size_t kLens[] = {0, 1, 15, 16, 17, 31, 32, 33, 63, 64, 65, 255, 256, 1000, 4096, 65537};
  std::vector<std::size_t> lens(std::begin(kLens), std::end(kLens));
  for (std::size_t i = 0; i < random_cases; ++i) {
    uint8_t r[4];
    secure_random(r, 4);
    lens.push_back((static_cast<std::size_t>(r[0]) | (static_cast<std::size_t>(r[1]) << 8) | (static_cast<std::size_t>(r[2]) << 16)) % 200000);
  }

  XvalSummary sum;
  for (const RefImpl& ref : refs) {
    std::size_t pass = 0, fail = 0;
    for (std::size_t idx = 0; idx < lens.size(); ++idx) {
      const std::size_t len = lens[idx];
      Bytes key = random_bytes(16), iv = random_bytes(16), data = random_bytes(len);
      if (idx % 3 == 1) {  // force carries: low 8 bytes near 2^64 boundary, carry runs into upper half
        std::fill(iv.begin() + 8, iv.end(), uint8_t(0xFF));
        iv[15] = static_cast<uint8_t>(0xF0 + (idx % 8));
        iv[7] = 0x00;
      } else if (idx % 3 == 2) {  // carry across the low 32 bits
        iv[12] = iv[13] = iv[14] = 0xFF;
        iv[15] = 0xFE;
      }
      Bytes expect(len);
      ref.ctr(key.data(), iv.data(), data.data(), expect.data(), len);
      bool ok = true;
      for (Backend be : {Backend::SBoxTable, Backend::TTable, Backend::SBoxComputed}) {
        if (be == Backend::SBoxComputed && len > 4096) continue;  // computed S-box is intentionally slow
        Aes128 aes(key.data(), key.size(), be);
        if (ctr_crypt(aes, iv.data(), data) != expect) ok = false;
      }
      ok ? ++pass : ++fail;
      if (!ok) os << "FAIL  vs " << ref.name << "  len=" << len << " iv=" << to_hex(iv) << "\n";
    }
    os << (fail == 0 ? "PASS  " : "FAIL  ") << "pure AES-128-CTR == " << ref.name << "  (" << pass << "/" << (pass + fail)
       << " cases, 3 backends)\n";
    sum.total += pass + fail;
    sum.failed += fail;
  }
  // ---- BONUS: AES-192/256 CTR and XTS-AES against the reference (and AES-NI against the software backends)
  std::vector<Backend> bonus_backends = {Backend::SBoxTable, Backend::TTable, Backend::SBoxComputed};
  if (aesni::available()) bonus_backends.push_back(Backend::AesNi);
  for (const RefImpl& ref : refs) {
    if (ref.ctr_any) {
      for (std::size_t klen : {std::size_t(24), std::size_t(32)}) {
        std::size_t pass = 0, fail = 0;
        for (std::size_t idx = 0; idx < lens.size(); ++idx) {
          const std::size_t len = lens[idx];
          Bytes key = random_bytes(klen), iv = random_bytes(16), data = random_bytes(len);
          if (idx % 3 == 1) {
            std::fill(iv.begin() + 8, iv.end(), uint8_t(0xFF));
            iv[15] = static_cast<uint8_t>(0xF0 + (idx % 8));
          }
          Bytes expect(len);
          ref.ctr_any(key.data(), klen, iv.data(), data.data(), expect.data(), len);
          bool ok = true;
          for (Backend be : bonus_backends) {
            if (be == Backend::SBoxComputed && len > 4096) continue;
            const Aes aes(key.data(), key.size(), be);
            if (ctr_crypt(aes, iv.data(), data) != expect) ok = false;
          }
          ok ? ++pass : ++fail;
          if (!ok) os << "FAIL  vs " << ref.name << "  AES-" << klen * 8 << "-CTR len=" << len << "\n";
        }
        os << (fail == 0 ? "PASS  " : "FAIL  ") << "pure AES-" << klen * 8 << "-CTR == " << ref.name << "  (" << pass << "/"
           << (pass + fail) << " cases, " << bonus_backends.size() << " backends)\n";
        sum.total += pass + fail;
        sum.failed += fail;
      }
    }
    if (ref.xts) {
      for (std::size_t klen : {std::size_t(32), std::size_t(64)}) {
        std::size_t pass = 0, fail = 0;
        for (std::size_t idx = 0; idx < lens.size(); ++idx) {
          const std::size_t len = lens[idx];
          if (len < 16) continue;  // XTS needs at least one full block
          Bytes key = random_bytes(klen), tweak = random_bytes(16), data = random_bytes(len);
          Bytes expect(len), expect_dec(len);
          ref.xts(key.data(), klen, tweak.data(), data.data(), expect.data(), len, true);
          ref.xts(key.data(), klen, tweak.data(), expect.data(), expect_dec.data(), len, false);
          bool ok = expect_dec == data;
          for (Backend be : bonus_backends) {
            if (be == Backend::SBoxComputed && len > 4096) continue;
            const Xts xts(key.data(), klen, be);
            Bytes got(len), back(len);
            xts.encrypt(tweak.data(), data.data(), got.data(), len);
            xts.decrypt(tweak.data(), got.data(), back.data(), len);
            if (got != expect || back != data) ok = false;
          }
          ok ? ++pass : ++fail;
          if (!ok) os << "FAIL  vs " << ref.name << "  XTS-AES-" << klen * 4 << " len=" << len << "\n";
        }
        os << (fail == 0 ? "PASS  " : "FAIL  ") << "pure XTS-AES-" << klen * 4 << " == " << ref.name << "  (" << pass << "/"
           << (pass + fail) << " cases incl. ciphertext stealing, " << bonus_backends.size() << " backends)\n";
        sum.total += pass + fail;
        sum.failed += fail;
      }
    }
  }

  os << "\nSummary: " << sum.total << " cross-validation cases, " << sum.failed << " failed\n";
  return sum;
}

}  // namespace aesp
