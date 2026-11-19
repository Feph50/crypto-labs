#include "rsa_oaep.hpp"

#include "errors.hpp"

namespace rsalab {

using Encryptor = CryptoPP::RSAES<CryptoPP::OAEP<CryptoPP::SHA256>>::Encryptor;
using Decryptor = CryptoPP::RSAES<CryptoPP::OAEP<CryptoPP::SHA256>>::Decryptor;

std::size_t max_plaintext_len(unsigned modulus_bits) {
  const std::size_t k = modulus_bytes(modulus_bits);
  return k - 2 * kHashLen - 2;
}

Bytes oaep_encrypt(const PublicKey& pub, const uint8_t* msg, std::size_t n, const Bytes& label, CryptoPP::RandomNumberGenerator& rng) {
  const unsigned bits = modulus_bits(pub);
  if (!is_supported_modulus_bits(bits)) usage_error("unsupported key size");
  if (n > max_plaintext_len(bits)) usage_error("message too long for direct RSA-OAEP");
  Encryptor enc(pub);
  Bytes ct(enc.CiphertextLength(n));
  try {
    if (label.empty()) {
      enc.Encrypt(rng, msg, n, ct.data());
    } else {
      const CryptoPP::AlgorithmParameters params =
          CryptoPP::MakeParameters(CryptoPP::Name::EncodingParameters(), CryptoPP::ConstByteArrayParameter(label.data(), label.size()));
      enc.Encrypt(rng, msg, n, ct.data(), params);
    }
  } catch (const CryptoPP::Exception&) {
    throw Error(ExitCode::Usage, "RSA-OAEP encryption failed");
  }
  return ct;
}

Bytes oaep_decrypt(const PrivateKey& priv, const uint8_t* ct, std::size_t n, const Bytes& label, CryptoPP::RandomNumberGenerator& rng) {
  const unsigned bits = modulus_bits(priv);
  if (!is_supported_modulus_bits(bits) || n != modulus_bytes(bits)) crypto_failure();
  try {
    Decryptor dec(priv);
    Bytes plain(dec.MaxPlaintextLength(n));
    CryptoPP::DecodingResult r;
    if (label.empty()) {
      r = dec.Decrypt(rng, ct, n, plain.data());
    } else {
      const CryptoPP::AlgorithmParameters params =
          CryptoPP::MakeParameters(CryptoPP::Name::EncodingParameters(), CryptoPP::ConstByteArrayParameter(label.data(), label.size()));
      r = dec.Decrypt(rng, ct, n, plain.data(), params);
    }
    if (!r.isValidCoding) {
      secure_zero(plain);
      crypto_failure();
    }
    plain.resize(r.messageLength);
    return plain;
  } catch (const CryptoPP::Exception&) {
    crypto_failure();
  }
}

}  // namespace rsalab
