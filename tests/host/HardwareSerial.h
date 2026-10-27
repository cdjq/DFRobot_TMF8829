#pragma once
#include "Arduino.h"
#include <deque>
#include <string>
#include <vector>
#include <functional>
class HardwareSerial {
public:
  std::deque<uint8_t> rx;
  std::string tx;
  std::vector<std::string> commands;
  std::function<void(const std::string &)> respond;
  uint32_t baud = 0;
  bool running = false;
  bool failResize = false;
  size_t rxBufferSize = 256;
  unsigned beginCount = 0;
  int8_t rxPin = -1, txPin = -1;
  size_t setRxBufferSize(size_t size) {
    if (failResize) return 0;
#if defined(TMF_TEST_LEGACY_CORE)
    if (!running) return 0;
#else
    if (running) return 0;
#endif
    rxBufferSize = size;
    return size;
  }
#if !defined(TMF_TEST_OLD_CORE) && !defined(TMF_TEST_LEGACY_CORE)
  bool setRxFIFOFull(uint8_t) { return true; }
#endif
#if defined(TMF_TEST_OLD_CORE)
  void begin(uint32_t b, int config, int8_t r, int8_t t, bool, unsigned long, uint8_t) { begin(b, config, r, t); }
#endif
  void begin(uint32_t b, int = 0, int8_t r = -1, int8_t t = -1) {
    baud = b; rxPin = r; txPin = t; running = true; ++beginCount;
  }
  void end() { running = false; }
  void flush() {}
  int available() { return int(rx.size()); }
  int read() { if (rx.empty()) return -1; int b = rx.front(); rx.pop_front(); return b; }
  size_t write(const uint8_t *p, size_t n) {
    for (size_t i = 0; i < n; ++i) {
      tx += char(p[i]);
      if (p[i] == '\n') {
        std::string c = tx.substr(0, tx.size()-2);
        tx.clear(); commands.push_back(c); if (respond) respond(c);
      }
    }
    return n;
  }
  void push(const std::string &s) { for (unsigned char c : s) rx.push_back(c); }
  void push(const std::vector<uint8_t> &v) { for (auto c : v) rx.push_back(c); }
};
