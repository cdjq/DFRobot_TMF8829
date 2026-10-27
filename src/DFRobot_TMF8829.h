/*!
 * @file DFRobot_TMF8829.h
 * @brief UART request/response driver for the TMF8829 point-cloud module.
 * @details Automatic point storage, mixed AT/binary parsing and XYZ decoding.
 * @n This driver targets the firmware specified in task.md, not the bare IC.
 * @copyright Copyright (c) 2026 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license The MIT License (MIT)
 * @author [PLELES](li.jia@dfrobot.com)
 * @version V1.0.0
 * @date 2026-10-23
 * @url https://github.com/cdjq/DFRobot_TMF8829
 */
#ifndef __DFROBOT_TMF8829_H__
#define __DFROBOT_TMF8829_H__

#include <Arduino.h>
#include <HardwareSerial.h>


#define TMF8829_AT_TIMEOUT       1000UL   ///< Ordinary AT deadline in milliseconds
#define TMF8829_CONFIG_TIMEOUT   5000UL   ///< Mode, recovery and NVS deadline in milliseconds
#define TMF8829_MAX_POINTS       3072UL   ///< Maximum legal XYZ slots in supported modes
#define TMF8829_POINT_BYTES      9UL      ///< Bytes per wire-format XYZ slot

class DFRobot_TMF8829 {
public:
  /** @enum eMeasurementMode_t
   *  @brief Measurement presets; mapped explicitly to firmware strings.
   */
  typedef enum {
    eMode8X8 = 0,    /**< 8 by 8, at most 4 peaks */
    eMode16X16,      /**< 16 by 16, at most 4 peaks */
    eMode32X32,      /**< 32 by 32, at most 3 peaks */
    eMode48X32,      /**< 48 by 32, at most 2 peaks */
    eModeH8X8,       /**< H_8X8 preset, at most 4 peaks */
    eModeH16X16,     /**< H_16X16 preset, at most 4 peaks */
    eModeH32X32,     /**< H_32X32 preset, at most 3 peaks */
    eModeH48X32,     /**< H_48X32 preset, at most 2 peaks */
    eModeL8X8        /**< L_8X8 preset, at most 4 peaks */
  } eMeasurementMode_t;

  /** @enum eOutputMode_t
   *  @brief Spatial output selection, independent of measurement preset.
   */
  typedef enum {
    eOutputFrame = 0, /**< Full image */
    eOutputPoint = 1, /**< Selected pixel, all peaks */
    eOutputLine = 2   /**< Selected row, all peaks */
  } eOutputMode_t;

  /** @enum eBaudRate_t
   *  @brief Supported UART baud rates, represented by their actual values.
   */
  typedef enum : uint32_t {
    eBaudRate115200  = 115200UL,  /**< 115200 baud */
    eBaudRate460800  = 460800UL,  /**< 460800 baud */
    eBaudRate921600  = 921600UL,  /**< 921600 baud, firmware default */
    eBaudRate1000000 = 1000000UL, /**< 1000000 baud */
    eBaudRate2000000 = 2000000UL, /**< 2000000 baud */
    eBaudRate3000000 = 3000000UL, /**< 3000000 baud */
    eBaudRate4000000 = 4000000UL, /**< 4000000 baud */
    eBaudRate5000000 = 5000000UL  /**< 5000000 baud */
  } eBaudRate_t;

  /** @struct sPoint_t
   *  @brief One XYZ slot; native sizeof may exceed the nine wire bytes.
   */
  typedef struct {
    int16_t  x;          /**< X in mm */
    int16_t  y;          /**< Y in mm */
    int16_t  z;          /**< Z in mm */
    uint16_t signal;     /**< Raw signal; zero is preserved */
    uint8_t  confidence; /**< Raw confidence; zero slots are preserved */
  } sPoint_t;

  /** @struct sFrame_t
   *  @brief Validated frame metadata and borrowed point storage.
   */
  typedef struct {
    uint8_t         width;      /**< Output-local width */
    uint8_t         height;     /**< Output-local height */
    uint8_t         peakCount;  /**< Slots per pixel */
    uint32_t        frameId;    /**< Acquisition sequence, may wrap */
    uint16_t        pointCount; /**< width * height * peakCount when valid */
    const sPoint_t *points;     /**< Borrowed array managed by the sensor object */
    bool            isNew;      /**< ID differs from last successful frame */
    bool            valid;      /**< Complete protocol validation passed */
  } sFrame_t;

  /** @struct sConfig_t
   *  @brief Actual firmware configuration returned by CONFIG?.
   */
  typedef struct {
    eMeasurementMode_t mode;       /**< Resolution/preset */
    uint8_t            peakCount;  /**< One to preset maximum */
    uint8_t            reportMode; /**< Raw REPORT field; operation requires zero */
    eOutputMode_t      outputMode; /**< FRAME, POINT or LINE */
    int16_t            x;          /**< Original pixel column, otherwise -1 */
    int16_t            y;          /**< Original row, otherwise -1 */
    eBaudRate_t        baudRate;   /**< Validated supported rate */
  } sConfig_t;

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

private:
  bool openSerial(void);
  bool send(const char *command);
  bool transact(const char *command, uint32_t timeoutMs, uint8_t query = 0);
  bool prepare(void);
  bool ensureConfig(void);
  bool change(const char *command, uint32_t timeoutMs = TMF8829_AT_TIMEOUT);
  bool poll(uint8_t &event);
  bool feed(uint8_t value, uint8_t &event);
  bool inspect(uint8_t &event);
  void resync(void);
  uint8_t wireByte(uint32_t index) const;
  void resetParser(void);
  void invalidate(void);
  bool parseField(char *line);
  bool validConfig(const sConfig_t &config) const;
  static bool dimensions(eMeasurementMode_t mode, uint8_t &width, uint8_t &height, uint8_t &maxPeak);
  static const char *modeName(eMeasurementMode_t mode);
  static bool validBaud(uint32_t baud);
  static bool number(const char *text, long &value);
  static uint16_t crcByte(uint16_t crc, uint8_t value);
  bool frameMatches(void) const;
  void publish(void);

  HardwareSerial * _pSerial;
  sPoint_t *       _pBuffer;
  size_t           _capacity;
  bool             _ownsBuffer;
  eBaudRate_t      _baudRate;
  int8_t           _rxPin;
  int8_t           _txPin;
  bool             _initialized;
  bool             _configValid;
  bool             _pendingFrame;
  bool             _dirty;
  bool             _haveFrameId;
  uint32_t         _lastFrameId;
  sFrame_t         _frame;
  sConfig_t        _config;
  sConfig_t        _queryConfig;
  uint8_t          _queryMask;
  bool             _queryBad;
  eBaudRate_t      _queryBaud;
  bool             _baudFound;
  uint8_t          _query;
  uint8_t          _prefix[15];
  uint32_t         _wireCount;
  uint32_t         _wireTotal;
  bool             _store;
  uint16_t         _wireCRC;
  uint8_t          _end[4];
  char             _line[128];
  uint8_t          _lineLength;
  bool             _lineOverflow;
};
#endif
