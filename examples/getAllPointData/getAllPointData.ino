/*!
 * @file getAllPointData.ino
 * @brief Print a complete frame with pixel numbers and every configured peak.
 * @details ESP32 hardware UART example; see the connection table below.
 * @n Serial prints results; Serial1 connects to the sensor. UART is 8N1.
 * @copyright Copyright (c) 2026 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license The MIT License (MIT)
 * @author [PLELES](li.jia@dfrobot.com)
 * @version V1.0.0
 * @date 2026-10-23
 * @url https://github.com/cdjq/DFRobot_TMF8829
 */
/* ------------------------------------------------------------------------------------
 *   TMF8829   |       MCU        |          ESP32           |   Other Serial1 boards
 *     VCC     |       3.3V       |           3.3V           |           3.3V    
 *     GND     |       GND        |           GND            |           GND
 *      RX     |        TX        |   GPIO26 (Serial1 TX)    |        Serial1 TX
 *      TX     |        RX        |   GPIO25 (Serial1 RX)    |        Serial1 RX
 * ------------------------------------------------------------------------------------ */
// VCC: 3.3V; UART pins use the module logic level.
#include <DFRobot_TMF8829.h>
#include <stdio.h>

// Set to 1 to print every point; set to 0 to print only the frame summary.
const eBaudRate_t sensorBaud = eBaudRate5000000;
// Peaks per pixel: 8/16 presets support 1..4, 32 presets 1..3, 48 presets 1..2.
const uint8_t PEAK_COUNT = 2;
DFRobot_TMF8829 tmf8829(&Serial1, sensorBaud, /*RX pin*/38, /*TX pin*/37);
// Point storage is managed automatically by the library.

void require(bool ok, const char *step)
{
  if (ok) return;
  Serial.print("Failed: ");
  Serial.println(step);
  while (true) delay(1000);
}

void printPoint(uint16_t pixel, uint8_t peak, const sPoint_t &p)
{
  char row[80];
  snprintf(row, sizeof(row), "%5u | %4u | %6d | %6d | %6d | %6u | %10u",
           (unsigned int)pixel, (unsigned int)peak,
           (int)p.x, (int)p.y, (int)p.z,
           (unsigned int)p.signal, (unsigned int)p.confidence);
  Serial.println(row);
  // confidence=0 is preserved; apply your application's quality threshold here.
}

void setup()
{
  Serial.begin(115200);
  if (!tmf8829.begin()) {
    Serial.println("Sensor init failed, check TX/RX wiring, power supply and baud rate");
    while (true) delay(1000);
  }
  Serial.println("Sensor init Success");
  require(tmf8829.setSensorMode(eMode48X32), "48X32");
  require(tmf8829.setPeakCount(PEAK_COUNT), "setPeakCount");
  require(tmf8829.setMeasureMode(), "FRAME");
  delay(1000);
}

void loop()
{
  if (!tmf8829.update()) {
    Serial.println("Frame unavailable");
    delay(500);
    return;
  }
  const sFrame_t &frame = tmf8829.getAllPointData();
  Serial.println();
  Serial.print("Frame: ");
  Serial.print(frame.frameId);
  Serial.print("  New: ");
  Serial.print(frame.isNew);
  Serial.print("  Size: ");
  Serial.print(frame.width);
  Serial.print("x");
  Serial.print(frame.height);
  Serial.print("  Peaks/pixel: ");
  Serial.print(frame.peakCount);
  Serial.print("  Points: ");
  Serial.println(frame.pointCount);
  // Pixel is row-major (row * width + column). Pixel and Peak are zero-based.
  Serial.println("Pixel | Peak |  X(mm) |  Y(mm) |  Z(mm) | Signal | Confidence");
  Serial.println("------+------+--------+--------+--------+--------+-----------");
  const uint16_t pixelCount = uint16_t(frame.width) * frame.height;
  for (uint16_t pixel = 0; pixel < pixelCount; ++pixel) {
    for (uint8_t peak = 0; peak < frame.peakCount; ++peak) {
      const uint16_t index = pixel * frame.peakCount + peak;
      printPoint(pixel, peak, frame.points[index]);
    }
  }
  delay(100);
}
