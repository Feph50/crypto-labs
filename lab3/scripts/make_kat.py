#!/usr/bin/env python3
"""Build lab3/kat/vectors.json from the OFFICIAL test-vector files (nothing is invented or edited by hand).

Sources (download them yourself; the expected SHA-256 values below pin the exact files that were used):

  1. RSAES-OAEP, RSA-3072, SHA-256, MGF1-SHA-256, with labels  (Project Wycheproof, maintained by C2SP; originally Google)
       https://github.com/C2SP/wycheproof/blob/main/testvectors_v1/rsa_oaep_3072_sha256_mgf1sha256_test.json
       raw: https://raw.githubusercontent.com/C2SP/wycheproof/main/testvectors_v1/rsa_oaep_3072_sha256_mgf1sha256_test.json
  2. AES-GCM known answers, NIST CAVP (CAVS 14.0): gcmEncryptExtIV256.rsp and gcmDecrypt256.rsp inside gcmtestvectors.zip
       page: https://csrc.nist.gov/projects/cryptographic-algorithm-validation-program/cavp-testing-block-cipher-modes
       file: https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Algorithm-Validation-Program/documents/mac/gcmtestvectors.zip

Usage:
    python3 scripts/make_kat.py --wycheproof PATH/rsa_oaep_3072_sha256_mgf1sha256_test.json --gcm-zip PATH/gcmtestvectors.zip
Output: kat/vectors.json (the OAEP tests are copied in full; the GCM tests are the first 3 cases of every
group with Keylen=256, IVlen=96, Taglen=128 -> 75 encrypt cases, plus the first FAIL cases of gcmDecrypt256.rsp).
"""
import argparse
import hashlib
import json
import pathlib
import re
import sys
import zipfile

