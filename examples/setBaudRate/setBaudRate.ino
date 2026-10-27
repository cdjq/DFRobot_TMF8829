/*!
 * @file setBaudRate.ino
 * @brief Change host and device UART rates between frame transactions.
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

DFRobot_TMF8829 sensor(&Serial1, DFRobot_TMF8829::eBaudRate115200, /*RX pin*/25, /*TX pin*/26);

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
  require(sensor.begin(), "begin: check wiring and current device baud");
  require(sensor.setSensorMode(DFRobot_TMF8829::eMode8X8), "8X8");
  require(sensor.setPeakCount(1), "Peak=1");
  require(sensor.setMeasureMode(3, 4), "POINT");
  require(sensor.setBaudRate(DFRobot_TMF8829::eBaudRate115200), "BAUD: reconnect at known rate if failed");
  DFRobot_TMF8829::eBaudRate_t rate;
  require(sensor.getBaudRate(rate), "BAUD?");
  Serial.print("Verified baud=");
  Serial.println(uint32_t(rate));
  // Optional: require(sensor.saveConfig(), "SAVE");
  // After saving, use the same baud rate in the constructor on the next power-on.
  // Failure can leave host/device unsynchronized; recreate at the known device rate.
}

void loop()
{
  delay(1000);
}
