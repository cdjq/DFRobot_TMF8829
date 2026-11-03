# DFRobot_TMF8829

- [中文版](./README_CN.md)

Arduino driver for the TMF8829 point cloud sensor module. The host uses hardware UART request/response to acquire XYZ point clouds, select measurement presets and peaks, choose point/line/frame output, and explicitly save configuration.

## Product Link ([https://www.dfrobot.com.cn](https://www.dfrobot.com.cn))

```
SKU: SEN0773
```

## Table of Contents

* [Summary](#summary)
* [Installation](#installation)
* [Methods](#methods)
* [UART Data Frame Format](#uart-data-frame-format)
* [Compatibility](#compatibility)
* [History](#history)
* [Credits](#credits)

## Summary

Use hardware UART (8N1) to request XYZ point-cloud frames. The library constructor defaults to **921600**, while the firmware starts at **921600** when no valid configuration has been saved. All six examples explicitly use `eBaudRate921600`; if the module has another saved rate, change the example constructor to match its current rate.

ESP32 examples use `Serial1`, with host RX on **GPIO25** and TX on **GPIO26**. Connect module TX to GPIO25, module RX to GPIO26, VCC to **3.3V**, and GND to GND. `Serial` prints the results.

Call `begin()`, select the measurement preset, peak count and point/line/frame output as needed, then call `update()` and read the validated cache using `getAllPointData()` or `getPointData()`. Enums and structs are defined outside the class: use `eMode48X32`, `eBaudRate921600` and `sPoint_t` without a `DFRobot_TMF8829::` prefix. Point storage is managed automatically across mode changes.

The `getAllPointData` example prints preparation time, request-to-first-byte time, receive/parse time, total `update()` time, and timing-report output time in milliseconds. `getTiming()` returns the latest `sTiming_t` measurements in microseconds, including failed requests. First-byte time is measured when the host reads the byte. Report output time includes `Serial.flush()` but excludes the final timing-summary lines and the point table.

Choose any example independently for its purpose; no run order is required:

* [getAllPointData](./examples/getAllPointData/getAllPointData.ino): read and print a complete point-cloud frame.
* [getPointData](./examples/getPointData/getPointData.ino): select a pixel using the macros at the top and read all its peaks.
* [getLineData](./examples/getLineData/getLineData.ino): select a row using the macro at the top.
* [getConfig](./examples/getConfig/getConfig.ino): query live settings and demonstrate preset side effects.
* [saveConfig](./examples/saveConfig/saveConfig.ino): save configuration to NVS once in setup.
* [setBaudRate](./examples/setBaudRate/setBaudRate.ino): switch from 921600 to 115200 and verify both ends.

## Installation

* Download into Arduino libraries/DFRobot_TMF8829, reopen the IDE and select an example.
* Once published to Library Manager, search for DFRobot_TMF8829 there; availability is not yet confirmed.

## Methods

### Public API

Complete public API, copied from the header:


```C++
  /**
   * @fn DFRobot_TMF8829
   * @brief Store the injected serial port and initial UART settings.
   * @param serial Borrowed hardware UART; null makes begin fail.
   * @param baudRate Current module rate: any eBaudRate_t value listed above.
   * @param rxPin ESP32 host RX pin; -1 uses the board default.
   * @param txPin ESP32 host TX pin; -1 uses the board default.
   * @n Construction performs no communication and does not own the UART.
   */
  DFRobot_TMF8829(HardwareSerial *serial, eBaudRate_t baudRate = eBaudRate921600, int8_t rxPin = -1, int8_t txPin = -1);

  /**
   * @fn ~DFRobot_TMF8829
   * @brief Release internally allocated point storage; the UART remains borrowed.
   */
  ~DFRobot_TMF8829();
  DFRobot_TMF8829(const DFRobot_TMF8829 &) = delete;
  DFRobot_TMF8829 &operator=(const DFRobot_TMF8829 &) = delete;

  /**
   * @fn setFrameBuffer
   * @brief Optionally replace automatic storage with caller-owned storage while idle.
   * @param buffer Non-null array kept alive throughout use.
   * @param capacity Number of point slots, not pixels or bytes; must be nonzero.
   * @n Clears frame validity and releases previous internal storage; never frees caller storage.
   * @return true if storage was bound.
   */
  bool setFrameBuffer(sPoint_t *buffer, size_t capacity);

  /**
   * @fn begin
   * @brief Open the UART, force REPORT=0, check AT and query actual configuration.
   * @n Allocates TMF8829_MAX_POINTS slots once unless caller storage was supplied.
   * @n Returns false on allocation failure. In-flight binary frames are parsed and discarded.
   * @n Does not change resolution, spatial selection or NVS.
   * @return true only after confirming request/response mode and all config fields.
   */
  bool begin(void);

  /**
   * @fn getConfig
   * @brief Query and validate all actual configuration fields.
   * @param config Output updated only on complete success.
   * @n Field order is arbitrary; unknown fields are ignored. Failed queries invalidate the config cache.
   * @return true on complete fields and final OK.
   */
  bool getConfig(sConfig_t &config);

  /**
   * @fn setSensorMode
   * @brief Set the resolution/measurement preset and re-query actual state.
   * @param mode eMode8X8, eMode16X16, eMode32X32, eMode48X32: normal presets.
   * @n eModeH8X8, eModeH16X16, eModeH32X32, eModeH48X32: H presets; eModeL8X8: L preset.
   * @n A different preset resets Peak to 1 and selection to FRAME; the same preset does not.
   * @n Clears frame validity. Changes are not automatically saved.
   * @return true if the command and configuration verification succeed.
   */
  bool setSensorMode(eMeasurementMode_t mode);

  /**
   * @fn setPeakCount
   * @brief Select the number of slots per pixel without clamping.
   * @param peakCount 1..4 for 8/16 presets, 1..3 for 32 presets, 1..2 for 48 presets.
   * @n Validates against actual preset before sending. Clears frame validity; does not save.
   * @return true if the command and verification succeed.
   */
  bool setPeakCount(uint8_t peakCount);

  /**
   * @fn setMeasureMode
   * @brief Select one original-image pixel for POINT output.
   * @param x Zero-based original column, less than preset width.
   * @param y Zero-based original row, less than preset height.
   * @n Output is 1x1 with all peaks. Cache coordinates are (0,0); XYZ is not translated.
   * @n Clears frame validity; does not save.
   * @return true on command and verification success.
   */
  bool setMeasureMode(uint8_t x, uint8_t y);

  /**
   * @fn setMeasureMode
   * @brief Select one original-image row for LINE output.
   * @param y Zero-based original row, less than preset height.
   * @n Output is preset width x 1 with all peaks. Cache y is 0; XYZ is not translated.
   * @n Clears frame validity; does not save.
   * @return true on command and verification success.
   */
  bool setMeasureMode(uint8_t y);

  /**
   * @fn setMeasureMode
   * @brief Restore full FRAME output at the current resolution.
   * @n Clears frame validity; does not change preset/peak count or save configuration.
   * @return true on command and verification success.
   */
  bool setMeasureMode(void);

  /**
   * @fn getBaudRate
   * @brief Send BAUD? and validate the returned supported rate.
   * @param baudRate Updated only after a complete query and final OK.
   * @n Supports exactly the eight eBaudRate_t values listed above; also updates cached baud.
   * @return true on complete success.
   */
  bool getBaudRate(eBaudRate_t &baudRate);

  /**
   * @fn setBaudRate
   * @brief Change module and host UART rates while no frame request is pending.
   * @param baudRate Any of the eight eBaudRate_t values listed above.
   * @n Waits for complete OK at the old rate, switches the host, then verifies AT and BAUD?.
   * @n Takes effect immediately without restart; does not save. Failure may leave rates unsynchronized.
   * @n Recover with a new object at the known device rate and begin; no automatic baud scanning.
   * @return true only when both sides are verified at the requested rate.
   */
  bool setBaudRate(eBaudRate_t baudRate);

  /**
   * @fn saveConfig
   * @brief Explicitly save all current module settings to NVS.
   * @n No restart required. Call once in setup when intended, never repeatedly in loop.
   * @n On the next boot the host must use the saved UART rate.
   * @return true on final OK.
   */
  bool saveConfig(void);

  /**
   * @fn update
   * @brief Send one GETFRAME and wait for one complete valid response.
   * @param timeoutMs Response deadline in milliseconds, default 5000.
   * @n Requires begin and REPORT=0. Recovery of a prior request can take an additional 5000 ms.
   * @n Clears valid/isNew on entry. Failure sets pointCount=0; previous point contents are not preserved.
   * @n Wire length, geometry, peak count, capacity, CRC and tail must all pass before publication.
   * @return true on a complete response, including a repeated frame ID.
   */
  bool update(uint32_t timeoutMs = 5000);

  /**
   * @fn getAllPointData
   * @brief Return the cache populated by the most recent successful update.
   * @n Call update first and check valid. This method performs no communication.
   * @n Row-major slots: (y * width + x) * peakCount + peak; zero-confidence slots remain present.
   * @n Reference and pointer are usable only until update, configuration/storage change or destruction.
   * @return Metadata and borrowed points; valid is a protocol result, not per-point quality.
   */
  const sFrame_t &getAllPointData(void) const;

  /**
   * @fn getPointData
   * @brief Copy one point from the validated update cache, without communication.
   * @param x Output-local column, starting at zero.
   * @param y Output-local row; POINT and LINE require zero.
   * @param peak Zero-based peak index, less than cached peakCount.
   * @param point Output unchanged on failure.
   * @n Call update first. POINT requires (0,0); indices do not represent the selected original pixel.
   * @return true when the cache is valid and all indices are in range.
   */
  bool getPointData(uint8_t x, uint8_t y, uint8_t peak, sPoint_t &point) const;

  /**
   * @fn isDataNew
   * @brief Read the update cache's new-frame flag without clearing it.
   * @n First successful frame is new; later IDs are compared for inequality, allowing wraparound.
   * @return Cached isNew, false after a failed update or invalidation.
   */
  bool isDataNew(void) const;

  /**
   * @fn isDataValid
   * @brief Read the update cache's complete-frame validation result.
   * @n Call update first. Individual confidence/signal values do not determine protocol validity.
   * @return true only for the currently published validated frame.
   */
  bool isDataValid(void) const;
```

### Power-on defaults

When Flash contains no valid saved configuration, the module starts with the following defaults. Otherwise, it restores the saved configuration.

| Setting | Default |
| --- | --- |
| Measurement mode | `48X32`, normal mode |
| Measurement period | `66 ms`, approximately 15 frames/s |
| Peak count | `1` |
| Output selection | `FRAME`, full frame |
| Automatic reporting | Disabled (`REPORT=0`) |
| Signal strength data | Enabled |
| Noise data | Disabled |
| Crosstalk data | Disabled |
| UART baud rate | `921600` |
| UART format | 8 data bits, no parity, 1 stop bit (8N1) |

The library constructor defaults to `921600`, matching the module power-on defaults above. Pass another rate explicitly if the module has a different saved configuration.

### Command format and functions

The module accepts case-sensitive ASCII commands over UART (8N1, no hardware flow control). Do not insert spaces in commands or parameters. Terminate each command with `\r\n` (CRLF); the library appends this automatically. A command must be shorter than 128 bytes. Send commands one at a time and wait for each complete response.

Successful text commands return `\r\nOK\r\n`; unknown commands, invalid parameters or execution failures return `\r\nERROR\r\n`. Queries return field lines followed by `OK`. A successful `AT+GETFRAME` returns a binary frame without a trailing `OK`.

| Command | Parameters and behavior | Library API |
| --- | --- | --- |
| `AT` | Check communication; returns `OK`. | Used by `begin()` |
| `AT+CONFIG?` | Query MODE, PEAK, REPORT, SELECT, X, Y and BAUD. | `getConfig()` |
| `AT+MODE=<mode>` | Select a measurement preset from the table below. | `setSensorMode()` |
| `AT+PEAK=<count>` | Peaks per pixel, starting at 1; maximum depends on the preset. | `setPeakCount()` |
| `AT+REPORT=<mode>` | `0`: request/response; `1`: firmware automatic reporting. Measurement continues in either mode. | `begin()` forces `0`; the library only supports request/response |
| `AT+GETFRAME` | Request the next complete XYZ frame in request/response mode. Returns `ERROR` if busy or automatic reporting is enabled. | `update()` |
| `AT+POINT=<x>,<y>` | Select a zero-based column and row in the original image; output is 1×1 with all peaks. Out-of-range indices return `ERROR`. | `setMeasureMode(x, y)` |
| `AT+LINE=<y>` | Select a zero-based original row; output is preset width × 1 with all peaks. Out-of-range indices return `ERROR`. | `setMeasureMode(y)` |
| `AT+FRAME` | Restore full-image output at the current preset, retaining the peak count. | `setMeasureMode()` |
| `AT+BAUD?` | Return `+BAUD:<rate>`, followed by `OK`. | `getBaudRate()` |
| `AT+BAUD=<rate>` | Set the UART rate using one of the integer values below. | `setBaudRate()` |
| `AT+SAVE` | Save MODE, PEAK, REPORT, SELECT, X, Y and BAUD to NVS for restoration at power-on. | `saveConfig()` |

### Measurement presets and peak limits

| `<mode>` | Width × height | Maximum `<count>` |
| --- | --- | --- |
| `8X8` | 8×8 | 4 |
| `16X16` | 16×16 | 4 |
| `32X32` | 32×32 | 3 |
| `48X32` | 48×32 | 2 |
| `H_8X8` | 8×8 | 4 |
| `H_16X16` | 16×16 | 4 |
| `H_32X32` | 32×32 | 3 |
| `H_48X32` | 48×32 | 2 |
| `L_8X8` | 8×8 | 4 |

Switching successfully to a different preset resets PEAK to 1 and selection to FRAME. Selecting the current preset does not perform these resets. Configure the preset before setting the peak count and point/line selection.

### Configuration query example

Send `AT+CONFIG?\r\n`. An example response is shown below; each line ends in CRLF and field order may vary:

```text
+CONFIG:MODE=48X32
+CONFIG:PEAK=1
+CONFIG:REPORT=0
+CONFIG:SELECT=FRAME
+CONFIG:X=-1
+CONFIG:Y=-1
+CONFIG:BAUD=921600
OK
```

SELECT is `FRAME`, `LINE` or `POINT`. FRAME reports X=Y=-1; LINE reports X=-1 and Y as the selected row; POINT reports the selected original column and row. POINT/LINE do not translate XYZ coordinates, and their binary frames do not carry the original selection indices.

### Requesting data, changing baud rate and saving

For example, send the following commands in order, appending CRLF and waiting for each response:

```text
AT
AT+REPORT=0
AT+MODE=48X32
AT+PEAK=2
AT+FRAME
AT+GETFRAME
```

The first five commands return `OK`; GETFRAME returns a binary XYZ frame beginning with `AA 55` and ending with `55 AA`. Each point contains little-endian int16 x/y/z (mm), uint16 signal and uint8 confidence (9 bytes total). Use `update()` to receive and validate a frame, then read the cached points with the getters. Wait until the current frame transaction finishes before sending another command; do not read the same UART manually while using the library.

Allowed baud rates are `115200`, `460800`, `921600`, `1000000`, `2000000`, `3000000`, `4000000` and `5000000`; abbreviations such as `1M` are invalid. The library constructor defaults to **921600**, while the referenced firmware defaults to **921600** without a saved configuration. Pass the module's actual current rate to the constructor; the library does not scan rates.

To change baud rate manually, finish any pending frame, send `AT+REPORT=0` and wait for `OK`, then send e.g. `AT+BAUD=921600` at the old rate. Wait for the complete old-rate `OK`, switch the host UART to 921600, and verify with `AT` and `AT+BAUD?`. `setBaudRate()` performs this rate switch and verification for library users.

Settings take effect without an automatic NVS write. Send `AT+SAVE` only when persistence is required; no restart is needed to apply the current settings. After saving a new baud rate, use that rate at the next power-on. Avoid repeatedly saving in `loop()`.

### Data and Memory

sPoint_t contains signed int16_t x/y/z in mm, uint16_t signal and uint8_t confidence. sFrame_t contains width, height, peakCount, frameId, pointCount, points, isNew and valid. sConfig_t contains mode, peakCount, reportMode, outputMode, x, y and baudRate. See the [header](./src/DFRobot_TMF8829.h) for all enums and fields.


Slot order is `(y * width + x) * peakCount + peak`. POINT uses local (0,0); LINE uses local y=0. Protocol validity does not guarantee per-point quality. Repeated IDs succeed with isNew=false; first success and changed/wrapped IDs are new. Failures leave valid/isNew=false and pointCount=0.


| Output | Slots | RAM if sizeof(sPoint_t)=10 |
| --- | ---: | ---: |
| 8x8, Peak=1 | 64 | 640 B |
| 48x32, Peak=2 / 32x32, Peak=3 | 3072 | 30720 B |
| POINT, Peak=4 | 4 | 40 B |
| LINE 8x1, Peak=4 | 32 | 320 B |


By default, begin() allocates TMF8829_MAX_POINTS (3072) slots once and reuses them across all sensor modes and peak counts. Users do not declare a points array or call setFrameBuffer(). With sizeof(sPoint_t)=10 this uses 30,720 bytes (30 KiB), plus sizeof(DFRobot_TMF8829) and allocator overhead. begin() returns false if allocation fails; destruction frees internal storage. update() does not allocate memory. Wire bytes are staged in this array and expanded backwards without a second frame buffer.

The table above shows minimum capacities for optional caller-owned storage supplied through setFrameBuffer(). This advanced option releases previous internal storage and never frees caller memory. An undersized caller buffer causes oversized frames to be consumed and rejected. References to point data expire on update, configuration/storage changes or destruction; a failed operation may already have overwritten the data.

AT timeout is 1000 ms; mode/SAVE timeout is 5000 ms; update defaults to 5000 ms. Recovery of a preceding transaction can add up to 5000 ms. A timed-out GETFRAME must finish with a complete late frame or ERROR before another request is sent. If recovery fails, check the connection and initialize again at the known rate. There is no automatic baud scanning.

setBaudRate waits for the complete old-rate OK, switches the host and verifies AT/BAUD?. Failure can leave rates unsynchronized. SAVE is explicit; never repeatedly write NVS in loop.

## UART Data Frame Format

A successful `AT+GETFRAME\r\n` returns a binary XYZ point-cloud frame over **8N1** UART. Offsets below are zero-based from the first header byte. All multibyte integers are **little-endian (least significant byte first)**. Binary frames do not end with `\r\n`; ordinary AT text replies are still parsed as lines.

Let `W` be the output width, `H` the output height, `P` the peaks per pixel, and `D = W × H × P × 9` the point-data size in bytes.

| Byte offset | Field | Bytes | Type / content |
|---|---|---:|---|
| 0 | Header | 2 | Fixed `AA 55` |
| 2 | Length L | 2 | `uint16_t`; `L = D + 9`, counting from this length field through the last point-data byte |
| 4 | Output width W | 1 | `uint8_t` |
| 5 | Output height H | 1 | `uint8_t` |
| 6 | Frame ID | 4 | `uint32_t`, corresponding to `sFrame_t::frameId` |
| 10 | Peaks per pixel P | 1 | `uint8_t` |
| 11 | Point data | D | 9 bytes per peak slot; see below |
| 11 + D | CRC16 | 2 | Little-endian CRC-16/MODBUS |
| 13 + D | Tail | 2 | Fixed `55 AA` |

**Each peak slot (9 bytes)**:

| Offset within slot | Field | Bytes | Type / unit |
|---|---|---:|---|
| 0 | X | 2 | `int16_t`, mm |
| 2 | Y | 2 | `int16_t`, mm |
| 4 | Z | 2 | `int16_t`, mm |
| 6 | Signal | 2 | `uint16_t`, signal strength |
| 8 | Confidence | 1 | `uint8_t`, raw confidence |

Pixels use row-major order, with P consecutive peak slots for each pixel. The point-array index is `((y × W) + x) × P + peak`; coordinates and peak indices are zero-based. For line/point output, array coordinates are local to the selected output region.

- Full frame: W and H are the current preset resolution.
- Single line: W is the current preset width and H is 1.
- Single point: W and H are both 1; all P configured peak slots are included.

**CRC rules**: initial value `0xFFFF`, reflected polynomial `0xA001`, byte-wise XOR followed by right shifts, and no final XOR. Coverage is offsets 2 through `10 + D` inclusive: length, dimensions, frame ID, peak count, and all point data. The header, CRC itself, and tail are excluded. Send the CRC low byte first. The standard check string `123456789` produces `0x4B37`.

```text
Total frame bytes = D + 15 = L + 6
48×32, Peak=1: D = 13824, L = 13833, total = 13839 bytes
Theoretical UART time (ms) = total frame bytes × 10 × 1000 / baud rate
```

Example: single-point output, Peak=1, frame ID=1, X=-7 mm, Y=10 mm, Z=31 mm, Signal=36102, Confidence=183:

```text
AA 55 12 00 01 01 01 00 00 00 01 F9 FF 0A 00 1F 00 06 8D B7 5E B8 55 AA
```

The length field is `12 00` (18), and the complete frame is 24 bytes. Each wire-format peak slot is exactly 9 bytes; do not calculate wire length using `sizeof(sPoint_t)`, which may include alignment padding.

## Compatibility

| Board | Work Well | Work Wrong | Untested | Remarks |
| ------------ | :--: | :----: | :----: | ---- |
| Mega2560 | | | √ | |
| Leonardo | | | √ | |
| ESP32 | | | √ | |
| ESP8266 | | | √ | |

## History

- 2026/09/03 - V1.0.0 version

## Credits

Written by PLELES(li.jia@dfrobot.com), 2026. (Welcome to our [website](https://www.dfrobot.com/))
