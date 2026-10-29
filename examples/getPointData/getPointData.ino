/*!
 * @file getPointData.ino
 * @brief Select a pixel using POINT_X/POINT_Y; read all peaks at local (0,0).
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
// Original pixel coordinates, starting at zero (0-7 in the default 8X8 mode).
#define POINT_X 3
#define POINT_Y 4

#include <DFRobot_TMF8829.h>
#include <stdio.h>

// Peaks per pixel in the default 8X8 mode: 1..4.
const uint8_t PEAK_COUNT = 1;
DFRobot_TMF8829 tmf8829(&Serial1, DFRobot_TMF8829::eBaudRate115200, /*RX pin*/25, /*TX pin*/26);

void require(bool ok, const char *step)
{
  if (ok) return;
  Serial.print("Failed: ");
  Serial.println(step);
  while (true) delay(1000);
}

void printPoint(uint16_t index, const DFRobot_TMF8829::sPoint_t &p)
{
  char row[80];
  snprintf(row, sizeof(row), "%5u | %6d | %6d | %6d | %6u | %10u",
           (unsigned int)index, (int)p.x, (int)p.y, (int)p.z,
           (unsigned int)p.signal, (unsigned int)p.confidence);
  Serial.println(row);
  // confidence=0 is preserved; apply your application's quality threshold here.
}

void setup()
{
  Serial.begin(115200);
  require(tmf8829.begin(), "begin: check wiring and current device baud");
  require(tmf8829.setSensorMode(DFRobot_TMF8829::eMode8X8), "8X8");
  require(tmf8829.setPeakCount(PEAK_COUNT), "setPeakCount");
  require(tmf8829.setMeasureMode(POINT_X, POINT_Y), "POINT selection");
}

void loop()
{
  if (!tmf8829.update()) {
    Serial.println("Frame unavailable");
    delay(500);
    return;
  }
  const DFRobot_TMF8829::sFrame_t &frame = tmf8829.getAllPointData();
  Serial.println();
  Serial.print("Frame: ");
  Serial.print(frame.frameId);
  Serial.print("  Pixel: (");
  Serial.print(POINT_X);
  Serial.print(", ");
  Serial.print(POINT_Y);
  Serial.print(")  Peaks/pixel: ");
  Serial.print(frame.peakCount);
  Serial.print("  Points: ");
  Serial.println(frame.pointCount);
  // Peak is zero-based.
  Serial.println(" Peak |  X(mm) |  Y(mm) |  Z(mm) | Signal | Confidence");
  Serial.println("------+--------+--------+--------+--------+-----------");
  for (uint8_t peak = 0; peak < frame.peakCount; ++peak) {
    DFRobot_TMF8829::sPoint_t point;
    require(tmf8829.getPointData(0, 0, peak, point), "local point");
    printPoint(peak, point);
  }
  delay(500);
}
