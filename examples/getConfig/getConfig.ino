/*!
 * @file getConfig.ino
 * @brief Query actual configuration and show preset side effects.
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

void printConfig()
{
  DFRobot_TMF8829::sConfig_t c;
  require(sensor.getConfig(c), "CONFIG?");
  Serial.print("mode(enum)=");
  Serial.println(int(c.mode));
  Serial.print("peakCount=");
  Serial.println(c.peakCount);
  Serial.print("reportMode=");
  Serial.println(c.reportMode);
  Serial.print("outputMode(enum)=");
  Serial.println(int(c.outputMode));
  Serial.print("x=");
  Serial.print(c.x);
  Serial.print(" y=");
  Serial.println(c.y);
  Serial.print("baudRate=");
  Serial.println(uint32_t(c.baudRate));
}

void setup()
{
  Serial.begin(115200);
  require(sensor.begin(), "begin: check wiring and current device baud");
  printConfig();
  // Max peaks: 8/16 presets=4, 32 presets=3, 48 presets=2 (including H).
  require(sensor.setSensorMode(DFRobot_TMF8829::eMode8X8), "8X8");
  require(sensor.setPeakCount(4), "8X8 Peak=4");
  require(sensor.setMeasureMode(3, 4), "POINT");
  printConfig();
  require(sensor.setSensorMode(DFRobot_TMF8829::eMode32X32), "32X32");
  // A different preset resets Peak to 1 and selection to FRAME. Same preset preserves them.
  printConfig();
  require(sensor.setPeakCount(3), "32X32 Peak=3");
  printConfig();
}

void loop()
{
  printConfig();
  delay(3000);
}
