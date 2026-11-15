# Kiem thu am o muc CLI: moi buoc phai co ma thoat dung ky vong
file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK})
set(ENV{HOME} ${WORK})
function(run expect_nonzero)
  execute_process(COMMAND ${TOOL} ${ARGN} WORKING_DIRECTORY ${WORK}
                  RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
  if(expect_nonzero AND rc EQUAL 0)
    message(FATAL_ERROR "expected failure but succeeded: ${ARGN}")
  elseif(NOT expect_nonzero AND NOT rc EQUAL 0)
    message(FATAL_ERROR "expected success but failed (${rc}): ${ARGN}")
  endif()
endfunction()

set(XTS_KEY "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f")
file(WRITE ${WORK}/msg.txt "hello cli negative tests")
run(0 keygen --bits 256 --out ${WORK}/key.bin)
run(0 encrypt --mode gcm --aead --key ${WORK}/key.bin --in ${WORK}/msg.txt --out ${WORK}/ct.bin)
run(0 decrypt --mode gcm --aead --key ${WORK}/key.bin --in ${WORK}/ct.bin --out ${WORK}/out.txt)
file(READ ${WORK}/out.txt got)
if(NOT got STREQUAL "hello cli negative tests")
  message(FATAL_ERROR "round trip mismatch")
endif()

# AEAD ma hoa khong co --aead -> tu choi
run(1 encrypt --mode gcm --key ${WORK}/key.bin --in ${WORK}/msg.txt --out ${WORK}/x.bin)
# IV sai do dai
run(1 encrypt --mode cbc --key ${WORK}/key.bin --iv 0011 --in ${WORK}/msg.txt --out ${WORK}/x.bin)
# Khoa sai do dai
run(1 encrypt --mode ctr --key-hex 0011223344 --in ${WORK}/msg.txt --out ${WORK}/x.bin)
# Khoa sai -> giai ma that bai
run(1 decrypt --mode gcm --aead --key-hex 000102030405060708090a0b0c0d0e0f000102030405060708090a0b0c0d0e0f
    --in ${WORK}/ct.bin --out ${WORK}/bad.txt)
# Tamper ciphertext -> that bai & khong ghi file
file(READ ${WORK}/ct.bin raw HEX)
string(SUBSTRING "${raw}" 0 2 b0)
if(b0 STREQUAL "00")
  string(REGEX REPLACE "^00" "01" raw "${raw}")
else()
  string(REGEX REPLACE "^.." "00" raw "${raw}")
endif()
string(LENGTH "${raw}" n)
set(i 0)
set(bytes "")
file(WRITE ${WORK}/tamper.hex "${raw}")
execute_process(COMMAND ${TOOL} decrypt --mode gcm --aead --key ${WORK}/key.bin
                --text ${raw} --encode hex --meta ${WORK}/ct.bin.json --out ${WORK}/tamper.out
                RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
if(rc EQUAL 0)
  message(FATAL_ERROR "tampered ciphertext was accepted")
endif()
if(EXISTS ${WORK}/tamper.out)
  message(FATAL_ERROR "plaintext file written despite auth failure")
endif()
# Nonce reuse -> tu choi
run(0 encrypt --mode gcm --aead --key ${WORK}/key.bin --iv 000000000000000000000001 --in ${WORK}/msg.txt --out ${WORK}/n1.bin)
run(1 encrypt --mode gcm --aead --key ${WORK}/key.bin --iv 000000000000000000000001 --in ${WORK}/msg.txt --out ${WORK}/n2.bin)
# ECB: file > 16KiB bi chan, co --allow-ecb thi qua
string(REPEAT "A" 20000 big)
file(WRITE ${WORK}/big.txt "${big}")
run(1 encrypt --mode ecb --key ${WORK}/key.bin --in ${WORK}/big.txt --out ${WORK}/e.bin)
run(0 encrypt --mode ecb --allow-ecb --key ${WORK}/key.bin --in ${WORK}/big.txt --out ${WORK}/e.bin)
# Hex hong
run(1 encrypt --mode ctr --key-hex ZZ --in ${WORK}/msg.txt)
# ---------------------------------------------------------------- additional fail-closed checks
# helper: flip the first hex digit of a JSON field value in a sidecar file
function(corrupt_field src dst key)
  file(READ ${src} j)
  string(REGEX MATCH "\"${key}\": \"([0-9a-f])" m "${j}")
  set(c "${CMAKE_MATCH_1}")
  if(c STREQUAL "0")
    set(r "1")
  else()
    set(r "0")
  endif()
  string(REGEX REPLACE "(\"${key}\": \")[0-9a-f]" "\\1${r}" j2 "${j}")
  file(WRITE ${dst} "${j2}")
endfunction()

# Tampered authentication TAG in the sidecar header -> authentication failure, no plaintext file
run(0 encrypt --mode gcm --aead --key ${WORK}/key.bin --in ${WORK}/msg.txt --out ${WORK}/t1.bin)
corrupt_field(${WORK}/t1.bin.json ${WORK}/t1_badtag.json tag)
execute_process(COMMAND ${TOOL} decrypt --mode gcm --aead --key ${WORK}/key.bin --in ${WORK}/t1.bin
                --meta ${WORK}/t1_badtag.json --out ${WORK}/t1_badtag.out
                RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
if(NOT rc EQUAL 2)
  message(FATAL_ERROR "tampered tag: expected exit 2 (authentication failure), got ${rc}")
endif()
if(EXISTS ${WORK}/t1_badtag.out)
  message(FATAL_ERROR "plaintext written despite a bad tag")
endif()

# Tampered IV / AAD in the header -> authentication failure
corrupt_field(${WORK}/t1.bin.json ${WORK}/t1_badiv.json iv)
execute_process(COMMAND ${TOOL} decrypt --mode gcm --aead --key ${WORK}/key.bin --in ${WORK}/t1.bin
                --meta ${WORK}/t1_badiv.json --out ${WORK}/t1_badiv.out
                RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
if(rc EQUAL 0 OR EXISTS ${WORK}/t1_badiv.out)
  message(FATAL_ERROR "tampered IV in header was accepted")
endif()

# Truncated tag (wrong length) and missing tag
run(1 decrypt --mode gcm --aead --key ${WORK}/key.bin --in ${WORK}/t1.bin --tag-hex 00112233 --out ${WORK}/t1_short.out)

# Wrong AAD -> failure (AAD is authenticated)
run(0 encrypt --mode gcm --aead --key ${WORK}/key.bin --aad-text "v1" --in ${WORK}/msg.txt --out ${WORK}/a1.bin)
run(1 decrypt --mode gcm --aead --key ${WORK}/key.bin --aad-text "v2" --in ${WORK}/a1.bin --out ${WORK}/a1.out)
run(0 decrypt --mode gcm --aead --key ${WORK}/key.bin --aad-text "v1" --in ${WORK}/a1.bin --out ${WORK}/a1.out)

# Sidecar header mode that disagrees with --mode, and a missing/corrupted header
run(1 decrypt --mode ccm --aead --key ${WORK}/key.bin --in ${WORK}/t1.bin --out ${WORK}/m.out)
file(WRITE ${WORK}/garbage.json "this is not json")
run(1 decrypt --mode gcm --aead --key ${WORK}/key.bin --in ${WORK}/t1.bin --meta ${WORK}/garbage.json --out ${WORK}/g.out)

# Key files: corrupted hex header, missing file, wrong length
file(WRITE ${WORK}/badkey.hex "AESKEY-HEX\nnot-hex-at-all\n")
run(1 encrypt --mode ctr --key ${WORK}/badkey.hex --in ${WORK}/msg.txt --out ${WORK}/k1.bin)
run(1 encrypt --mode ctr --key ${WORK}/no_such_key.bin --in ${WORK}/msg.txt --out ${WORK}/k2.bin)
file(WRITE ${WORK}/shortkey.bin "0123456789")
run(1 encrypt --mode ctr --key ${WORK}/shortkey.bin --in ${WORK}/msg.txt --out ${WORK}/k3.bin)
# a valid hex-with-header key file works
run(0 keygen --bits 128 --hex --out ${WORK}/key128.hex)
run(0 encrypt --mode cbc --key ${WORK}/key128.hex --in ${WORK}/msg.txt --out ${WORK}/k4.bin)

# Non-AEAD tamper is NOT detected (documented): decrypt succeeds but yields different plaintext
run(0 encrypt --mode ctr --key ${WORK}/key.bin --in ${WORK}/msg.txt --out ${WORK}/c1.bin)

# Wrong key for CBC fails (padding) or yields garbage, but never the original text
execute_process(COMMAND ${TOOL} decrypt --mode cbc --key ${WORK}/key128.hex --in ${WORK}/k4.bin --out ${WORK}/k4.out
                RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
run(1 decrypt --mode cbc --key-hex 00000000000000000000000000000000 --in ${WORK}/k4.bin --out ${WORK}/k4w.out)

# Invalid mode, XTS too short, unknown option / missing value, AAD with a non-AEAD mode
run(1 encrypt --mode des --key ${WORK}/key.bin --in ${WORK}/msg.txt --out ${WORK}/z.bin)
run(1 encrypt --mode xts --key-hex ${XTS_KEY} --text short --out ${WORK}/z.bin)
run(1 encrypt --mode ctr --key ${WORK}/key.bin --aad-text x --in ${WORK}/msg.txt --out ${WORK}/z.bin)
run(1 encrypt --mode ctr --key)
run(1 encrypt --mode ctr --key ${WORK}/key.bin --in ${WORK}/msg.txt --encode utf99)
run(1 encrypt --mode ctr --key ${WORK}/key.bin)

# A malformed KAT file must not be reported as success
file(WRITE ${WORK}/bad_kat.json "[ {\"name\": \"x\", \"mode\": \"gcm\"")
execute_process(COMMAND ${TOOL} --kat ${WORK}/bad_kat.json RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
if(rc EQUAL 0)
  message(FATAL_ERROR "malformed KAT file was accepted")
endif()

message(STATUS "cli_negative: all checks OK")
