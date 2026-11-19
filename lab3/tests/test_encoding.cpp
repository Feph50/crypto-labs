#include <catch2/catch_test_macros.hpp>

#include "encoding.hpp"
#include "errors.hpp"

using namespace rsalab;

TEST_CASE("base64: RFC 4648 section 10 test vectors", "[encoding]") {
  const char* plain[] = {"", "f", "fo", "foo", "foob", "fooba", "foobar"};
  const char* b64[] = {"", "Zg==", "Zm8=", "Zm9v", "Zm9vYg==", "Zm9vYmE=", "Zm9vYmFy"};
  for (int i = 0; i < 7; ++i) {
    const Bytes p(plain[i], plain[i] + std::string(plain[i]).size());
    REQUIRE(base64_encode(p) == b64[i]);
    const auto d = base64_decode(b64[i]);
    REQUIRE(d.has_value());
    REQUIRE(*d == p);
  }
}

TEST_CASE("base64: strict decoder rejects malformed input", "[encoding][negative]") {
  REQUIRE_FALSE(base64_decode("Zg=").has_value());       // missing padding
  REQUIRE_FALSE(base64_decode("Zg").has_value());        // missing padding
  REQUIRE_FALSE(base64_decode("Zm9v!").has_value());     // stray character
  REQUIRE_FALSE(base64_decode("Zm9v=Zm9v").has_value()); // '=' in the middle
  REQUIRE_FALSE(base64_decode("Zh==").has_value());      // non-canonical trailing bits
  REQUIRE_FALSE(base64_decode("Zm9=").has_value());      // non-canonical trailing bits
  REQUIRE_FALSE(base64_decode("=Zm9").has_value());
  REQUIRE(base64_decode(" Zm9v\r\nYmFy \n").has_value());  // whitespace is ignored
}

TEST_CASE("hex: round trip and strict rejection", "[encoding]") {
  const Bytes b = {0x00, 0x01, 0xAB, 0xFF};
  REQUIRE(hex_encode(b) == "0001abff");
  REQUIRE(*hex_decode("0001ABFF") == b);
  REQUIRE(*hex_decode("00 01\nab ff") == b);
  REQUIRE_FALSE(hex_decode("abc").has_value());   // odd length
  REQUIRE_FALSE(hex_decode("zz").has_value());
  REQUIRE_FALSE(hex_decode("0x00").has_value());
}

TEST_CASE("file encodings round trip (raw, hex, base64)", "[encoding]") {
  const Bytes data = {1, 2, 3, 250, 251, 252, 0, 7};
  for (Encoding e : {Encoding::Raw, Encoding::Hex, Encoding::Base64}) {
    const Bytes file = encode_for_file(data, e);
    const auto back = decode_from_file(file, e);
    REQUIRE(back.has_value());
    REQUIRE(*back == data);
  }
  REQUIRE_FALSE(decode_from_file(Bytes{'z', 'z'}, Encoding::Hex).has_value());
  REQUIRE_FALSE(decode_from_file(Bytes{'*'}, Encoding::Base64).has_value());
}

TEST_CASE("parse_encoding rejects unknown names", "[encoding][negative]") {
  REQUIRE(parse_encoding("raw") == Encoding::Raw);
  REQUIRE(parse_encoding("hex") == Encoding::Hex);
  REQUIRE(parse_encoding("base64") == Encoding::Base64);
  REQUIRE_THROWS_AS(parse_encoding("base32"), Error);
}
