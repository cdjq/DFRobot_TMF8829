/*!
 * @file getAllPointData.ino
 * @brief Request and print one complete 48x32 single-peak frame.
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

const DFRobot_TMF8829::eBaudRate_t sensorBaud = DFRobot_TMF8829::eBaudRate115200;
DFRobot_TMF8829 sensor(&Serial1, sensorBaud, /*RX pin*/25, /*TX pin*/26);
// Point storage is managed automatically by the library.

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
  require(sensor.begin(), "begin: check wiring and current device baud");
  require(sensor.setSensorMode(DFRobot_TMF8829::eMode48X32), "48X32");
  require(sensor.setPeakCount(1), "Peak=1");
  require(sensor.setMeasureMode(), "FRAME");
}

void loop()
{
  if (!sensor.update()) {
    Serial.println("Frame unavailable");
    delay(500);
    return;
  }
  const DFRobot_TMF8829::sFrame_t &frame = sensor.getAllPointData();
  Serial.println();
  Serial.print("Frame: ");
  Serial.print(frame.frameId);
  Serial.print("  New: ");
  Serial.println(frame.isNew);
  Serial.println(" Slot |  X(mm) |  Y(mm) |  Z(mm) | Signal | Confidence");
  Serial.println("------+--------+--------+--------+--------+-----------");
  for (uint16_t i = 0; i < frame.pointCount; ++i) {
    printPoint(i, frame.points[i]);
  }
  delay(500);
}
