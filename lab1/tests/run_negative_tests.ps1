Write-Host "Running Automated Negative Tests on Windows (MSVC aestool)..."
$work = "$env:TEMP\lab1_neg_run"
if (Test-Path $work) { Remove-Item -Path $work -Recurse -Force }
New-Item -ItemType Directory -Path $work | Out-Null
$key = "$work\key.bin"
$msg = "$work\msg.txt"
$ct  = "$work\ct.bin"
$out = "$work\out.txt"
"Automated Negative Test Data" | Out-File -FilePath $msg -Encoding utf8 -NoNewline

Write-Host "[TEST 1] Keygen 256 bits"
& "D:\MMH_Final_Labs\Final_Lab Tasks\NT219-Crypto-Labs\lab1\release\windows\aestool_msvc.exe" keygen --bits 256 --out $key
Write-Host "Exit Code:" $LASTEXITCODE

Write-Host "[TEST 2] Encrypt GCM + Decrypt Roundtrip"
& "D:\MMH_Final_Labs\Final_Lab Tasks\NT219-Crypto-Labs\lab1\release\windows\aestool_msvc.exe" encrypt --mode gcm --aead --key $key --in $msg --out $ct --meta "$ct.json"
& "D:\MMH_Final_Labs\Final_Lab Tasks\NT219-Crypto-Labs\lab1\release\windows\aestool_msvc.exe" decrypt --mode gcm --aead --key $key --in $ct --meta "$ct.json" --out $out
$recovered = Get-Content $out -Raw
if ($recovered -eq "Automated Negative Test Data") { Write-Host "Result: PASS (recovered plaintext exactly)" } else { Write-Host "Result: FAIL" }

Write-Host "[TEST 3] AEAD mode without --aead flag"
& "D:\MMH_Final_Labs\Final_Lab Tasks\NT219-Crypto-Labs\lab1\release\windows\aestool_msvc.exe" encrypt --mode gcm --key $key --in $msg --out "$work\fail.bin"
Write-Host "Exit Code (Must be non-zero):" $LASTEXITCODE

Write-Host "[TEST 4] Malformed IV length"
& "D:\MMH_Final_Labs\Final_Lab Tasks\NT219-Crypto-Labs\lab1\release\windows\aestool_msvc.exe" encrypt --mode cbc --key $key --iv 0011 --in $msg --out "$work\fail.bin"
Write-Host "Exit Code (Must be non-zero):" $LASTEXITCODE

Write-Host "[TEST 5] Malformed Key length"
& "D:\MMH_Final_Labs\Final_Lab Tasks\NT219-Crypto-Labs\lab1\release\windows\aestool_msvc.exe" encrypt --mode ctr --key-hex 0011223344 --in $msg --out "$work\fail.bin"
Write-Host "Exit Code (Must be non-zero):" $LASTEXITCODE

Write-Host "[TEST 6] Nonce Reuse Detection"
$db = "$work\nonces.db"
& "D:\MMH_Final_Labs\Final_Lab Tasks\NT219-Crypto-Labs\lab1\release\windows\aestool_msvc.exe" encrypt --mode gcm --aead --key $key --iv 000000000000000000000001 --in $msg --out "$work\n1.bin" --nonce-db $db
Write-Host "First usage exit code:" $LASTEXITCODE
& "D:\MMH_Final_Labs\Final_Lab Tasks\NT219-Crypto-Labs\lab1\release\windows\aestool_msvc.exe" encrypt --mode gcm --aead --key $key --iv 000000000000000000000001 --in $msg --out "$work\n2.bin" --nonce-db $db
Write-Host "Second usage exit code (Must be non-zero):" $LASTEXITCODE

Write-Host "[TEST 7] ECB file > 16KiB blocked without flag"
$big = "$work\big.txt"
("A" * 20000) | Out-File -FilePath $big -Encoding utf8 -NoNewline
& "D:\MMH_Final_Labs\Final_Lab Tasks\NT219-Crypto-Labs\lab1\release\windows\aestool_msvc.exe" encrypt --mode ecb --key $key --in $big --out "$work\e.bin"
Write-Host "Without flag exit code (Must be non-zero):" $LASTEXITCODE
& "D:\MMH_Final_Labs\Final_Lab Tasks\NT219-Crypto-Labs\lab1\release\windows\aestool_msvc.exe" encrypt --mode ecb --allow-ecb --key $key --in $big --out "$work\e.bin"
Write-Host "With --allow-ecb exit code (Must be 0):" $LASTEXITCODE
