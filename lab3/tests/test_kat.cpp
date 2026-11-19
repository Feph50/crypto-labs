#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <sstream>

#include "errors.hpp"
#include "kat.hpp"
#include "test_support.hpp"

using namespace rsalab;
using namespace testsupport;

TEST_CASE("KAT runner passes on the shipped official vectors", "[kat]") {
  std::ostringstream os;
  const KatSummary s = run_kat_file(RSALAB_KAT_FILE, os);
  INFO(os.str());
  REQUIRE(s.ok());
  REQUIRE(s.failed == 0);
  REQUIRE(s.total >= 100);  // 37 Wycheproof OAEP + 75 NIST GCM + NIST FAIL cases
}

TEST_CASE("KAT runner detects a wrong expected value", "[kat][negative]") {
  const auto raw = read_file(RSALAB_KAT_FILE, 64u << 20);
  auto j = nlohmann::json::parse(raw.begin(), raw.end());
  std::string ct = j["aes256_gcm"]["tests"][0]["tag_hex"].get<std::string>();
  ct[0] = ct[0] == '0' ? '1' : '0';
  j["aes256_gcm"]["tests"][0]["tag_hex"] = ct;
  // a valid OAEP vector now expects a different message
  for (auto& t : j["rsa_oaep_decrypt"]["tests"])
    if (t["result"] == "valid" && !t["msg_hex"].get<std::string>().empty()) {
      t["msg_hex"] = "00";
      break;
    }
  const auto dir = temp_dir("kat");
  const std::string path = (dir / "bad.json").string();
  const std::string text = j.dump();
  write_file(path, reinterpret_cast<const uint8_t*>(text.data()), text.size(), true);
  std::ostringstream os;
  const KatSummary s = run_kat_file(path, os);
  REQUIRE_FALSE(s.ok());
  REQUIRE(s.failed >= 2);
  REQUIRE(os.str().find("FAIL") != std::string::npos);
}

TEST_CASE("a KAT file that is not valid JSON or has missing sections never reports success", "[kat][negative]") {
  const auto dir = temp_dir("kat2");
  auto put = [&](const char* name, const std::string& s) {
    const std::string p = (dir / name).string();
    write_file(p, reinterpret_cast<const uint8_t*>(s.data()), s.size(), true);
    return p;
  };
  std::ostringstream os;
  REQUIRE_THROWS_AS(run_kat_file(put("broken.json", "[ {\"name\": \"x\""), os), Error);
  REQUIRE_THROWS_AS(run_kat_file(put("empty.json", "{}"), os), Error);
  REQUIRE_THROWS_AS(run_kat_file((dir / "missing.json").string(), os), Error);
}