EXPECTED = {
    "wycheproof": "c8122302f522bfd539650c71c4aa966bd1f89acb50c6f4c190030fdb9b2684a4",
    "gcm_zip": "f9fc479e134cde2980b3bb7cddbcb567b2cd96fd753835243ed067699f26a023",
}
PER_GROUP = 3
MAX_REJECT = 12


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def parse_rsp(text):
    groups, cur = [], None
    for blk in re.split(r"\n\s*\n", text):
        lines = [l.strip() for l in blk.strip().split("\n") if l.strip() and not l.startswith("#")]
        if not lines:
            continue
        if lines[0].startswith("["):
            hdr = {}
            for l in lines:
                m = re.match(r"\[(\w+) = (\d+)\]", l)
                hdr[m.group(1)] = int(m.group(2))
            cur = {"hdr": hdr, "cases": []}
            groups.append(cur)
        else:
            d = {"FAIL": False}
            for l in lines:
                if l == "FAIL":
                    d["FAIL"] = True
                    continue
                k, _, v = l.partition("=")
                d[k.strip()] = v.strip()
            cur["cases"].append(d)
    return groups


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--wycheproof", required=True)
    ap.add_argument("--gcm-zip", required=True)
    ap.add_argument("--out", default="kat/vectors.json")
    a = ap.parse_args()

    for name, path in (("wycheproof", a.wycheproof), ("gcm_zip", a.gcm_zip)):
        got = sha256(path)
        if got != EXPECTED[name]:
            sys.exit("%s: SHA-256 %s differs from the pinned file %s (different version of the official file)" % (path, got, EXPECTED[name]))

    wy = json.loads(pathlib.Path(a.wycheproof).read_text(encoding="utf-8"))
    assert wy["algorithm"] == "RSAES-OAEP" and len(wy["testGroups"]) == 1
    g = wy["testGroups"][0]
    assert g["keySize"] == 3072 and g["sha"] == "SHA-256" and g["mgf"] == "MGF1" and g["mgfSha"] == "SHA-256"
    oaep_tests = []
    for t in g["tests"]:
        if t["result"] not in ("valid", "invalid"):
            continue  # 'acceptable' results are implementation-defined: not used as a pass/fail criterion
        oaep_tests.append({
            "id": "wycheproof-tc%d" % t["tcId"], "comment": t["comment"], "flags": t["flags"],
            "label_hex": t["label"], "ct_hex": t["ct"], "msg_hex": t["msg"], "result": t["result"],
        })

    z = zipfile.ZipFile(a.gcm_zip)
    enc = parse_rsp(z.read("gcmEncryptExtIV256.rsp").decode())
    dec = parse_rsp(z.read("gcmDecrypt256.rsp").decode())
    gcm_tests = []
    for grp in enc:
        h = grp["hdr"]
        if h["Keylen"] != 256 or h["IVlen"] != 96 or h["Taglen"] != 128:
            continue
        for c in grp["cases"][:PER_GROUP]:
            gcm_tests.append({
                "id": "cavp-enc-pt%d-aad%d-c%s" % (h["PTlen"], h["AADlen"], c["Count"]),
                "key_hex": c["Key"], "iv_hex": c["IV"], "aad_hex": c["AAD"], "pt_hex": c["PT"], "ct_hex": c["CT"], "tag_hex": c["Tag"],
            })
    reject = []
    for grp in dec:
        h = grp["hdr"]
        if h["Keylen"] != 256 or h["IVlen"] != 96 or h["Taglen"] != 128:
            continue
        for c in grp["cases"]:
            if c["FAIL"] and len(reject) < MAX_REJECT:
                reject.append({
                    "id": "cavp-dec-fail-pt%d-aad%d-c%s" % (h["PTlen"], h["AADlen"], c["Count"]),
                    "key_hex": c["Key"], "iv_hex": c["IV"], "aad_hex": c["AAD"], "ct_hex": c["CT"], "tag_hex": c["Tag"],
                })

    out = {
        "meta": {
            "generated_by": "scripts/make_kat.py",
            "note": "Vectors are copied from the official sources unchanged; see kat/SOURCES.md",
            "sources": [
                {"name": "Project Wycheproof (C2SP): rsa_oaep_3072_sha256_mgf1sha256_test.json",
                 "url": "https://github.com/C2SP/wycheproof/blob/main/testvectors_v1/rsa_oaep_3072_sha256_mgf1sha256_test.json",
                 "sha256": EXPECTED["wycheproof"]},
                {"name": "NIST CAVP GCM test vectors (CAVS 14.0): gcmEncryptExtIV256.rsp, gcmDecrypt256.rsp in gcmtestvectors.zip",
                 "url": "https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Algorithm-Validation-Program/documents/mac/gcmtestvectors.zip",
                 "page": "https://csrc.nist.gov/projects/cryptographic-algorithm-validation-program/cavp-testing-block-cipher-modes",
                 "sha256": EXPECTED["gcm_zip"]},
            ],
        },
        "rsa_oaep_decrypt": {
            "description": "RSAES-OAEP, RSA-3072, SHA-256, MGF1-SHA-256. valid: must decrypt to msg; invalid: must be rejected",
            "private_key_pkcs8_hex": g["privateKeyPkcs8"],
            "tests": oaep_tests,
        },
        "aes256_gcm": {
            "description": "AES-256-GCM, 96-bit IV, 128-bit tag: encrypt must give ct and tag; decrypt must give pt",
            "tests": gcm_tests,
        },
        "aes256_gcm_reject": {
            "description": "AES-256-GCM authentication must FAIL (NIST gcmDecrypt256.rsp FAIL cases)",
            "tests": reject,
        },
    }
    pathlib.Path(a.out).parent.mkdir(parents=True, exist_ok=True)
    pathlib.Path(a.out).write_text(json.dumps(out, indent=1) + "\n", encoding="utf-8")
    print("wrote %s: %d OAEP (%d valid), %d GCM, %d GCM reject" % (
        a.out, len(oaep_tests), sum(t["result"] == "valid" for t in oaep_tests), len(gcm_tests), len(reject)))


if __name__ == "__main__":
    main()
