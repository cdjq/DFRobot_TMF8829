# DFRobot_TMF8829

- [中文版](./README_CN.md)

Arduino driver for the TMF8829 module firmware specified in task.md. The host uses hardware UART request/response to acquire XYZ point clouds, select measurement presets and peaks, choose point/line/frame output, and explicitly save configuration.

## Product Link

SKU, product URL, supply/UART voltage, images, final maintainer and repository URL await confirmation. The existing MIT license is preserved.

## Table of Contents

* [Summary](#summary)

* [Installation](#installation)

* [Wiring](#wiring)

* [Data and Memory](#data-and-memory)

* [Examples](#examples)

* [Methods](#methods)

* [Compatibility](#compatibility)

* [History](#history)

* [Credits](#credits)


## Summary

* Hardware UART, 8N1, library default 115200; REPORT=0 and GETFRAME request/response only.
* Nine presets: setSensorMode selects resolution; three setMeasureMode overloads select spatial output.
* Length, geometry, CRC16/Modbus and trailer validation; zero signal/confidence slots are retained.
* Automatic point storage, no heap allocation in update, and cache-only data getters.


## Installation

* Download into Arduino libraries/DFRobot_TMF8829, reopen the IDE and select an example.
* Once published to Library Manager, search for DFRobot_TMF8829 there; availability is not yet confirmed.


## Wiring

| Module | ESP32 |
| --- | --- |
| TX | GPIO16 (Serial1 RX) |
| RX | GPIO17 (Serial1 TX) |
| GND | GND |


Confirm module supply and UART levels before connecting. Examples use Serial for the debug console and Serial1 for the sensor; change host pins in the constructor as needed.


## Data and Memory

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


## Examples

* [getAllPointData](./examples/getAllPointData/getAllPointData.ino)

* [getPointData](./examples/getPointData/getPointData.ino)

* [getLineData](./examples/getLineData/getLineData.ino)

* [getConfig](./examples/getConfig/getConfig.ino)

* [saveConfig](./examples/saveConfig/saveConfig.ino)

* [setBaudRate](./examples/setBaudRate/setBaudRate.ino)


## Methods

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
  DFRobot_TMF8829(HardwareSerial *serial, eBaudRate_t baudRate = eBaudRate115200, int8_t rxPin = -1, int8_t txPin = -1);

  /** @brief Release internally allocated point storage; the UART remains borrowed. */
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
   * @fn setMeasureMode(uint8_t, uint8_t)
   * @brief Select one original-image pixel for POINT output.
   * @param x Zero-based original column, less than preset width.
   * @param y Zero-based original row, less than preset height.
   * @n Output is 1x1 with all peaks. Cache coordinates are (0,0); XYZ is not translated.
   * @n Clears frame validity; does not save.
   * @return true on command and verification success.
   */
  bool setMeasureMode(uint8_t x, uint8_t y);

  /**
   * @fn setMeasureMode(uint8_t)
   * @brief Select one original-image row for LINE output.
   * @param y Zero-based original row, less than preset height.
   * @n Output is preset width x 1 with all peaks. Cache y is 0; XYZ is not translated.
   * @n Clears frame validity; does not save.
   * @return true on command and verification success.
   */
  bool setMeasureMode(uint8_t y);

  /**
   * @fn setMeasureMode(void)
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


## Compatibility

| Board | Work Well | Work Wrong | Untested | Remarks |
| --- | --- | --- | --- | --- |
| ESP32 | | | ? | Hardware untested; see validation notes below. |


See tests/host/README.md for software validation. Compilation and simulated protocol tests do not establish hardware compatibility. Other boards need a hardware UART, enough RAM and accurate baud support; UNO/SoftwareSerial is unverified.


## History

* 2026/10/23 - V1.0.0 version


## Credits

Written by cdjq, 2026. Final maintainer and contact await confirmation. [DFRobot](https://www.dfrobot.com/)
