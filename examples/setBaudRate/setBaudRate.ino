/*!
 * @file setBaudRate.ino
 * @brief Change host and device UART rates between frame transactions.
 * @details ESP32 hardware UART example; see the connection table below.
 * @n Starts at 921600 baud; use the module current saved rate if different.
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

DFRobot_TMF8829 tmf8829(&Serial1, eBaudRate921600, /*RX pin*/25, /*TX pin*/26);

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
  if (!tmf8829.begin()) {
    Serial.println("Sensor init failed, check TX/RX wiring, power supply and baud rate");
    while (true) delay(1000);
  }
  Serial.println("Sensor init Success");
  require(tmf8829.setSensorMode(eMode8X8), "8X8");
  require(tmf8829.setPeakCount(1), "Peak=1");
  require(tmf8829.setMeasureMode(3, 4), "POINT");
  require(tmf8829.setBaudRate(eBaudRate115200), "BAUD: reconnect at known rate if failed");
  eBaudRate_t rate;
  require(tmf8829.getBaudRate(rate), "BAUD?");
  Serial.print("Verified baud=");
  Serial.println(uint32_t(rate));
  // Optional: require(tmf8829.saveConfig(), "SAVE");
  // After saving, use the same baud rate in the constructor on the next power-on.
  // Failure can leave host/device unsynchronized; recreate at the known device rate.
}

void loop()
{
  delay(1000);
}
