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
Write-Output 'OK : CRC Modbus, mesures, trames fragmentees, bruit, echo, mauvais esclave, reprise et exceptions.'
Write-Output 'OK : reponses WB-01 reelles, tension prise hors charge, demarrage solaire, erreurs serie.'
Write-Output 'OK : seuil, adaptation, plafond, batterie 5 min, reprise, defauts, perte Deye 5 min, reconnexion, debordement millis.'
