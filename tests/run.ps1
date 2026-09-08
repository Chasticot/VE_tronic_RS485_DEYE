$ErrorActionPreference = 'Stop'
$compilerRoot = Join-Path $env:LOCALAPPDATA 'Arduino15\packages\esp32\tools\xtensa-esp32-elf-gcc'
$solarCompiler = Get-ChildItem -LiteralPath $compilerRoot -Filter xtensa-esp32-elf-g++.exe -Recurse |
  Select-Object -First 1 -ExpandProperty FullName
if (-not $solarCompiler) { throw 'Installer le coeur Arduino ESP32 avant les tests.' }
& $solarCompiler -std=c++14 -fsyntax-only (Join-Path $PSScriptRoot 'solar_logic_test.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Echec des assertions du regulateur solaire.' }
& $solarCompiler -std=c++14 -fsyntax-only (Join-Path $PSScriptRoot 'wb_protocol_test.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Echec des assertions du protocole WB-01.' }
& $solarCompiler -std=c++14 -fsyntax-only (Join-Path $PSScriptRoot 'deye_modbus_test.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Echec des assertions du protocole Deye Modbus.' }
& $solarCompiler -std=c++14 -fsyntax-only (Join-Path $PSScriptRoot 'lilygo_led_test.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Echec des assertions de la LED LILYGO.' }
& $solarCompiler -std=c++14 -fsyntax-only (Join-Path $PSScriptRoot 'battery_soc_guard_test.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Echec des assertions du verrou SOC batterie.' }
Write-Output 'OK : CRC Modbus, mesures, trames fragmentees, bruit, echo, mauvais esclave, reprise et exceptions.'
Write-Output 'OK : LED LILYGO bleu/vert/violet/rouge, priorites, expiration et debordement millis.'
Write-Output 'OK : verrou SOC Deye, hysteresis, perte de telemetrie et desactivation.'
Write-Output 'OK : reponses WB-01 reelles, tension prise hors charge, demarrage solaire, erreurs serie.'
Write-Output 'OK : seuil, adaptation, plafond, batterie 5 min, reprise, defauts, perte Deye 5 min, reconnexion, debordement millis.'
