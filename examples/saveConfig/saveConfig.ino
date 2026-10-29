/*!
 * @file saveConfig.ino
 * @brief Explicit one-time NVS save followed by request/response measurements.
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

DFRobot_TMF8829 tmf8829(&Serial1, DFRobot_TMF8829::eBaudRate115200, /*RX pin*/25, /*TX pin*/26);

void require(bool ok, const char *step)
{
  if (ok) return;
  Serial.print("Failed: ");
  Serial.println(step);
  while (true) delay(1000);
}

void setup()
{
  Serial.begin(115200);
  require(tmf8829.begin(), "begin: check wiring and current device baud");
  require(tmf8829.setSensorMode(DFRobot_TMF8829::eMode8X8), "8X8");
  require(tmf8829.setPeakCount(1), "Peak=1");
  require(tmf8829.setMeasureMode(), "FRAME");
  require(tmf8829.saveConfig(), "SAVE"); // Only once in setup, never repeatedly in loop.
  Serial.println("Saved: restored at next power-on; no restart is needed now.");
}

void loop()
{
  if (tmf8829.update()) {
    const DFRobot_TMF8829::sFrame_t &f = tmf8829.getAllPointData();
    Serial.print("frameId=");
    Serial.print(f.frameId);
    Serial.print(" slots=");
    Serial.println(f.pointCount);
  } else Serial.println("Frame unavailable");
  delay(1000);
}
