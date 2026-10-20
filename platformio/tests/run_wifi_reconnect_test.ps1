param([string]$Zig = 'zig')
$ErrorActionPreference = 'Stop'
$testBuildDir = Join-Path $PSScriptRoot '..\.pio\wifi-reconnect-tests'
New-Item -ItemType Directory -Force -Path $testBuildDir | Out-Null
$source = [IO.File]::ReadAllText((Join-Path $PSScriptRoot '..\include\wifi_reconnect.h'))
$source = [regex]::Replace($source, '(?m)^#include <(?:WiFi.h|AutoConnectCredential.h|esp_wifi.h)>\r?\n', '')
[IO.File]::WriteAllText((Join-Path $testBuildDir 'wifi_reconnect_under_test.h'), $source)
$previousGlobalCache = $env:ZIG_GLOBAL_CACHE_DIR
$previousLocalCache = $env:ZIG_LOCAL_CACHE_DIR
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $testBuildDir 'zig-global-cache'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $testBuildDir 'zig-local-cache'
try {
  $testExecutable = Join-Path $testBuildDir 'wifi_reconnect_test.exe'
  & $Zig c++ -std=c++14 -O1 -UNDEBUG -I $testBuildDir (Join-Path $PSScriptRoot 'wifi_reconnect_test.cpp') -o $testExecutable
  if ($LASTEXITCODE -ne 0) { throw 'Wi-Fi recovery test compilation failed.' }
  & $testExecutable
  if ($LASTEXITCODE -ne 0) { throw 'Wi-Fi recovery test failed.' }
} finally {
  $env:ZIG_GLOBAL_CACHE_DIR = $previousGlobalCache
  $env:ZIG_LOCAL_CACHE_DIR = $previousLocalCache
}
