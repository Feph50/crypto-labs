# End-to-end test of the rsatool command line. Run by ctest as:
#   cmake -DTOOL=<rsatool> -DWORK=<scratch dir> -P cli_e2e.cmake
# Checks exit codes and that no output file is left behind when a command fails.
if(NOT TOOL OR NOT WORK)
    message(FATAL_ERROR "usage: cmake -DTOOL=<rsatool> -DWORK=<dir> -P cli_e2e.cmake")
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

set(PASS 0)
function(run expected)
    execute_process(COMMAND "${TOOL}" ${ARGN} RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err WORKING_DIRECTORY "${WORK}")
    if(NOT rc STREQUAL "${expected}")
        message(FATAL_ERROR "FAILED: rsatool ${ARGN}\n  expected exit ${expected}, got ${rc}\n  stderr: ${err}")
    endif()
    set(LAST_ERR "${err}" PARENT_SCOPE)
    set(LAST_OUT "${out}" PARENT_SCOPE)
    math(EXPR n "${PASS} + 1")
    set(PASS ${n} PARENT_SCOPE)
endfunction()
function(expect_absent path)
    if(EXISTS "${WORK}/${path}")
        message(FATAL_ERROR "FAILED: '${path}' must not exist after a failed command")
    endif()
endfunction()
function(expect_same a b)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files "${WORK}/${a}" "${WORK}/${b}" RESULT_VARIABLE rc)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "FAILED: ${a} and ${b} differ")
    endif()
endfunction()

# ---- key generation: PEM + DER + metadata
run(0 keygen --pub pub.pem --priv priv.pem)
foreach(f pub.pem pub.der priv.pem priv.der priv.meta.json)
    if(NOT EXISTS "${WORK}/${f}")
        message(FATAL_ERROR "FAILED: keygen did not write ${f}")
    endif()
endforeach()
file(READ "${WORK}/priv.meta.json" META)
foreach(k creation_time modulus_bits hash "3072" "SHA-256")
    if(NOT META MATCHES "${k}")
        message(FATAL_ERROR "FAILED: metadata lacks ${k}: ${META}")
    endif()
endforeach()
run(2 keygen --pub pub.pem --priv priv.pem)           # refuses to overwrite
run(0 keygen --pub pub.pem --priv priv.pem --force)   # ... unless --force
run(1 keygen --bits 2048 --pub p2.pem --priv k2.pem)  # unsupported size
run(1 keygen --bits abc --pub p2.pem --priv k2.pem)
run(0 keygen --pub other_pub.pem --priv other_priv.pem)

# ---- direct mode (short message), with and without a label
file(WRITE "${WORK}/msg.txt" "hello rsatool direct mode")
run(0 encrypt --in msg.txt --pub pub.pem --out ct.bin --verbose)
if(NOT LAST_ERR MATCHES "direct")
    message(FATAL_ERROR "FAILED: a short message must use direct mode: ${LAST_ERR}")
endif()
run(0 decrypt --in ct.bin --priv priv.pem --out dec.txt)
expect_same(msg.txt dec.txt)

run(0 encrypt --in msg.txt --pub pub.pem --out ctl.bin --label project-x)
run(0 decrypt --in ctl.bin --priv priv.pem --out decl.txt --label project-x)
expect_same(msg.txt decl.txt)
run(3 decrypt --in ctl.bin --priv priv.pem --out bad1.txt --label other)   # wrong label
expect_absent(bad1.txt)
run(3 decrypt --in ctl.bin --priv priv.pem --out bad2.txt)                 # missing label
expect_absent(bad2.txt)
if(NOT LAST_ERR MATCHES "decryption failed")
    message(FATAL_ERROR "FAILED: the failure message must be the generic one: ${LAST_ERR}")
endif()

# ---- hybrid mode (message longer than 318 bytes)
set(LONG "")
foreach(i RANGE 1 200)
    string(APPEND LONG "line ${i}: hybrid mode test data 0123456789\n")
endforeach()
file(WRITE "${WORK}/long.txt" "${LONG}")
run(0 encrypt --in long.txt --pub pub.pem --out long.enc --verbose)
if(NOT LAST_ERR MATCHES "hybrid")
    message(FATAL_ERROR "FAILED: a long message must switch to hybrid mode: ${LAST_ERR}")
