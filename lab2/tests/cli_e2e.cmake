# End-to-end CLI regression test (run by ctest):  cmake -DTOOL=<aestool> -DWORK=<dir> -P cli_e2e.cmake
# Covers: keygen, encrypt (auto IV + sidecar), decrypt, raw-binary file output, known ciphertext,
# and fail-closed behaviour (exit codes) for malformed input.
if(NOT TOOL OR NOT WORK)
  message(FATAL_ERROR "usage: cmake -DTOOL=... -DWORK=... -P cli_e2e.cmake")
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

function(run expect_rc)
  execute_process(COMMAND "${TOOL}" ${ARGN} WORKING_DIRECTORY "${WORK}"
                  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT rc EQUAL expect_rc)
    message(FATAL_ERROR "aestool ${ARGN}\n expected exit ${expect_rc}, got ${rc}\n stdout: ${out}\n stderr: ${err}")
  endif()
  set(LAST_OUT "${out}" PARENT_SCOPE)
endfunction()

# 1. plaintext: 3 MB of non-trivial bytes (spans several 1 MiB streaming chunks)
string(REPEAT "0123456789abcdef-Lab2-AES-CTR\n" 100000 plain)
file(WRITE "${WORK}/msg.bin" "${plain}")
file(SIZE "${WORK}/msg.bin" msg_size)

# 2. round trip through files; ciphertext must be RAW (same size, not hex text) + sidecar IV
run(0 keygen --out key.bin --quiet)
run(0 encrypt --key key.bin --in msg.bin --out ct.bin --quiet)
file(SIZE "${WORK}/ct.bin" ct_size)
file(SIZE "${WORK}/ct.bin.iv" iv_size)
if(NOT ct_size EQUAL msg_size)
  message(FATAL_ERROR "ciphertext size ${ct_size} != plaintext size ${msg_size} (file output must be raw binary)")
endif()
if(NOT iv_size EQUAL 16)
  message(FATAL_ERROR "IV sidecar must be 16 bytes, got ${iv_size}")
endif()
run(0 decrypt --key key.bin --in ct.bin --out dec.bin --quiet)
file(SHA256 "${WORK}/msg.bin" h1)
file(SHA256 "${WORK}/dec.bin" h2)
file(SHA256 "${WORK}/ct.bin" h3)
if(NOT h1 STREQUAL h2)
  message(FATAL_ERROR "round trip mismatch")
endif()
if(h1 STREQUAL h3)
  message(FATAL_ERROR "ciphertext equals plaintext")
endif()

# 3. NIST SP 800-38A F.5.1 block 1 through the CLI (screen output, hex)
run(0 encrypt --key-hex 2b7e151628aed2a6abf7158809cf4f3c --iv-hex f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
      --in-encode hex --text 6bc1bee22e409f96e93d7e117393172a --quiet)
if(NOT LAST_OUT MATCHES "ciphertext\\(hex\\)=874d6191b620e3261bef6864990db6ce")
  message(FATAL_ERROR "SP 800-38A F.5.1 block 1 mismatch via CLI: ${LAST_OUT}")
endif()

# 3b. BONUS: AES-192 / AES-256 CTR (SP 800-38A F.5.3 / F.5.5 block 1) and XTS-AES (IEEE 1619 vector 2) via the CLI
run(0 encrypt --key-hex 8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b --iv-hex f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
      --in-encode hex --text 6bc1bee22e409f96e93d7e117393172a --quiet)
if(NOT LAST_OUT MATCHES "ciphertext\\(hex\\)=1abc932417521ca24f2b0459fe7e6e0b")
  message(FATAL_ERROR "AES-192-CTR KAT mismatch via CLI: ${LAST_OUT}")
endif()
run(0 encrypt --key-hex 603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4 --iv-hex f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
      --in-encode hex --text 6bc1bee22e409f96e93d7e117393172a --quiet)
if(NOT LAST_OUT MATCHES "ciphertext\\(hex\\)=601ec313775789a5b7a7f504bbf3d228")
  message(FATAL_ERROR "AES-256-CTR KAT mismatch via CLI: ${LAST_OUT}")
endif()
run(0 encrypt --mode xts
      --key-hex 1111111111111111111111111111111122222222222222222222222222222222
      --iv-hex 33333333330000000000000000000000
      --in-encode hex --text 4444444444444444444444444444444444444444444444444444444444444444 --quiet)
if(NOT LAST_OUT MATCHES "ciphertext\\(hex\\)=c454185e6a16936e39334038acef838bfb186fff7480adc4289382ecd6d394f0")
  message(FATAL_ERROR "XTS-AES-128 KAT mismatch via CLI: ${LAST_OUT}")
endif()
run(3 encrypt --mode xts --key-hex 2b7e151628aed2a6abf7158809cf4f3c --text 0123456789abcdef --quiet)  # XTS needs 32/64-byte key
run(3 encrypt --mode xts --key-hex 1111111111111111111111111111111111111111111111111111111111111111
      --text 0123456789abcdef --quiet)                                                              # K1 == K2 forbidden
run(3 encrypt --mode xts --key-hex 1111111111111111111111111111111122222222222222222222222222222222
      --text short --quiet)                                                                         # data unit < 16 bytes
run(2 encrypt --mode xts --key-hex 1111111111111111111111111111111122222222222222222222222222222222
      --nonce 000000000000000000000000 --text 0123456789abcdef --quiet)                             # --nonce is CTR only
run(3 encrypt --key-hex 00112233445566778899aabbccddeeff0011 --text hi --quiet)                    # 18-byte key

# 4. fail-closed paths (exit codes: 2 usage, 3 invalid input, 4 I/O, 5 counter overflow)
run(3 encrypt --key-hex 00112233 --text hi --quiet)                           # short key
run(3 encrypt --key-hex 2b7e151628aed2a6abf7158809cf4f3c --iv-hex 00 --text hi --quiet)  # short IV
run(3 decrypt --key-hex 2b7e151628aed2a6abf7158809cf4f3c --iv-hex 00000000000000000000000000000000
      --text zz --in-encode hex --quiet)                                      # malformed hex
run(4 encrypt --key key.bin --in msg.bin --out ct.bin --quiet)               # refuses to overwrite
run(4 encrypt --key key.bin --in missing.bin --out x.bin --quiet)            # missing input
run(5 encrypt --key key.bin --in msg.bin --out ovf.bin --force --quiet
      --iv-hex ffffffffffffffffffffffffffffffff)                              # counter overflow
if(EXISTS "${WORK}/ovf.bin" OR EXISTS "${WORK}/ovf.bin.tmp")
  message(FATAL_ERROR "partial output left behind after failure")
endif()
run(2 encrypt --mode gcm --key key.bin --in msg.bin --out y.bin --quiet)     # unsupported mode
run(2 encrypt --key key.bin --text hi --encode raw --quiet)                  # raw to terminal refused
run(2 --bogus)                                                               # unknown option
run(3 --kat "${WORK}/key.bin")                                               # KAT file is not JSON
message(STATUS "CLI end-to-end test passed")
