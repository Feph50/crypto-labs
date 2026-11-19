// RSA key generation and key files: PEM (SPKI public / PKCS#8 private), DER and the metadata JSON.
// Crypto++ has no PEM support, so the PEM armor is implemented here (strict parser: exact BEGIN/END labels, 64-column
// base64, no headers, nothing after END).
#pragma once
#include <optional>
#include <string>

#include "cryptopp_all.hpp"
#include "util.hpp"

namespace rsalab {

using PublicKey = CryptoPP::RSA::PublicKey;
using PrivateKey = CryptoPP::RSA::PrivateKey;

struct KeyPair {
  PrivateKey priv;
  PublicKey pub;
  unsigned bits = 0;
};

// Only RSA-3072 (required) and RSA-4096 (performance comparison) are supported; the public exponent is 65537.
bool is_supported_modulus_bits(unsigned bits);
inline std::size_t modulus_bytes(unsigned bits) { return (bits + 7) / 8; }
unsigned modulus_bits(const PublicKey& k);
unsigned modulus_bits(const PrivateKey& k);

// Throws Usage error for an unsupported size unless `allow_unsupported_for_test` (unit tests only; not reachable from the CLI).
KeyPair generate_keypair(unsigned bits, CryptoPP::RandomNumberGenerator& rng, bool allow_unsupported_for_test = false);

Bytes public_key_to_der(const PublicKey& k);    // SubjectPublicKeyInfo
Bytes private_key_to_der(const PrivateKey& k);  // PKCS#8 PrivateKeyInfo
std::string der_to_pem(const Bytes& der, const std::string& label);  // label: "PUBLIC KEY" or "PRIVATE KEY"
std::optional<Bytes> pem_to_der(const std::string& text, const std::string& label);

// Parse and validate; throw Error(Io) ("invalid or unsupported key file") on anything wrong, without saying more.
PublicKey public_key_from_der(const Bytes& der, bool allow_unsupported_for_test = false);
PrivateKey private_key_from_der(const Bytes& der, bool allow_unsupported_for_test = false);
// PEM or DER, detected from the content.
PublicKey load_public_key(const std::string& path);
PrivateKey load_private_key(const std::string& path);

struct SavedFiles {
  std::string pub_pem, pub_der, priv_pem, priv_der, meta;
};
// Writes <pub>.pem/.der and <priv>.pem/.der (private files 0600 on POSIX) and <priv>.meta.json. Nothing is written if any
// target already exists and `overwrite` is false.
SavedFiles save_keypair(const KeyPair& kp, const std::string& pub_path, const std::string& priv_path, bool overwrite);

std::string metadata_json(unsigned bits, const std::string& creation_time_utc);

}  // namespace rsalab
