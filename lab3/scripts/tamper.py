#!/usr/bin/env python3
"""Create a tampered copy of an rsatool ciphertext file (raw container format) for the manual negative tests.

    python3 scripts/tamper.py --in ct.bin --out ct_bad.bin --mode flip-payload

Container: "RSAE" | version 0x01 | header_len (u32 big-endian) | header (JSON) | payload.   Modes:
  flip-direct        flip one bit in the first-quarter of the payload (RSA ciphertext of a direct-mode file)
  flip-payload       flip one bit in the middle of the payload (AES-GCM ciphertext of a hybrid file)
  flip-tag           flip one bit of the "tag" value in the header
  flip-wrapped-key   flip one bit of the "wrapped_key" value in the header
  flip-header-iv     flip one bit of the "iv" value in the header
  change-modulus     rewrite "rsa_modulus":3072 as 4096 in the header
  add-header-field   add an unknown field to the header (header_len is updated, so the container stays well formed)
  bad-header-len     put a header_len larger than the file
  bad-magic          change the first byte of the magic
  truncate           drop the last byte
  extract-payload    write only the payload (for interoperability checks with other tools)
Only works with ciphertexts written with --encode raw. Uses the Python standard library only.
"""
import argparse
import base64
import json
import sys

MODES = ["flip-direct", "flip-payload", "flip-tag", "flip-wrapped-key", "flip-header-iv", "change-modulus", "add-header-field",
         "bad-header-len", "bad-magic", "truncate", "extract-payload"]


def parse(data):
    if len(data) < 9 or data[:4] != b"RSAE" or data[4] != 1:
        sys.exit("not an rsatool container (raw format expected)")
    hl = int.from_bytes(data[5:9], "big")
    if 9 + hl > len(data):
        sys.exit("corrupt container")
    return data[9:9 + hl].decode("utf-8"), data[9 + hl:]


def build(header, payload):
    h = header.encode("utf-8")
    return b"RSAE" + bytes([1]) + len(h).to_bytes(4, "big") + h + payload


def flip_bit(b, index):
    b = bytearray(b)
    b[index] ^= 0x01
    return bytes(b)


def flip_field(header, field):
    obj = json.loads(header)
    raw = bytearray(base64.b64decode(obj[field]))
    raw[0] ^= 0x01
    obj[field] = base64.b64encode(bytes(raw)).decode("ascii")
    return json.dumps(obj, separators=(",", ":"))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--in", dest="inp", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--mode", required=True, choices=MODES)
    a = ap.parse_args()
    data = open(a.inp, "rb").read()
    header, payload = parse(data)
    m = a.mode
    if m == "flip-direct":
        out = build(header, flip_bit(payload, max(0, len(payload) // 4)))
    elif m == "flip-payload":
        out = build(header, flip_bit(payload, len(payload) // 2))
    elif m == "flip-tag":
        out = build(flip_field(header, "tag"), payload)
    elif m == "flip-wrapped-key":
        out = build(flip_field(header, "wrapped_key"), payload)
    elif m == "flip-header-iv":
        out = build(flip_field(header, "iv"), payload)
    elif m == "change-modulus":
        if '"rsa_modulus":3072' not in header:
            sys.exit('header has no "rsa_modulus":3072')
        out = build(header.replace('"rsa_modulus":3072', '"rsa_modulus":4096'), payload)
    elif m == "add-header-field":
        out = build(header[:-1] + ',"extra":1}', payload)
    elif m == "bad-header-len":
        out = data[:5] + (len(data) + 1000).to_bytes(4, "big") + data[9:]
    elif m == "bad-magic":
        out = b"X" + data[1:]
    elif m == "truncate":
        out = data[:-1]
    else:  # extract-payload
        out = payload
    with open(a.out, "wb") as f:
        f.write(out)
    print("wrote %s (%d bytes, mode %s)" % (a.out, len(out), m))


if __name__ == "__main__":
    main()
