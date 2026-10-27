$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$binary = Join-Path ([System.IO.Path]::GetTempPath()) ('tmf8829-' + [guid]::NewGuid().ToString('N') + '.exe')
Push-Location $root
try {
  foreach ($platform in @('generic', 'esp32', 'esp32-old', 'esp32-legacy')) {
    $flags = @('-std=c++11', '-Wall', '-Wextra', '-Werror', '-I', 'tests/host', '-I', 'src')
    if ($platform -like 'esp32*') { $flags += '-DARDUINO_ARCH_ESP32' }
    if ($platform -eq 'esp32-old') { $flags += '-DTMF_TEST_OLD_CORE' }
    if ($platform -eq 'esp32-legacy') { $flags += '-DTMF_TEST_LEGACY_CORE' }
    Write-Output "Host UART tests: $platform"
    & g++ @flags src/DFRobot_TMF8829.cpp tests/host/test.cpp -o $binary
    if ($LASTEXITCODE -ne 0) { throw 'Host test compilation failed' }
    & $binary
    if ($LASTEXITCODE -ne 0) { throw 'Host UART tests failed' }
  }
} finally {
  Pop-Location
  if (Test-Path -LiteralPath $binary) { Remove-Item -LiteralPath $binary }
}
