#include "DFRobot_TMF8829.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <functional>
#include <algorithm>

static uint32_t now = 0;
static std::function<void()> tick;
uint32_t millis() { return now; }
void delay(uint32_t ms) { now += ms; if (tick) tick(); }
using Sensor = DFRobot_TMF8829;
static uint16_t crc(const std::vector<uint8_t> &v, size_t start, size_t end) {
  uint16_t c = 0xffff;
  for (size_t i=start;i<end;++i) {
    c ^= v[i];
    for (int b=0;b<8;++b) c = (c&1) ? (c>>1)^0xa001 : c>>1;
  }
  return c;
}
static void word(std::vector<uint8_t> &v, uint16_t n) { v.push_back(n&255); v.push_back(n>>8); }
static std::vector<uint8_t> frame(uint8_t w, uint8_t h, uint8_t peaks, uint32_t id=1) {
  std::vector<uint8_t> v{0xaa,0x55};
  word(v, uint16_t(unsigned(w)*h*peaks*9+9));
  v.push_back(w); v.push_back(h);
  for (int i=0;i<4;++i) v.push_back(uint8_t(id>>(8*i)));
  v.push_back(peaks);
  for (unsigned i=0;i<unsigned(w)*h*peaks;++i) {
    word(v,uint16_t(-123-int(i))); word(v,uint16_t(456+i));
    word(v,uint16_t(-789-int(i))); word(v,i%2 ? 0x55aa : 0); v.push_back(i%2 ? 99 : 0);
  }
  word(v,crc(v,2,v.size())); v.push_back(0x55); v.push_back(0xaa);
  return v;
}

