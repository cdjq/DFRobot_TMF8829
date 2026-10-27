# Software verification

Public capability mapping:

| Capability | Public API |
| --- | --- |
| UART setup and automatic/optional caller-owned storage | Constructor, setFrameBuffer, begin, destructor |
| Actual configuration | getConfig |
| Resolution and Peak selection | setSensorMode, setPeakCount |
| Pixel, row and full image selection | Three setMeasureMode overloads |
| UART rate and explicit persistence | getBaudRate, setBaudRate, saveConfig |
| Request one frame | update |
| Read validated storage and status | getAllPointData, getPointData, isDataNew, isDataValid |

API Doxygen documents preset resets, original versus local coordinates, buffer lifetime, explicit NVS writes, baud recovery and the distinction between valid frames and point quality. Transport details stay private.

Run from the library root with PowerShell and a C++11-capable `g++` on PATH:

```powershell
powershell -ExecutionPolicy Bypass -File tests/host/run.ps1
```

The runner builds and executes the real library against a simulated hardware UART, for both the generic and ESP32 initialization branches. It writes a temporary executable outside the repository and removes it afterward. Warnings are errors.

| Requirement | Coverage |
| --- | --- |
| Initialization and configuration | REPORT=0, startup binary traffic, shuffled CONFIG fields, query validation and preset changes |
| Presets and output | All nine presets, Peak limits, POINT/LINE/FRAME, local coordinates and row-major Peak order |
| Point decoding | Signed little-endian XYZ, zero signal/confidence, invalid indices and frame ID repetition/wraparound |
| Stream parser | CRC reference vector, malformed CRC/length/geometry/tail, fragmented frames, embedded text/frame markers, nested recovery and unsolicited frames |
| Transaction recovery | Timeout with partial response, drain late response before sending another GETFRAME, timer wraparound |
| Point storage | Automatic storage across all presets and maximum peaks, repeated begin reuse, ownership replacement/destruction; caller storage canaries and undersized buffer rejection |
| Persistence and baud | Explicit SAVE, verified UART switch, failed baud verification invalidates initialization |

For ESP32 Arduino compilation, use the Arduino CLI with the ESP32 core installed:

```powershell
$cli = 'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'
foreach ($example in @('getAllPointData', 'getPointData', 'getLineData', 'getConfig', 'saveConfig', 'setBaudRate')) {
  & $cli compile --fqbn esp32:esp32:esp32 --library . "examples/$example"
  if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $example" }
}
```

Validation date: 2026-10-23. Both host test variants passed, and all six examples compiled successfully with ESP32 Arduino core 3.3.3. These checks exercise software and compilation only. Sensor wiring, sustained reception at each baud rate, actual mode transitions, power-cycle persistence and physical measurements require hardware testing; they have not been verified here.
