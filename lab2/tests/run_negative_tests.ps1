$tool = "D:\MMH_Final_Labs\Final_Lab Tasks\NT219-Crypto-Labs\lab2\release\windows\aestool.exe"
Write-Host "============================================================"
Write-Host "  LAB 2: AUTOMATED NEGATIVE & MALFORMED INPUT TEST SUITE"
Write-Host "============================================================"
$work = "$env:TEMP\lab2_neg_run"
if (Test-Path $work) { Remove-Item -Path $work -Recurse -Force }
New-Item -ItemType Directory -Path $work | Out-Null

$keyValid   = "$work\key16.bin"
$keyBad     = "$work\key10.bin"
$ivValid    = "$work\iv16.bin"
$ivBad      = "$work\iv8.bin"
$msg        = "$work\plain.txt"
$ct         = "$work\ct.bin"
$decWrongKey= "$work\dec_wrongkey.txt"
$decWrongIv = "$work\dec_wrongiv.txt"
$key2       = "$work\key2.bin"
$iv2        = "$work\iv2.bin"

& $tool keygen --out $keyValid --bits 128
& $tool keygen --out $key2 --bits 128
& $tool ivgen  --out $ivValid
& $tool ivgen  --out $iv2

[System.IO.File]::WriteAllBytes($keyBad, [byte[]](1..10))
[System.IO.File]::WriteAllBytes($ivBad,  [byte[]](1..8))
"ATTACK_AT_DAWN_123456789" | Out-File -FilePath $msg -Encoding utf8 -NoNewline

# Encrypt baseline
& $tool encrypt --mode ctr --key $keyValid --iv $ivValid --in $msg --out $ct

Write-Host "`n[TEST 1] Invalid Key Length (10 bytes instead of 16)"
& $tool encrypt --mode ctr --key $keyBad --iv $ivValid --in $msg --out "$work\x.bin"
Write-Host "Exit Code (Expect 2):" $LASTEXITCODE

Write-Host "`n[TEST 2] Invalid IV Length (8 bytes instead of 16)"
& $tool encrypt --mode ctr --key $keyValid --iv $ivBad --in $msg --out "$work\x.bin"
Write-Host "Exit Code (Expect 2):" $LASTEXITCODE

Write-Host "`n[TEST 3] Decrypt with Wrong Key"
& $tool decrypt --mode ctr --key $key2 --iv $ivValid --in $ct --out $decWrongKey
$rec1 = Get-Content $decWrongKey -Raw
Write-Host "Recovered with wrong key: $rec1"
Write-Host "Plaintext mismatch confirmed: $(if ($rec1 -ne "ATTACK_AT_DAWN_123456789") {'YES [CORRUPTED OUTPUT - PASS]'} else {'FAIL'})"

Write-Host "`n[TEST 4] Decrypt with Wrong IV"
& $tool decrypt --mode ctr --key $keyValid --iv $iv2 --in $ct --out $decWrongIv
$rec2 = Get-Content $decWrongIv -Raw
Write-Host "Recovered with wrong IV: $rec2"
Write-Host "Plaintext mismatch confirmed: $(if ($rec2 -ne "ATTACK_AT_DAWN_123456789") {'YES [CORRUPTED OUTPUT - PASS]'} else {'FAIL'})"

Write-Host "`n[TEST 5] Bit-Flipping Malleability Attack (CTR Tampering Demonstration)"
# Tamper with byte 0 of ciphertext: XOR with 0x20 to flip case 'A' (0x41) -> 'a' (0x61)
$ctBytes = [System.IO.File]::ReadAllBytes($ct)
$ctBytes[0] = $ctBytes[0] -bxor 0x20
$tamperedCt = "$work\ct_tampered.bin"
[System.IO.File]::WriteAllBytes($tamperedCt, $ctBytes)

$tamperedOut = "$work\dec_tampered.txt"
& $tool decrypt --mode ctr --key $keyValid --iv $ivValid --in $tamperedCt --out $tamperedOut
$recTampered = Get-Content $tamperedOut -Raw
Write-Host "Original: ATTACK_AT_DAWN_123456789"
Write-Host "Tampered: $recTampered"
if ($recTampered -eq "aTTACK_AT_DAWN_123456789") {
    Write-Host "Malleability result: Exactly 1st bit-flipped character modified without error! [DEMONSTRATED]"
    Write-Host "Security conclusion: CTR mode DOES NOT provide integrity/authenticity (Requires MAC/AEAD)."
}

Write-Host "`n[TEST 6] Non-existent input file (I/O error handling)"
& $tool encrypt --mode ctr --key $keyValid --iv $ivValid --in "$work\no_such_file.bin" --out "$work\x.bin"
Write-Host "Exit Code (Expect 3):" $LASTEXITCODE

Write-Host "`n=== ALL NEGATIVE AND INTEGRITY CHECKS COMPLETED ==="
