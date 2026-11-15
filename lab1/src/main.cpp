// aestool - Lab 1: AES symmetric encryption voi Crypto++
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <set>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

#include "aescore.hpp"

using namespace aesl;
using namespace std;

// Ma thoat: 0 OK | 1 loi dau vao/cu phap | 2 giai ma / xac thuc that bai | 3 KAT fail
static const int EXIT_OK = 0, EXIT_USAGE = 1, EXIT_AUTH = 2, EXIT_KAT = 3;

struct Args {
    string cmd;
    map<string, string> kv;
    set<string> flags;
    bool has(const string& k) const { return kv.count(k) > 0; }
    string get(const string& k, const string& d = "") const {
        auto it = kv.find(k);
        return it == kv.end() ? d : it->second;
    }
    bool flag(const string& k) const { return flags.count(k) > 0; }
};

static const set<string> BOOL_FLAGS = {"aead", "allow-ecb", "verbose", "hex", "quick", "help"};

// Argument vector as UTF-8. On Windows argv[] uses the ANSI code page, so re-read the real UTF-16 command line.
static vector<string> getArgs(int argc, char** argv) {
    vector<string> v;
#if defined(_WIN32)
    (void)argc; (void)argv;
    int n = 0;
    LPWSTR* w = CommandLineToArgvW(GetCommandLineW(), &n);
    if (w) {
        for (int i = 0; i < n; ++i) {
            int len = WideCharToMultiByte(CP_UTF8, 0, w[i], -1, nullptr, 0, nullptr, nullptr);
            string s((size_t)(len > 0 ? len - 1 : 0), '\0');
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

static Args parseArgs(const vector<string>& argv) {
    Args a;
    const int argc = (int)argv.size();
    int i = 1;
    if (i < argc && argv[i].rfind("--", 0) != 0) a.cmd = argv[i++];
    for (; i < argc; ++i) {
        string t = argv[i];
        if (t.rfind("--", 0) != 0) throw Error("unexpected argument: " + t);
        string k = t.substr(2);
        if (BOOL_FLAGS.count(k)) { a.flags.insert(k); continue; }
        if (i + 1 >= argc) throw Error("option --" + k + " requires a value");
        a.kv[k] = argv[++i];
    }
    return a;
}

static void usage() {
    cout <<
R"(aestool - AES (Crypto++) Lab 1

  aestool keygen  --bits 128|192|256 [--hex] --out key.bin
  aestool encrypt --mode ecb|cbc|ofb|cfb|ctr|xts|ccm|gcm (--in F | --text "..")
                  (--key F | --key-hex H) [--iv F|HEX | --nonce F|HEX] [--aead]
                  [--aad F | --aad-text S] [--tag-len 4|8|16 (ccm)] [--out F]
                  [--meta F.json] [--encode hex|base64|raw] [--allow-ecb]
                  [--nonce-db F]
  aestool decrypt --mode .. (--in F | --text ENCODED_CT) (--key..) [--iv ..]
                  [--tag-hex H] [--aad..] [--meta F.json] [--out F]
  aestool --kat vectors.json        (hoac: aestool kat --kat vectors.json)
  aestool bench   [--runs 30..100] [--threads N] [--warmup SEC] [--ops N] [--budget-mb MiB] [--out bench.csv] [--quick]
Exit code: 0 ok, 1 input error, 2 decrypt/auth failure, 3 KAT failure
)";
}

// ---------------------------------------------------------------- helpers
static string encodeOut(const string& s, const string& enc) {
    if (enc == "hex") return toHex(s);
    if (enc == "base64") return toB64(s);
    if (enc == "raw") return s;
    throw Error("--encode must be hex|base64|raw");
}
static string decodeIn(const string& s, const string& enc) {
    if (enc == "hex") return fromHex(s);
    if (enc == "base64") return fromB64(s);
    if (enc == "raw") return s;
    throw Error("--encode must be hex|base64|raw");
}
static string loadKey(const Args& a) {
    if (a.has("key-hex")) return fromHex(a.get("key-hex"));
    if (a.has("key")) return loadKeyFile(a.get("key"));
    throw Error("missing --key or --key-hex");
}
// --iv / --nonce: ten file (raw hoac hex) hoac chuoi hex truc tiep
static bool loadIv(const Args& a, string& iv) {
    string v = a.has("iv") ? a.get("iv") : a.get("nonce");
    if (v.empty()) return false;
    if (fileExists(v)) {
        string d = readFile(v);
        string t;
        for (char c : d) if (!std::isspace(static_cast<unsigned char>(c))) t.push_back(c);
        bool allHex = !t.empty() && std::all_of(t.begin(), t.end(), [](unsigned char c) { return std::isxdigit(c); });
        iv = (allHex && t.size() % 2 == 0 && d.size() != t.size() / 2) ? fromHex(t) : d;
    } else {
        iv = fromHex(v);
    }
    return true;
}
static string loadAad(const Args& a) {
    if (a.has("aad")) return readFile(a.get("aad"));
    if (a.has("aad-text")) return a.get("aad-text");
    return "";
}
static string loadInput(const Args& a, bool decrypting) {
    if (a.has("in") && a.has("text")) throw Error("use only one of --in / --text");
    if (a.has("in")) return readFile(a.get("in"));
    if (a.has("text")) return decrypting ? decodeIn(a.get("text"), a.get("encode", "hex")) : a.get("text");
    throw Error("missing --in or --text");
}

// ---------------------------------------------------------------- keygen
static int cmdKeygen(const Args& a) {
    int bits = std::stoi(a.get("bits", "256"));
    if (bits != 128 && bits != 192 && bits != 256 && bits != 512) throw Error("--bits must be 128|192|256 (512 for XTS)");
    if (!a.has("out")) throw Error("missing --out");
    string k = randomBytes(bits / 8);
    writeFile(a.get("out"), a.flag("hex") ? makeHexKeyFile(k) : k);
    cout << "Key (" << bits << " bits) written to " << a.get("out") << "\n"
              << "WARNING: protect this file (chmod 600); never commit real keys.\n";
    return EXIT_OK;
}

// ---------------------------------------------------------------- encrypt
static int cmdEncrypt(const Args& a) {
    Opts o;
    o.mode = a.get("mode");
    if (!validMode(o.mode)) throw Error("--mode must be ecb|cbc|ofb|cfb|ctr|xts|ccm|gcm");
    if (isAead(o.mode) && !a.flag("aead")) throw Error("mode " + o.mode + " is AEAD: pass --aead");
    if (!isAead(o.mode) && a.flag("aead")) throw Error("--aead only valid with ccm|gcm");
    if (!isAead(o.mode) && (a.has("aad") || a.has("aad-text"))) throw Error("AAD only valid with --aead modes");
    o.key = loadKey(a);
    o.aad = loadAad(a);
    if (a.has("tag-len")) {
        if (o.mode != "ccm") throw Error("--tag-len only valid for ccm");
        o.tagLen = std::stoul(a.get("tag-len"));
    }
    string pt = loadInput(a, false);

    if (o.mode == "ecb") {
        cerr << "WARNING: ECB leaks plaintext patterns and is NOT semantically secure. Do not use for real data.\n";
        if (pt.size() > 16 * 1024 && !a.flag("allow-ecb"))
            throw Error("ECB blocked for inputs > 16 KiB (override with --allow-ecb)");
    }
    // IV / nonce
    bool given = loadIv(a, o.iv);
    if (o.mode == "ecb") {
        if (given) throw Error("ECB does not use an IV");
    } else if (!given) {
        o.iv = randomBytes(defaultIvLen(o.mode));   // AutoSeededRandomPool
        if (a.flag("verbose")) cerr << "[info] IV/nonce auto-generated (" << o.iv.size() << " bytes)\n";
    }
    if (!keyLenOk(o.mode, o.key.size())) throw Error("invalid key length for mode " + o.mode);
    if (!ivLenOk(o.mode, o.iv.size())) throw Error("invalid IV/nonce length for mode " + o.mode);

    // Nonce-reuse protection
    const string db = a.get("nonce-db", ".aestool_nonces");
    if (nonceUsed(db, o)) throw Error("REJECTED: this (key, nonce) pair was already used (nonce reuse is catastrophic)");

    Out r = encrypt(o, pt);
    nonceRecord(db, o);

    const string enc = a.get("encode", "hex");
    if (a.has("out")) {
        writeFile(a.get("out"), r.data);                       // ciphertext raw
        string meta = a.get("meta", a.get("out") + ".json");
        vector<std::pair<string, string>> kv = {
            {"alg", algName(o.mode, o.key.size())}, {"mode", o.mode}, {"iv", toHex(o.iv)}};
        if (isAead(o.mode)) {
            kv.push_back({"aad", toHex(o.aad)});
            kv.push_back({"tag", toHex(r.tag)});
            kv.push_back({"tag_len", std::to_string(r.tag.size())});
        }
        writeFile(meta, toJson(kv));
        cerr << "ciphertext -> " << a.get("out") << " ; header -> " << meta << "\n";
    }
    if (enc == "raw") { if (!a.has("out")) cout << r.data; }
    else {
        cout << "alg:        " << algName(o.mode, o.key.size()) << "\n";
        if (!o.iv.empty()) cout << "iv:         " << toHex(o.iv) << "\n";
        cout << "ciphertext: " << encodeOut(r.data, enc) << "\n";
        if (isAead(o.mode)) cout << "tag:        " << toHex(r.tag) << "\n";
    }
    return EXIT_OK;
}

// ---------------------------------------------------------------- decrypt
static int cmdDecrypt(const Args& a) {
    Opts o;
    map<string, string> meta;
    string metaPath = a.get("meta", a.has("in") ? a.get("in") + ".json" : "");
    if (!metaPath.empty() && fileExists(metaPath)) meta = parseJsonObjects(readFile(metaPath)).at(0);

    o.mode = a.get("mode", meta.count("mode") ? meta["mode"] : "");
    if (!validMode(o.mode)) throw Error("--mode must be ecb|cbc|ofb|cfb|ctr|xts|ccm|gcm");
    if (meta.count("mode") && meta["mode"] != o.mode) throw Error("header mode does not match --mode");
    if (isAead(o.mode) && !a.flag("aead")) throw Error("mode " + o.mode + " is AEAD: pass --aead");
    if (!isAead(o.mode) && a.flag("aead")) throw Error("--aead only valid with ccm|gcm");
    if (o.mode == "ecb")
        cerr << "WARNING: ECB leaks plaintext patterns and is NOT semantically secure. Do not use for real data.\n";
    o.key = loadKey(a);
    if (a.has("aad") || a.has("aad-text")) o.aad = loadAad(a);
    else if (meta.count("aad")) o.aad = fromHex(meta["aad"]);
    if (!loadIv(a, o.iv)) {
        if (o.mode == "ecb") o.iv.clear();
        else if (meta.count("iv")) o.iv = fromHex(meta["iv"]);
        else throw Error("missing IV/nonce (no --iv and no header)");
    }
    string tag;
    if (isAead(o.mode)) {
        if (a.has("tag-hex")) tag = fromHex(a.get("tag-hex"));
        else if (meta.count("tag")) tag = fromHex(meta["tag"]);
        else throw Error("missing authentication tag");
        o.tagLen = tag.size();
    }
    string ct = loadInput(a, true);
    string pt = decrypt(o, ct, tag);   // throw Error neu that bai -> khong ghi file

    if (a.has("out")) {
        writeFile(a.get("out"), pt);
        cerr << "plaintext -> " << a.get("out") << "\n";
    } else if (a.has("encode")) {
        cout << encodeOut(pt, a.get("encode")) << "\n";
    } else {
        cout << pt << "\n";
    }
    return EXIT_OK;
}

// ---------------------------------------------------------------- KAT
static int cmdKat(const string& path) {
    auto vecs = parseJsonObjects(readFile(path));
    int pass = 0, fail = 0;
    for (auto& v : vecs) {
        string name = v.count("name") ? v["name"] : "(unnamed)";
        bool ok = false;
        try {
            Opts o;
            o.mode = v.at("mode");
            o.key = fromHex(v.at("key"));
            o.iv = v.count("iv") ? fromHex(v["iv"]) : "";
            o.aad = v.count("aad") ? fromHex(v["aad"]) : "";
            o.pkcs = false;     // vector NIST khong co padding
            string pt = fromHex(v.at("pt")), ct = fromHex(v.at("ct"));
            string tag = v.count("tag") ? fromHex(v["tag"]) : "";
            if (!tag.empty()) o.tagLen = tag.size();
            Out r = encrypt(o, pt);
            bool encOk = (r.data == ct) && (r.tag == tag);
            bool decOk = (decrypt(o, ct, tag) == pt);
            ok = encOk && decOk;
        } catch (const std::exception& e) {
            ok = false;
        }
        cout << (ok ? "[PASS] " : "[FAIL] ") << name << "\n";
        ok ? ++pass : ++fail;
    }
    cout << "---- KAT summary: " << pass << " passed, " << fail << " failed, " << (pass + fail) << " total\n";
    return fail ? EXIT_KAT : EXIT_OK;
}

// ---------------------------------------------------------------- bench
struct Stats { double mean, median, sd, ci95; };
static Stats stats(vector<double> x) {
    size_t n = x.size();
    double m = 0;
    for (double v : x) m += v;
    m /= n;
    double ss = 0;
    for (double v : x) ss += (v - m) * (v - m);
    double sd = n > 1 ? std::sqrt(ss / (n - 1)) : 0;
    std::sort(x.begin(), x.end());
    double med = n % 2 ? x[n / 2] : (x[n / 2 - 1] + x[n / 2]) / 2;
    double df = n > 1 ? n - 1 : 1;
    double t = 1.96 + 2.4 / df;      // xap xi gia tri t (chinh xac ~1% voi df>=9)
    return {m, med, sd, t * sd / std::sqrt((double)n)};
}
// Performance protocol (standardised for all labs):
//   1. warm-up 1-2 s  (here 1.5 s, run separately for EVERY mode and direction),
//   2. one block = ~1000 operations (fewer only for big payloads so that a block stays within --budget-mb of data;
//      --ops N forces N operations per block),
//   3. repeat the block N times (--runs, 30..100 recommended, default 30),
//   4. report mean / median / SD / 95% CI of throughput and latency; ops_per_block is recorded in the CSV.
static int cmdBench(const Args& a) {
    using clk = std::chrono::steady_clock;
    const int runs = std::stoi(a.get("runs", "30"));
    const bool quick = a.flag("quick");
    if (runs < 2) throw Error("--runs must be >= 2");
    if (!quick && (runs < 30 || runs > 100))
        cerr << "WARNING: the standard protocol uses 30-100 runs per case (got " << runs << ")\n";
    const int threads = std::stoi(a.get("threads", "1"));
    if (threads < 1 || threads > 256) throw Error("--threads must be 1..256");
    const double warmup = std::stod(a.get("warmup", quick ? "0.2" : "1.5"));
    if (!(warmup >= 0.0 && warmup <= 60.0)) throw Error("--warmup must be 0..60 seconds");
    const int fixedOps = std::stoi(a.get("ops", "0"));
    if (fixedOps < 0 || fixedOps > 1000000) throw Error("--ops must be 0 (auto) .. 1000000");
    const size_t budgetMb = std::stoul(a.get("budget-mb", quick ? "4" : "256"));
    if (budgetMb < 1 || budgetMb > 65536) throw Error("--budget-mb must be 1..65536");
    const size_t budget = budgetMb << 20;
    vector<size_t> sizes = {1 << 10, 4 << 10, 16 << 10, 256 << 10, 1 << 20, 8 << 20};
    vector<string> modes = {"ecb", "cbc", "ofb", "cfb", "ctr", "xts", "ccm", "gcm"};

    string csv = "mode,dir,size_bytes,ops_per_block,runs,thr_mean_MBps,thr_median_MBps,thr_sd,thr_ci95,lat_mean_us,lat_median_us,lat_sd,lat_ci95\n";
    cout << "# protocol: warm-up " << warmup << " s per mode/direction, ~1000 ops per block (budget " << budgetMb
         << " MiB, --ops to force), " << runs << " runs per case, " << threads << " thread(s)\n";
    cout << "mode dir  size      ops    MB/s(mean ±CI95)      latency us/op (mean)\n";

    auto makeOpts = [](const string& m) {
        Opts o; o.mode = m; o.key = randomBytes(m == "xts" ? 64 : 32);
        o.iv = randomBytes(defaultIvLen(m));
        if (m == "ccm") o.tagLen = 16;
        return o;
    };
    auto makePayload = [](size_t sz) {
        string pt(sz, '\0');
        string tmp = randomBytes(1024);
        for (size_t i = 0; i < sz; ++i) pt[i] = tmp[i % 1024];
        return pt;
    };

    for (auto& m : modes) for (int dir = 0; dir < 2; ++dir) {
        // 1. warm-up for THIS algorithm and direction (CPU frequency, caches, allocator)
        {
            Opts w = makeOpts(m);
            string wpt = makePayload(256 << 10);
            Out wref = encrypt(w, wpt);
            auto t0 = clk::now();
            do {
                if (dir == 0) { volatile size_t s = encrypt(w, wpt).data.size(); (void)s; }
                else { volatile size_t s = decrypt(w, wref.data, wref.tag).size(); (void)s; }
            } while (std::chrono::duration<double>(clk::now() - t0).count() < warmup);
        }
        for (size_t sz : sizes) {
            Opts o = makeOpts(m);
            string pt = makePayload(sz);
            Out ref = encrypt(o, pt);
            // 2. block of ~1000 operations (reduced only for large payloads, see header comment)
            int ops = fixedOps > 0 ? fixedOps : (int)std::max<size_t>(1, std::min<size_t>(1000, budget / sz));
            vector<double> thr, lat;
            // Each worker runs `ops` operations; encrypt()/decrypt() keep all state local, inputs are read-only.
            auto work = [&]() {
                for (int i = 0; i < ops; ++i) {
                    if (dir == 0) { volatile size_t s = encrypt(o, pt).data.size(); (void)s; }
                    else { volatile size_t s = decrypt(o, ref.data, ref.tag).size(); (void)s; }
                }
            };
            // 3. N independent runs of the block
            for (int r = 0; r < runs; ++r) {
                auto t0 = clk::now();
                if (threads == 1) {
                    work();
                } else {
                    vector<std::thread> ts;
                    for (int t = 0; t < threads; ++t) ts.emplace_back(work);
                    for (auto& t : ts) t.join();
                }
                double sec = std::chrono::duration<double>(clk::now() - t0).count();
                lat.push_back(sec / ops * 1e6);                              // wall time of one round
                thr.push_back((double)sz * ops * threads / sec / 1e6);      // aggregate over all workers
            }
            // 4. statistics
            Stats T = stats(thr), L = stats(lat);
            csv += m + "," + (dir ? "dec" : "enc") + "," + std::to_string(sz) + "," + std::to_string(ops) + "," + std::to_string(runs) + "," +
                   std::to_string(T.mean) + "," + std::to_string(T.median) + "," + std::to_string(T.sd) + "," + std::to_string(T.ci95) + "," +
                   std::to_string(L.mean) + "," + std::to_string(L.median) + "," + std::to_string(L.sd) + "," + std::to_string(L.ci95) + "\n";
            printf("%-4s %-4s %-9zu %-6d %9.1f ± %-7.1f      %10.2f\n", m.c_str(), dir ? "dec" : "enc", sz, ops, T.mean, T.ci95, L.mean);
            fflush(stdout);
        }
    }
    if (a.has("out")) { writeFile(a.get("out"), csv); cout << "CSV -> " << a.get("out") << "\n"; }
    return EXIT_OK;
}

// ---------------------------------------------------------------- main
int main(int argc, char** argv) {
    try {
        Args a = parseArgs(getArgs(argc, argv));
        if (a.flag("help") || (a.cmd.empty() && !a.has("kat"))) { usage(); return a.flag("help") ? 0 : EXIT_USAGE; }
        if (a.has("kat")) return cmdKat(a.get("kat"));
        if (a.cmd == "keygen") return cmdKeygen(a);
        if (a.cmd == "encrypt") return cmdEncrypt(a);
        if (a.cmd == "decrypt") return cmdDecrypt(a);
        if (a.cmd == "bench") return cmdBench(a);
        throw Error("unknown command: " + a.cmd);
    } catch (const Error& e) {
        string m = e.what();
        cerr << "ERROR: " << m << "\n";
        bool authFail = (m == "decryption failed" || m == "authentication failed");
        return authFail ? EXIT_AUTH : EXIT_USAGE;
    } catch (const std::exception& e) {
        cerr << "ERROR: invalid input (" << e.what() << ")\n";   // fail closed
        return EXIT_USAGE;
    }
}