struct Device {
  HardwareSerial port;
  int mode=0, peak=1, report=1, select=0, x=-1,y=-1;
  uint32_t baud=115200,id=1;
  bool omitField=false, failBaud=false;
  int reportErrors=0, reportTimeouts=0, reportAttempts=0;
  std::function<void()> get;
  const char *names[9] = {"8X8","16X16","32X32","48X32","H_8X8","H_16X16","H_32X32","H_48X32","L_8X8"};
  Device() { port.respond = [this](const std::string &s) { command(s); }; }
  int width() { int m = mode==8 ? 0 : mode%4; return m==0 ? 8 : m==1 ? 16 : m==2 ? 32 : 48; }
  std::vector<uint8_t> data() { return frame(select==1?1:width(),select?1:std::min(width(),32),peak,id); }
  void command(const std::string &s) {
    if (s=="AT+GETFRAME") { if (get) get(); else port.push(data()); return; }
    if (s=="AT+CONFIG?") {
      // Deliberately shuffled fields and an unknown future field.
      port.push("\r\n+CONFIG:BAUD="+std::to_string(baud)+"\r\n+CONFIG:FUTURE=42\r\n+CONFIG:Y="+std::to_string(y)+
        "\r\n+CONFIG:SELECT="+(select==0?std::string("FRAME"):select==1?std::string("POINT"):std::string("LINE"))+
        "\r\n+CONFIG:PEAK="+std::to_string(peak)+"\r\n+CONFIG:X="+std::to_string(x)+"\r\n+CONFIG:MODE="+names[mode]+"\r\n");
      if (!omitField) port.push("+CONFIG:REPORT="+std::to_string(report)+"\r\n");
    } else if (s=="AT+BAUD?") port.push("+BAUD:"+std::to_string(baud)+"\r\n");
    else if (s=="AT+REPORT=0") {
      ++reportAttempts;
      if (reportErrors>0) { --reportErrors; port.push("\r\nERROR\r\n"); return; }
      if (reportTimeouts>0) { --reportTimeouts; return; }
      report=0;
    }
    else if (s.compare(0,8,"AT+MODE=")==0) {
      for (int m=0;m<9;++m) if (s.substr(8)==names[m] && m!=mode) { mode=m; peak=1; select=0; x=y=-1; break; }
    } else if (s.compare(0,8,"AT+PEAK=")==0) peak=std::stoi(s.substr(8));
    else if (s.compare(0,9,"AT+POINT=")==0) { select=1; std::sscanf(s.c_str()+9,"%d,%d",&x,&y); }
    else if (s.compare(0,8,"AT+LINE=")==0) { select=2; x=-1; y=std::stoi(s.substr(8)); }
    else if (s=="AT+FRAME") { select=0; x=y=-1; }
    else if (s.compare(0,8,"AT+BAUD=")==0) { baud=std::stoul(s.substr(8)); if (failBaud) return; }
    port.push("\r\nOK\r\n");
  }
};
static void checkPoints(Sensor &s) {
  const auto &f=s.getAllPointData(); assert(f.valid && f.pointCount);
  for (unsigned i=0;i<f.pointCount;++i) {
    assert(f.points[i].x==-123-int(i)); assert(f.points[i].y==456+int(i));
    assert(f.points[i].z==-789-int(i));
    assert(f.points[i].signal==(i%2?0x55aa:0)); assert(f.points[i].confidence==(i%2?99:0));
  }
}
int main() {
  const std::vector<uint8_t> digits{'1','2','3','4','5','6','7','8','9'};
  assert(crc(digits,0,digits.size())==0x4b37);
  Sensor nil(nullptr); assert(!nil.begin()); assert(!nil.setFrameBuffer(nullptr,1));
#if defined(ARDUINO_ARCH_ESP32)
  // Reclaim an already-open port, preserve pins, and start it only once.
  {
    Device device;
    device.port.begin(115200);
    device.baud = 2000000;
    Sensor tmf8829(&device.port, Sensor::eBaudRate2000000, 25, 26);
    assert(tmf8829.begin());
    assert(device.port.beginCount == 2);
    assert(device.port.rxBufferSize == 32768);
    assert(device.port.rxPin == 25 && device.port.txPin == 26);
    assert(tmf8829.begin());
    assert(device.port.beginCount == 3);
  }
  {
    Device device;
    device.port.failResize = true;
    Sensor tmf8829(&device.port);
    assert(!tmf8829.begin());
    assert(device.port.commands.empty());
    assert(!device.port.running);
  }
#endif
  // REPORT retries stop on success and never exceed three writes.
  for (int failures=0; failures<=3; ++failures) {
    Device retry;
    retry.reportErrors=failures;
    Sensor tmf8829(&retry.port);
    Sensor::sPoint_t buffer[1];
    assert(tmf8829.setFrameBuffer(buffer,1));
    assert(tmf8829.begin()==(failures<3));
    assert(retry.reportAttempts==std::min(failures+1,3));
  }
  for (int failures : {1,3}) {
    Device retry;
    retry.reportTimeouts=failures;
    Sensor tmf8829(&retry.port);
    Sensor::sPoint_t buffer[1];
    assert(tmf8829.setFrameBuffer(buffer,1));
    assert(tmf8829.begin()==(failures<3));
    assert(retry.reportAttempts==std::min(failures+1,3));
  }
  // Default storage supports every preset and peak count without user arrays.
  Device automatic;
  Sensor::sPoint_t borrowed[4];
  {
    Sensor tmf8829(&automatic.port);
    assert(tmf8829.begin());
    for (int mode=0; mode<9; ++mode) {
      assert(tmf8829.setSensorMode(Sensor::eMeasurementMode_t(mode)));
      assert(tmf8829.setPeakCount(automatic.width()==48?2:automatic.width()==32?3:4));
      assert(tmf8829.update()); checkPoints(tmf8829);
    }
    const auto *storage=tmf8829.getAllPointData().points;
    assert(tmf8829.begin());
    assert(tmf8829.update()); checkPoints(tmf8829);
    assert(tmf8829.getAllPointData().points==storage);
    assert(tmf8829.setMeasureMode(0,0));
    assert(tmf8829.setFrameBuffer(borrowed,4));
    assert(tmf8829.update()); checkPoints(tmf8829);
    assert(tmf8829.getAllPointData().points==borrowed);
  } // Destruction must not free borrowed storage.
  borrowed[0].x=42;
  {
    Sensor tmf8829(&automatic.port);
    assert(tmf8829.begin() && tmf8829.update()); checkPoints(tmf8829);
  } // Destruction releases automatic storage.
  Device d;
  struct { uint32_t before; Sensor::sPoint_t p[3072]; uint32_t after; } mem;
  mem.before=0x12345678; mem.after=0x87654321;
  Sensor s(&d.port);
  assert(s.setFrameBuffer(mem.p,3072));
  // An active frame already in flight contains binary marker bytes.
  d.port.push(frame(8,8,1));
  assert(s.begin() && d.report==0);
  assert(s.update()); checkPoints(s); assert(s.isDataNew());
  assert(s.update()); assert(!s.isDataNew());
  d.id=0xffffffff; assert(s.update() && s.isDataNew());
  d.id=0; assert(s.update() && s.isDataNew());
  Sensor::sPoint_t p{1,2,3,4,5};
  assert(!s.getPointData(8,0,0,p) && p.x==1);
  assert(s.getPointData(7,7,0,p) && p.x==-186);
  for (int mode=0;mode<9;++mode) {
    assert(s.setSensorMode(Sensor::eMeasurementMode_t(mode)));
    unsigned maxPeak=d.width()==48?2:d.width()==32?3:4;
    assert(s.setPeakCount(maxPeak));
    size_t n=d.port.commands.size(); assert(!s.setPeakCount(maxPeak+1)); assert(d.port.commands.size()==n);
    assert(s.update()); checkPoints(s);
    assert(s.setMeasureMode(2,3)); assert(s.update()); checkPoints(s);
    assert(s.getAllPointData().width==1 && s.getAllPointData().height==1);
    assert(s.setSensorMode(Sensor::eMeasurementMode_t(mode))); assert(d.select==1 && d.peak==int(maxPeak));
    assert(s.setMeasureMode(3)); assert(s.update()); checkPoints(s);
    assert(s.getAllPointData().height==1);
    assert(s.setMeasureMode());
  }
  assert(s.setSensorMode(Sensor::eMode8X8)); assert(s.setPeakCount(1));
  size_t n=d.port.commands.size(); assert(!s.setMeasureMode(8,0)); assert(d.port.commands.size()==n);
  assert(!s.setSensorMode(Sensor::eMeasurementMode_t(90)));
  Sensor::sConfig_t config; memset(&config,0,sizeof(config)); config.x=99;
  d.omitField=true; assert(!s.getConfig(config) && config.x==99); d.omitField=false;
  assert(s.setPeakCount(2)); assert(s.setPeakCount(1));
  // Fragment every byte, including text-like payload, with millis wraparound.
  auto bytes=d.data(); size_t cursor=0;
  d.get=[&] { cursor=0; tick=[&] { if (cursor<bytes.size()) d.port.rx.push_back(bytes[cursor++]); }; };
  now=0xffffff00; assert(s.update(2000)); tick={}; checkPoints(s);
  // Bad CRC/tail/length candidates followed by a complete valid frame.
  for (int error=0;error<4;++error) {
    auto bad=d.data();
    if (error==0) bad[bad.size()-4]^=1;
    if (error==1) bad.back()=0;
    if (error==2) bad[2]=0;
    if (error==3) bad[4]=0;
    d.get=[&,bad] { d.port.push(bad); d.port.push(d.data()); };
    assert(s.update(2000)); checkPoints(s);
  }
  // A legal frame nested inside a longer corrupt candidate must be retained.
  auto outer=frame(16,16,1); auto inner=d.data();
  std::copy(inner.begin(),inner.end(),outer.begin()+20);
  d.get=[&,outer] { d.port.push(outer); }; assert(s.update(2000)); checkPoints(s);
  // Payload contains CRLF OK CRLF: it cannot acknowledge a command.
  bytes=d.data(); const uint8_t marker[]={13,10,'O','K',13,10};
  std::copy(marker,marker+6,bytes.begin()+20); uint16_t c=crc(bytes,2,bytes.size()-4);
  bytes[bytes.size()-4]=c&255; bytes[bytes.size()-3]=c>>8;
  d.get=[&] { d.port.push(bytes); }; assert(s.update());
  // A timeout preserves a partial frame; next call drains late bytes then requests afresh.
  bytes=d.data(); d.get=[&] { d.port.push(std::vector<uint8_t>(bytes.begin(),bytes.begin()+30)); };
  assert(!s.update(20)); assert(!s.isDataValid() && !s.isDataNew() && s.getAllPointData().pointCount==0);
  n=d.port.commands.size(); uint32_t lateStart=now;
  tick=[&] {
    if (uint32_t(now-lateStart)>=100) {
      d.port.push(std::vector<uint8_t>(bytes.begin()+30,bytes.end())); tick={};
    }
  };
  d.get={}; ++d.id;
  assert(s.update()); assert(d.port.commands.size()==n+1);
  assert(s.getAllPointData().frameId==d.id);
  // A permanently lost tail must not block every subsequent request.
  d.get=[&] { d.port.push(std::vector<uint8_t>(bytes.begin(),bytes.begin()+30)); };
  assert(!s.update(20)); n=d.port.commands.size(); d.get={}; ++d.id;
  uint32_t recoveryStart=now;
  assert(s.update()); assert(d.port.commands.size()==n+1);
  assert(uint32_t(now-recoveryStart)>=TMF8829_CONFIG_TIMEOUT);
  assert(s.getAllPointData().frameId==d.id); checkPoints(s);
  // A completely missing reply can also recover after the bounded drain.
  d.get=[] {}; assert(!s.update(20)); d.get={}; assert(s.update());
  // An unsolicited complete old frame is discarded before sending GETFRAME.
  d.port.push(frame(8,8,1,12345)); assert(s.update()); assert(s.getAllPointData().frameId==d.id);
  // A fully consumed CRC failure allows the following call to recover.
  d.get=[&] { auto v=d.data(); v[v.size()-4]^=1; d.port.push(v); };
  assert(!s.update(30)); d.get={}; assert(s.update());
  assert(s.saveConfig());
  assert(s.setBaudRate(Sensor::eBaudRate115200)); assert(d.port.baud==115200);
  Sensor::eBaudRate_t baud; assert(s.getBaudRate(baud) && baud==Sensor::eBaudRate115200);
  assert(s.update());
  d.failBaud=true; assert(!s.setBaudRate(Sensor::eBaudRate460800)); assert(!s.update());
  assert(mem.before==0x12345678 && mem.after==0x87654321);
  // Tiny user storage: begin may discard a big active frame, but update rejects it.
  Device small; struct { Sensor::sPoint_t p[1]; uint32_t guard; } tiny;
  tiny.guard=0xcafebabe; Sensor t(&small.port); assert(t.setFrameBuffer(tiny.p,1));
  small.port.push(frame(48,32,2)); assert(t.begin()); assert(!t.update());
  assert(tiny.guard==0xcafebabe); assert(t.setMeasureMode(0,0)); assert(t.update());
  assert(tiny.guard==0xcafebabe); checkPoints(t);
  puts("PASS: presets, peaks, all output modes, XYZ, CRC, framing, nested recovery, late frames, capacity, baud and SAVE");
}