endif()
run(0 decrypt --in long.enc --priv priv.pem --out long.dec)
expect_same(long.txt long.dec)
run(0 encrypt --in msg.txt --pub pub.pem --out forced.enc --force-hybrid --verbose)
if(NOT LAST_ERR MATCHES "hybrid")
    message(FATAL_ERROR "FAILED: --force-hybrid ignored")
endif()

# ---- encodings: hex and base64 files, and a mismatching encoding
run(0 encrypt --in long.txt --pub pub.pem --out long.hex --encode hex)
run(0 decrypt --in long.hex --priv priv.pem --out long.hex.dec --encode hex)
expect_same(long.txt long.hex.dec)
run(0 encrypt --in long.txt --pub pub.pem --out long.b64 --encode base64)
run(0 decrypt --in long.b64 --priv priv.pem --out long.b64.dec --encode base64)
expect_same(long.txt long.b64.dec)
run(3 decrypt --in long.hex --priv priv.pem --out wrongenc.txt)                    # hex file read as raw
expect_absent(wrongenc.txt)
run(3 decrypt --in long.enc --priv priv.pem --out wrongenc2.txt --encode base64)  # raw file read as base64
expect_absent(wrongenc2.txt)
run(0 encrypt --text "from the command line" --pub pub.pem)                       # on-screen output (hex)
if(NOT LAST_OUT MATCHES "^52534145")
    message(FATAL_ERROR "FAILED: on-screen ciphertext must be hex starting with the RSAE magic: ${LAST_OUT}")
endif()

# ---- tampering and wrong key
file(READ "${WORK}/long.hex" HEXTEXT)
string(LENGTH "${HEXTEXT}" HLEN)
math(EXPR POS "${HLEN} - 20")
string(SUBSTRING "${HEXTEXT}" 0 ${POS} HEAD)
string(SUBSTRING "${HEXTEXT}" ${POS} 1 CH)
if(CH STREQUAL "0")
    set(CH2 "1")
else()
    set(CH2 "0")
endif()
math(EXPR POS1 "${POS} + 1")
string(SUBSTRING "${HEXTEXT}" ${POS1} -1 TAIL)
file(WRITE "${WORK}/long.tampered.hex" "${HEAD}${CH2}${TAIL}")
run(3 decrypt --in long.tampered.hex --priv priv.pem --out tampered.txt --encode hex)
expect_absent(tampered.txt)
run(3 decrypt --in long.enc --priv other_priv.pem --out wrongkey.txt)               # hybrid, wrong key
expect_absent(wrongkey.txt)
run(3 decrypt --in ct.bin --priv other_priv.pem --out wrongkey2.txt)                # direct, wrong key
expect_absent(wrongkey2.txt)

# ---- usage and I/O errors
run(1 encrypt --pub pub.pem --out x.bin)                                          # no input
run(1 encrypt --in msg.txt --text hi --pub pub.pem --out x.bin)                   # both inputs
run(1 encrypt --in msg.txt --pub pub.pem --out x.bin --encode base32)             # unknown encoding
run(1 frobnicate)
run(1 encrypt --in msg.txt --pub pub.pem --out x.bin --no-such-option)
run(2 encrypt --in missing.txt --pub pub.pem --out x.bin)                         # missing input
run(2 encrypt --in msg.txt --pub missing.pem --out x.bin)                         # missing key
run(2 encrypt --in msg.txt --pub priv.pem --out x.bin)                            # a private key is not a public key
run(2 decrypt --in ct.bin --priv pub.pem --out x.txt)                             # a public key is not a private key
run(2 encrypt --in msg.txt --pub pub.pem --out ct.bin)                            # output exists, no --force
run(0 encrypt --in msg.txt --pub pub.pem --out ct.bin --force)
file(WRITE "${WORK}/garbage.pem" "-----BEGIN PUBLIC KEY-----\nAAAA\n-----END PUBLIC KEY-----\n")
run(2 encrypt --in msg.txt --pub garbage.pem --out x.bin)                         # corrupted PEM
expect_absent(x.bin)

# ---- KAT command: a broken file must not report success
file(WRITE "${WORK}/bad_kat.json" "[ {\"name\": \"x\"")
run(2 --kat bad_kat.json)

message(STATUS "rsatool CLI end-to-end: ${PASS} checks passed")
