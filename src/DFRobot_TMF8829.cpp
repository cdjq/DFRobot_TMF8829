/*!
 * @file DFRobot_TMF8829.cpp
 * @brief UART request/response implementation.
 * @details Bounded mixed AT/XYZ parser using automatic or optional caller-owned storage.
 * @n Wire records are decoded explicitly; no packed structs or allocations during update.
 * @copyright Copyright (c) 2026 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license The MIT License (MIT)
 * @author [PLELES](li.jia@dfrobot.com)
 * @version V1.0.0
 * @date 2026-10-23
 * @url https://github.com/cdjq/DFRobot_TMF8829
 */
#include "DFRobot_TMF8829.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

namespace {
#if defined(ARDUINO_ARCH_ESP32)
// Prefer the begin() threshold argument so the port is started only once.
template <typename T>
auto startUART(T *serial, uint32_t baud, int8_t rx, int8_t tx, int)
  -> decltype(serial->begin(baud, SERIAL_8N1, rx, tx, false, 20000UL, 64), bool())
{
  serial->begin(baud, SERIAL_8N1, rx, tx, false, 20000UL, 64);
  return true;
}
template <typename T>
bool startUART(T *serial, uint32_t baud, int8_t rx, int8_t tx, long)
{
  serial->begin(baud, SERIAL_8N1, rx, tx);
  return false;
}
template <typename T>
auto configureRxFIFO(T *serial, int)
  -> decltype(serial->setRxFIFOFull(64), bool())
{
  return serial->setRxFIFOFull(64);
}
// Very old cores offer neither interface; retain their default threshold.
inline bool configureRxFIFO(...) { return false; }
#endif
const uint8_t EVENT_OK = 1;
const uint8_t EVENT_ERROR = 2;
const uint8_t EVENT_FRAME = 3;
uint16_t le16(const uint8_t *p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
}

DFRobot_TMF8829::DFRobot_TMF8829(HardwareSerial *serial, eBaudRate_t baudRate, int8_t rxPin, int8_t txPin) : _pSerial(serial), _pBuffer(NULL), _capacity(0), _ownsBuffer(false), _baudRate(baudRate), _rxPin(rxPin), _txPin(txPin), _initialized(false), _configValid(false), _pendingFrame(false), _dirty(false), _haveFrameId(false), _lastFrameId(0), _queryMask(0), _queryBad(false), _queryBaud(eBaudRate921600), _baudFound(false), _query(0)
{
  memset(&_frame, 0, sizeof(_frame));
  memset(&_config, 0, sizeof(_config));
  memset(&_queryConfig, 0, sizeof(_queryConfig));
  resetParser();
}

DFRobot_TMF8829::~DFRobot_TMF8829()
{
  if (_ownsBuffer) free(_pBuffer);
}

void DFRobot_TMF8829::invalidate(void)
{
  _frame.valid = false;
  _frame.isNew = false;
  _frame.pointCount = 0;
  _frame.points = _pBuffer;
}

bool DFRobot_TMF8829::setFrameBuffer(sPoint_t *buffer, size_t capacity)
{
  if (!buffer || !capacity || _pendingFrame || _wireCount) return false;
  if (buffer == _pBuffer) {
    if (capacity > _capacity) return false;
  } else {
    if (_ownsBuffer) free(_pBuffer);
    _ownsBuffer = false;
  }
  _pBuffer = buffer;
  _capacity = capacity;
  invalidate();
  return true;
}

bool DFRobot_TMF8829::validBaud(uint32_t baud)
{
  switch (baud) {
    case 115200: case 460800: case 921600: case 1000000:
    case 2000000: case 3000000: case 4000000: case 5000000: return true;
    default: return false;
  }
}

const char *DFRobot_TMF8829::modeName(eMeasurementMode_t mode)
{
  switch (mode) {
    case eMode8X8: return "8X8";
    case eMode16X16: return "16X16";
    case eMode32X32: return "32X32";
    case eMode48X32: return "48X32";
    case eModeH8X8: return "H_8X8";
    case eModeH16X16: return "H_16X16";
    case eModeH32X32: return "H_32X32";
    case eModeH48X32: return "H_48X32";
    case eModeL8X8: return "L_8X8";
    default: return NULL;
  }
}

bool DFRobot_TMF8829::dimensions(eMeasurementMode_t mode, uint8_t &w, uint8_t &h, uint8_t &p)
{
  switch (mode) {
    case eMode8X8: case eModeH8X8: case eModeL8X8: w = h = 8; p = 4; return true;
    case eMode16X16: case eModeH16X16: w = h = 16; p = 4; return true;
    case eMode32X32: case eModeH32X32: w = h = 32; p = 3; return true;
    case eMode48X32: case eModeH48X32: w = 48; h = 32; p = 2; return true;
    default: return false;
  }
}

bool DFRobot_TMF8829::openSerial(void)
{
  if (!_pSerial || !validBaud(_baudRate)) return false;
#if defined(ARDUINO_ARCH_ESP32)
  // Reserve enough UART RX storage for the largest frame (27663 bytes).
  // Stop a previously opened port before resizing (2.x/3.x requirement).
  _pSerial->end();
  size_t rxBufferSize = _pSerial->setRxBufferSize(32768);
  startUART(_pSerial, uint32_t(_baudRate), _rxPin, _txPin, 0);
  // Legacy cores resize the live UART instead of storing a pre-begin size.
  if (rxBufferSize != 32768) rxBufferSize = _pSerial->setRxBufferSize(32768);

  if (rxBufferSize != 32768) {
    _pSerial->end();
    return false;
  }
  // Leave interrupt latency headroom at high baud rates instead of waiting
  // until the hardware FIFO is almost full (the ESP32 default is 120 bytes).
  // Some modern cores override the begin threshold; apply their setter too.
  configureRxFIFO(_pSerial, 0);
#else
  _pSerial->begin(uint32_t(_baudRate), SERIAL_8N1);
#endif
  return true;
}

bool DFRobot_TMF8829::send(const char *command)
{
  const size_t n = strlen(command);
  if (!_pSerial || !n || n + 2 >= sizeof(_line)) return false;

  return _pSerial->write(reinterpret_cast<const uint8_t *>(command), n) == n &&
         _pSerial->write(reinterpret_cast<const uint8_t *>("\r\n"), 2) == 2;
}

void DFRobot_TMF8829::resetParser(void)
{
  _wireCount = _wireTotal = 0;
  _wireCRC = 0xFFFF;
  _store = false;
  _lineLength = 0;
  _lineOverflow = false;
}

uint16_t DFRobot_TMF8829::crcByte(uint16_t crc, uint8_t value)
{
  crc ^= value;
  for (uint8_t i = 0; i < 8; ++i) crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
  return crc;
}

uint8_t DFRobot_TMF8829::wireByte(uint32_t index) const
{
  return index < sizeof(_prefix) ? _prefix[index] :
         reinterpret_cast<const uint8_t *>(_pBuffer)[index - sizeof(_prefix)];
}

// Retain a candidate inside the invalid frame, or its final AA byte.
// Move raw data towards the start; a fifteen-byte temporary protects the prefix.
void DFRobot_TMF8829::resync(void)
{
  uint32_t offset = _wireCount;
  if (_store) {
    for (uint32_t i = 1; i < _wireCount; ++i) {
      if (wireByte(i) == 0xAA && (i + 1 == _wireCount || wireByte(i + 1) == 0x55)) {
        offset = i;
        break;
      }
    }
  } else if (_wireCount && _end[3] == 0xAA) {
    offset = _wireCount - 1;
  }
  const uint32_t remaining = _wireCount - offset;
  uint8_t head[15];
  if (!_store && remaining == 1) head[0] = 0xAA;
  else for (uint32_t i = 0; i < remaining && i < sizeof(head); ++i) head[i] = wireByte(offset + i);
  if (remaining > sizeof(head)) {
    uint8_t *raw = reinterpret_cast<uint8_t *>(_pBuffer);
    memmove(raw, raw + offset, remaining - sizeof(head));
  }
  resetParser();
  if (remaining) memcpy(_prefix, head, remaining < sizeof(head) ? remaining : sizeof(head));
  _wireCount = remaining;
  _store = remaining != 0;
  // Rebuild CRC and trailer for the retained candidate, without recursive parsing.
  if (remaining >= 4) _wireTotal = uint32_t(le16(_prefix + 2)) + 6;
  memset(_end, 0, sizeof(_end));
  for (uint32_t i = 2; i < remaining; ++i) {
    const uint8_t b = wireByte(i);
    if (i < _wireTotal - 4) _wireCRC = crcByte(_wireCRC, b);
    if (_wireTotal >= 4 && i >= _wireTotal - 4 && i < _wireTotal) _end[i - (_wireTotal - 4)] = b;
  }
}

bool DFRobot_TMF8829::inspect(uint8_t &event)
{
  while (_wireCount >= 4) {
    _wireTotal = uint32_t(le16(_prefix + 2)) + 6;
    bool bad = _wireTotal < 24 || _wireTotal > TMF8829_MAX_POINTS * TMF8829_POINT_BYTES + 15;
    if (!bad && _wireCount >= 11) {
      const uint8_t w = _prefix[4], h = _prefix[5], p = _prefix[10];
      const uint32_t slots = uint32_t(w) * h * p;
      const bool shape = (w == 1 && h == 1) ||
        ((w == 8 || w == 16 || w == 32 || w == 48) && (h == 1 || h == (w == 48 ? 32 : w)));
      const uint8_t maxPeak = w == 48 ? 2 : (w == 32 ? 3 : 4);
      bad = !shape || !p || p > maxPeak || slots > TMF8829_MAX_POINTS ||
            _wireTotal != slots * TMF8829_POINT_BYTES + 15;
      if (!bad && slots > _capacity) _store = false;
    }
    if (!bad && _wireCount >= _wireTotal) {
      bad = _wireCRC != le16(_end) || _end[2] != 0x55 || _end[3] != 0xAA;
      if (!bad) { event = EVENT_FRAME; return true; }
      _pendingFrame = false; // Entire corrupt response has been consumed.
    }
    if (!bad) return false;
    resync();
  }
  return false;
}

bool DFRobot_TMF8829::feed(uint8_t value, uint8_t &event)
{
  event = 0;
  if (!_wireCount) {
    if (value == 0xAA) {
      _prefix[0] = value;
      _wireCount = 1;
      _store = _pBuffer != NULL;
      _wireCRC = 0xFFFF;
      _lineLength = 0;
      _lineOverflow = false;
      memset(_end, 0, sizeof(_end));
      return false;
    }
    if (value == '\n') {
      if (!_lineOverflow) {
        if (_lineLength && _line[_lineLength - 1] == '\r') --_lineLength;
        _line[_lineLength] = 0;
        if (!strcmp(_line, "OK")) event = EVENT_OK;
        else if (!strcmp(_line, "ERROR")) event = EVENT_ERROR;
        else if (_query) parseField(_line);
      }
      _lineLength = 0;
      _lineOverflow = false;
      return event != 0;
    }
    if (value == '\r' || (value >= 32 && value <= 126)) {
      if (_lineLength < sizeof(_line) - 1) _line[_lineLength++] = char(value);
      else _lineOverflow = true;
    } else {
      _lineLength = 0;
      _lineOverflow = false;
    }
    return false;
  }
  if (_wireCount == 1 && value != 0x55) {
    resetParser();
    return feed(value, event);
  }
  const uint32_t i = _wireCount++;
  if (i < sizeof(_prefix)) _prefix[i] = value;
  else if (_store) reinterpret_cast<uint8_t *>(_pBuffer)[i - sizeof(_prefix)] = value;
  if (i >= 2 && (i < 4 || i < _wireTotal - 4)) _wireCRC = crcByte(_wireCRC, value);
  if (_wireTotal >= 4 && i >= _wireTotal - 4 && i < _wireTotal) _end[i - (_wireTotal - 4)] = value;
  return inspect(event);
}

bool DFRobot_TMF8829::poll(uint8_t &event)
{
  while (_pSerial->available() > 0) {
    const int value = _pSerial->read();
    if (value < 0) break;
    if (feed(uint8_t(value), event)) return true;
  }
  event = 0;
  return false;
}

bool DFRobot_TMF8829::number(const char *text, long &value)
{
  if (!text || !*text || (*text != '-' && (*text < '0' || *text > '9'))) return false;
  char *end;
  errno = 0;
  const long v = strtol(text, &end, 10);
  if (*end || errno == ERANGE) return false;
  value = v;
  return true;
}

bool DFRobot_TMF8829::parseField(char *line)
{
  if (_query == 2) {
    if (!strncmp(line, "+BAUD:", 6)) {
      long n;
      if (!number(line + 6, n) || n < 0 || !validBaud(uint32_t(n))) _queryBad = true;
      else { _queryBaud = eBaudRate_t(n); _baudFound = true; }
    }
    return true;
  }
  if (strncmp(line, "+CONFIG:", 8)) return true;
  char *key = line + 8;
  char *value = strchr(key, '=');
  if (!value) { _queryBad = true; return false; }
  *value++ = 0;
  uint8_t bit = 0;
  bool ok = true;
  long n = 0;
  if (!strcmp(key, "MODE")) {
    bit = 1;
    ok = false;
    for (uint8_t i = 0; i < 9; ++i) {
      if (!strcmp(value, modeName(eMeasurementMode_t(i)))) { _queryConfig.mode = eMeasurementMode_t(i); ok = true; break; }
    }
  } else if (!strcmp(key, "SELECT")) {
    bit = 8;
    if (!strcmp(value, "FRAME")) _queryConfig.outputMode = eOutputFrame;
    else if (!strcmp(value, "POINT")) _queryConfig.outputMode = eOutputPoint;
    else if (!strcmp(value, "LINE")) _queryConfig.outputMode = eOutputLine;
    else ok = false;
  } else if (!strcmp(key, "PEAK")) {
    bit = 2; ok = number(value, n) && n >= 1 && n <= 4;
    if (ok) _queryConfig.peakCount = uint8_t(n);
  } else if (!strcmp(key, "REPORT")) {
    bit = 4; ok = number(value, n) && n >= 0 && n <= 1;
    if (ok) _queryConfig.reportMode = uint8_t(n);
  } else if (!strcmp(key, "X") || !strcmp(key, "Y")) {
    bit = *key == 'X' ? 16 : 32;
    ok = number(value, n) && n >= -1 && n <= 255;
    if (ok) { if (bit == 16) _queryConfig.x = int16_t(n); else _queryConfig.y = int16_t(n); }
  } else if (!strcmp(key, "BAUD")) {
    bit = 64; ok = number(value, n) && n >= 0 && validBaud(uint32_t(n));
    if (ok) _queryConfig.baudRate = eBaudRate_t(n);
  }
  if (!ok || (bit && (_queryMask & bit))) _queryBad = true;
  _queryMask |= bit;
  return ok;
}

bool DFRobot_TMF8829::validConfig(const sConfig_t &c) const
{
  uint8_t w, h, p;
  if (!dimensions(c.mode, w, h, p) || !c.peakCount || c.peakCount > p || c.reportMode > 1 || !validBaud(c.baudRate)) return false;
  switch (c.outputMode) {
    case eOutputFrame: return c.x == -1 && c.y == -1;
    case eOutputPoint: return c.x >= 0 && c.x < w && c.y >= 0 && c.y < h;
    case eOutputLine: return c.x == -1 && c.y >= 0 && c.y < h;
    default: return false;
  }
}

// Drain late replies before opening a new transaction. If bytes were lost,
// abandon the incomplete transaction after the recovery deadline and UART idle.
bool DFRobot_TMF8829::prepare(void)
{
  const uint32_t start = millis();
  uint32_t quiet = start;
  do {
    const bool available = _pSerial->available() > 0;
    uint8_t event = 0;
    if (poll(event)) {
      if (event == EVENT_FRAME || event == EVENT_ERROR) _pendingFrame = false;
      if (event == EVENT_FRAME) { invalidate(); resetParser(); }
      if (event == EVENT_OK || event == EVENT_ERROR) _dirty = false;
    }
    if (available || event) quiet = millis();
    // After a complete corrupt response, embedded AA55 bytes may leave a false
    // candidate. Discard it once the transaction is over and UART is quiet;
    // incomplete replies get the longer recovery window below.
    if (!_pendingFrame && !_dirty && _wireCount && uint32_t(millis() - quiet) >= 3) resetParser();
    if (!_pendingFrame && !_wireCount && !_dirty && uint32_t(millis() - quiet) >= 3) return true;
    delay(1);
  } while (uint32_t(millis() - start) < TMF8829_CONFIG_TIMEOUT);
  // A missing tail can never complete. Allow recovery only after a quiet gap,
  // so a response that is still arriving is not cut off by the deadline.
  if (!_pSerial->available() && uint32_t(millis() - quiet) >= 100) {
    resetParser();
    _pendingFrame = false;
    _dirty = false;
    _query = 0;
    invalidate();

    return true;
  }
  return false;
}

bool DFRobot_TMF8829::transact(const char *command, uint32_t timeoutMs, uint8_t query)
{
  if (!prepare()) return false;
  _query = query;
  _queryMask = 0;
  _queryBad = false;
  _baudFound = false;
  memset(&_queryConfig, 0, sizeof(_queryConfig));
  if (!send(command)) { _query = 0; _dirty = true; return false; }
  _dirty = true;
  const uint32_t start = millis();
  do {
    uint8_t event = 0;
    if (poll(event)) {
      if (event == EVENT_FRAME) { invalidate(); resetParser(); }
      else {
        _query = 0;
        _dirty = false;
        return event == EVENT_OK && !_queryBad &&
          (query != 1 || (_queryMask == 127 && validConfig(_queryConfig))) &&
          (query != 2 || _baudFound);
      }
    }
    delay(1);
  } while (uint32_t(millis() - start) < timeoutMs);

  _query = 0;
  return false;
}

bool DFRobot_TMF8829::begin(void)
{
  invalidate();
  _initialized = _configValid = false;

  if (!_pBuffer) {
    _pBuffer = static_cast<sPoint_t *>(malloc(TMF8829_MAX_POINTS * sizeof(sPoint_t)));
    if (!_pBuffer) return false;
    _capacity = TMF8829_MAX_POINTS;
    _ownsBuffer = true;
    invalidate();
  }

  if (!openSerial()) return false;
  // REPORT may already be active. Send STOP directly, parsing in-flight frames
  // while waiting for its acknowledgement instead of waiting for a quiet stream.

  bool stopped = false;
  for (uint8_t attempt = 1; attempt <= 3 && !stopped; ++attempt) {
    if (!send("AT+REPORT=0")) continue;
    _dirty = true;
    const uint32_t start = millis();
    do {
      uint8_t event = 0;
      if (poll(event)) {
        if (event == EVENT_FRAME) { resetParser(); _pendingFrame = false; }
        else {
          stopped = event == EVENT_OK;
          _dirty = false;
          break;
        }
      }
      delay(1);
    } while (uint32_t(millis() - start) < TMF8829_CONFIG_TIMEOUT);
  }
  if (!stopped) return false;

  if (!transact("AT", TMF8829_AT_TIMEOUT)) return false;
  _initialized = true;

  sConfig_t c;
  if (!getConfig(c)) { _initialized = false; return false; }

  if (c.reportMode != 0 || c.baudRate != _baudRate) { _initialized = false; return false; }

  return true;
}

bool DFRobot_TMF8829::getConfig(sConfig_t &config)
{
  if (!_initialized) return false;
  invalidate();
  _configValid = false;
  if (!transact("AT+CONFIG?", TMF8829_AT_TIMEOUT, 1)) return false;
  _config = _queryConfig;
  _configValid = true;
  config = _config;
  return true;
}

bool DFRobot_TMF8829::ensureConfig(void)
{
  if (!_initialized) return false;
  sConfig_t c;
  return (_configValid || getConfig(c)) && _config.reportMode == 0;
}

bool DFRobot_TMF8829::change(const char *command, uint32_t timeoutMs)
{
  invalidate();
  _configValid = false;
  if (!transact(command, timeoutMs)) return false;
  sConfig_t c;
  return getConfig(c) && c.reportMode == 0;
}

bool DFRobot_TMF8829::setSensorMode(eMeasurementMode_t mode)
{
  const char *name = modeName(mode);
  if (!name || !ensureConfig()) return false;
  const bool different = _config.mode != mode;
  char command[32];
  snprintf(command, sizeof(command), "AT+MODE=%s", name);
  if (!change(command, TMF8829_CONFIG_TIMEOUT)) return false;
  return _config.mode == mode && (!different || (_config.peakCount == 1 && _config.outputMode == eOutputFrame));
}

bool DFRobot_TMF8829::setPeakCount(uint8_t peakCount)
{
  if (!peakCount || peakCount > 4 || !ensureConfig()) return false;
  uint8_t w, h, p;
  if (!dimensions(_config.mode, w, h, p) || peakCount > p) return false;
  char command[24];
  snprintf(command, sizeof(command), "AT+PEAK=%u", unsigned(peakCount));
  return change(command) && _config.peakCount == peakCount;
}

bool DFRobot_TMF8829::setMeasureMode(uint8_t x, uint8_t y)
{
  if (!ensureConfig()) return false;
  uint8_t w, h, p;
  if (!dimensions(_config.mode, w, h, p) || x >= w || y >= h) return false;
  char command[32];
  snprintf(command, sizeof(command), "AT+POINT=%u,%u", unsigned(x), unsigned(y));
  return change(command) && _config.outputMode == eOutputPoint && _config.x == x && _config.y == y;
}

bool DFRobot_TMF8829::setMeasureMode(uint8_t y)
{
  if (!ensureConfig()) return false;
  uint8_t w, h, p;
  if (!dimensions(_config.mode, w, h, p) || y >= h) return false;
  char command[24];
  snprintf(command, sizeof(command), "AT+LINE=%u", unsigned(y));
  return change(command) && _config.outputMode == eOutputLine && _config.y == y;
}

bool DFRobot_TMF8829::setMeasureMode(void)
{
  return ensureConfig() && change("AT+FRAME") && _config.outputMode == eOutputFrame;
}

bool DFRobot_TMF8829::getBaudRate(eBaudRate_t &baudRate)
{
  if (!_initialized) return false;
  if (!transact("AT+BAUD?", TMF8829_AT_TIMEOUT, 2)) { _configValid = false; return false; }
  baudRate = _queryBaud;
  if (_configValid && _config.baudRate != baudRate) { invalidate(); _config.baudRate = baudRate; }
  return true;
}

bool DFRobot_TMF8829::setBaudRate(eBaudRate_t baudRate)
{
  if (!validBaud(baudRate) || !ensureConfig()) return false;
  invalidate();
  char command[32];
  snprintf(command, sizeof(command), "AT+BAUD=%lu", static_cast<unsigned long>(baudRate));
  if (!transact(command, TMF8829_AT_TIMEOUT)) { _initialized = _configValid = false; return false; }
  _baudRate = baudRate;
  _pSerial->flush(); // TX completion only; incoming bytes are never blindly cleared.
  _pSerial->end();
  if (!openSerial() || !transact("AT", TMF8829_AT_TIMEOUT)) { _initialized = _configValid = false; return false; }
  eBaudRate_t actual;
  if (!getBaudRate(actual) || actual != baudRate) { _initialized = _configValid = false; return false; }
  return true;
}

bool DFRobot_TMF8829::saveConfig(void)
{
  return ensureConfig() && transact("AT+SAVE", TMF8829_CONFIG_TIMEOUT);
}

bool DFRobot_TMF8829::frameMatches(void) const
{
  uint8_t w, h, p;
  if (!_store || !_configValid || !dimensions(_config.mode, w, h, p)) return false;
  if (_config.outputMode == eOutputPoint) w = h = 1;
  else if (_config.outputMode == eOutputLine) h = 1;
  return _prefix[4] == w && _prefix[5] == h && _prefix[10] == _config.peakCount;
}

void DFRobot_TMF8829::publish(void)
{
  const uint16_t count = uint16_t(uint32_t(_prefix[4]) * _prefix[5] * _prefix[10]);
  // Backward expansion is safe even if native sPoint_t has padding. Source bytes
  // occupy the same point buffer, starting four bytes before each packed record.
  for (uint16_t n = count; n; --n) {
    const uint16_t i = n - 1;
    uint8_t raw[9];
    for (uint8_t k = 0; k < sizeof(raw); ++k) raw[k] = wireByte(11 + uint32_t(i) * 9 + k);
    sPoint_t point;
    point.x = int16_t(le16(raw));
    point.y = int16_t(le16(raw + 2));
    point.z = int16_t(le16(raw + 4));
    point.signal = le16(raw + 6);
    point.confidence = raw[8];
    _pBuffer[i] = point;
  }
  _frame.width = _prefix[4];
  _frame.height = _prefix[5];
  _frame.peakCount = _prefix[10];
  _frame.frameId = uint32_t(_prefix[6]) | (uint32_t(_prefix[7]) << 8) |
                   (uint32_t(_prefix[8]) << 16) | (uint32_t(_prefix[9]) << 24);
  _frame.pointCount = count;
  _frame.points = _pBuffer;
  _frame.isNew = !_haveFrameId || _lastFrameId != _frame.frameId;
  _frame.valid = true;
  _haveFrameId = true;
  _lastFrameId = _frame.frameId;
}

bool DFRobot_TMF8829::update(uint32_t timeoutMs)
{
  invalidate();
  if (!timeoutMs) return false;
  if (!ensureConfig()) return false;
  if (!prepare()) return false;
  if (!send("AT+GETFRAME")) return false;
  _pendingFrame = true;
  const uint32_t start = millis();
  do {
    uint8_t event = 0;
    if (poll(event)) {
      if (event == EVENT_ERROR) { _pendingFrame = false; return false; }
      if (event == EVENT_FRAME) {
        _pendingFrame = false;
        const bool valid = frameMatches();
        if (valid) publish();
        resetParser();
        return valid;
      }
    }
    delay(1);
  } while (uint32_t(millis() - start) < timeoutMs);

  return false;
}

const DFRobot_TMF8829::sFrame_t &DFRobot_TMF8829::getAllPointData(void) const { return _frame; }
bool DFRobot_TMF8829::isDataNew(void) const { return _frame.isNew; }
bool DFRobot_TMF8829::isDataValid(void) const { return _frame.valid; }

bool DFRobot_TMF8829::getPointData(uint8_t x, uint8_t y, uint8_t peak, sPoint_t &point) const
{
  if (!_frame.valid || x >= _frame.width || y >= _frame.height || peak >= _frame.peakCount) return false;
  point = _pBuffer[(uint16_t(y) * _frame.width + x) * _frame.peakCount + peak];
  return true;
}
