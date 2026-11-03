# Arduino 库审核报告

库目录：/workspace/DFRobot_TMF8829

先写规范：名字、注释和库的写法。再写体验：示例是否合理、用户能不能按用途直接用上。示例是并列的，不排先烧哪一份。

## 规范

### `header-layout`

**结论：** 不满足

**reason：** 文件级顺序应为 guard → Arduino.h → 平台头 → 宏 → 枚举 → 结构体 → 类。现状宏在类外正确，但 `eMeasurementMode_t` / `eOutputMode_t` / `eBaudRate_t` 与 `sPoint_t` / `sFrame_t` / `sConfig_t` 均嵌在类的 `public:` 内，未置于类定义之前。

**suggestion：**

```cpp
#ifndef __DFROBOT_TMF8829_H__
#define __DFROBOT_TMF8829_H__

#include <Arduino.h>
#include <HardwareSerial.h>

#define TMF8829_AT_TIMEOUT       1000UL   ///< Ordinary AT deadline in milliseconds
#define TMF8829_CONFIG_TIMEOUT   5000UL   ///< Mode, recovery and NVS deadline in milliseconds
#define TMF8829_MAX_POINTS       3072UL   ///< Maximum legal XYZ slots in supported modes
#define TMF8829_POINT_BYTES      9UL      ///< Bytes per wire-format XYZ slot

/**
 * @enum eMeasurementMode_t
 * @brief Measurement presets; mapped explicitly to firmware strings.
 */
typedef enum {
  eMode8X8 = 0,    /**< 8 by 8, at most 4 peaks */
  eMode16X16,      /**< 16 by 16, at most 4 peaks */
  eMode32X32,      /**< 32 by 32, at most 3 peaks */
  eMode48X32,      /**< 48 by 32, at most 2 peaks */
  eModeH8X8,       /**< H_8X8 preset, at most 4 peaks */
  eModeH16X16,     /**< H_16X16 preset, at most 4 peaks */
  eModeH32X32,     /**< H_32X32 preset, at most 3 peaks */
  eModeH48X32,     /**< H_48X32 preset, at most 2 peaks */
  eModeL8X8        /**< L_8X8 preset, at most 4 peaks */
} eMeasurementMode_t;

/**
 * @enum eOutputMode_t
 * @brief Spatial output selection, independent of measurement preset.
 */
typedef enum {
  eOutputFrame = 0, /**< Full image */
  eOutputPoint = 1, /**< Selected pixel, all peaks */
  eOutputLine = 2   /**< Selected row, all peaks */
} eOutputMode_t;

/**
 * @enum eBaudRate_t
 * @brief Supported UART baud rates, represented by their actual values.
 */
typedef enum : uint32_t {
  eBaudRate115200  = 115200UL,
  eBaudRate460800  = 460800UL,
  eBaudRate921600  = 921600UL,
  eBaudRate1000000 = 1000000UL,
  eBaudRate2000000 = 2000000UL,
  eBaudRate3000000 = 3000000UL,
  eBaudRate4000000 = 4000000UL,
  eBaudRate5000000 = 5000000UL
} eBaudRate_t;

/**
 * @struct sPoint_t
 * @brief One XYZ slot; native sizeof may exceed the nine wire bytes.
 */
typedef struct {
  int16_t  x;
  int16_t  y;
  int16_t  z;
  uint16_t signal;
  uint8_t  confidence;
} sPoint_t;

/**
 * @struct sFrame_t
 * @brief Validated frame metadata and borrowed point storage.
 */
typedef struct {
  uint8_t         width;
  uint8_t         height;
  uint8_t         peakCount;
  uint32_t        frameId;
  uint16_t        pointCount;
  const sPoint_t *points;
  bool            isNew;
  bool            valid;
} sFrame_t;

/**
 * @struct sConfig_t
 * @brief Actual firmware configuration returned by CONFIG?.
 */
typedef struct {
  eMeasurementMode_t mode;
  uint8_t            peakCount;
  uint8_t            reportMode;
  eOutputMode_t      outputMode;
  int16_t            x;
  int16_t            y;
  eBaudRate_t        baudRate;
} sConfig_t;

class DFRobot_TMF8829 {
public:
  DFRobot_TMF8829(HardwareSerial *serial, eBaudRate_t baudRate = eBaudRate115200, int8_t rxPin = -1, int8_t txPin = -1);
  // ... remaining public API unchanged ...
protected:
  bool openSerial(void);
  bool send(const char *command);
  bool transact(const char *command, uint32_t timeoutMs, uint8_t query = 0);
  bool prepare(void);
  bool ensureConfig(void);
  bool change(const char *command, uint32_t timeoutMs = TMF8829_AT_TIMEOUT);
  bool poll(uint8_t &event);
  bool feed(uint8_t value, uint8_t &event);
  bool inspect(uint8_t &event);
  void resync(void);
  uint8_t wireByte(uint32_t index) const;
  void resetParser(void);
  void invalidate(void);
  bool parseField(char *line);
  bool validConfig(const sConfig_t &config) const;
  static bool dimensions(eMeasurementMode_t mode, uint8_t &width, uint8_t &height, uint8_t &maxPeak);
  static const char *modeName(eMeasurementMode_t mode);
  static bool validBaud(uint32_t baud);
  static bool number(const char *text, long &value);
  static uint16_t crcByte(uint16_t crc, uint8_t value);
  bool frameMatches(void) const;
  void publish(void);
private:
  HardwareSerial * _pSerial;
  sPoint_t *       _pBuffer;
  size_t           _capacity;
  bool             _ownsBuffer;
  eBaudRate_t      _baudRate;
  int8_t           _rxPin;
  int8_t           _txPin;
  // ... remaining private state unchanged ...
};

#endif
```

（若枚举/结构体改到类外，示例与 README 中的 `DFRobot_TMF8829::eMode48X32` 等写法可改为全局 `eMode48X32`，或保留 `using` / 类型别名；方法签名与 KEYWORD/LITERAL 只用上述正式类型名。）

### `class-sections`

**结论：** 不满足

**reason：** 协议与内部读写（`openSerial`/`send`/`transact`/`prepare`/`poll`/`feed`/`inspect`/`resync`/`wireByte`/`resetParser`/`parseField`/`publish` 等）应在 `protected`；当前全部在 `private`。总线指针与缓存 `_pSerial`/`_pBuffer`/`_frame`/`_config` 等放在 `private` 正确。

**suggestion：**

```cpp
protected:
  bool openSerial(void);
  bool send(const char *command);
  bool transact(const char *command, uint32_t timeoutMs, uint8_t query = 0);
  bool prepare(void);
  bool ensureConfig(void);
  bool change(const char *command, uint32_t timeoutMs = TMF8829_AT_TIMEOUT);
  bool poll(uint8_t &event);
  bool feed(uint8_t value, uint8_t &event);
  bool inspect(uint8_t &event);
  void resync(void);
  uint8_t wireByte(uint32_t index) const;
  void resetParser(void);
  void invalidate(void);
  bool parseField(char *line);
  bool validConfig(const sConfig_t &config) const;
  static bool dimensions(eMeasurementMode_t mode, uint8_t &width, uint8_t &height, uint8_t &maxPeak);
  static const char *modeName(eMeasurementMode_t mode);
  static bool validBaud(uint32_t baud);
  static bool number(const char *text, long &value);
  static uint16_t crcByte(uint16_t crc, uint8_t value);
  bool frameMatches(void) const;
  void publish(void);

private:
  HardwareSerial * _pSerial;
  sPoint_t *       _pBuffer;
  size_t           _capacity;
  bool             _ownsBuffer;
  eBaudRate_t      _baudRate;
  int8_t           _rxPin;
  int8_t           _txPin;
  bool             _initialized;
  bool             _configValid;
  bool             _pendingFrame;
  bool             _dirty;
  bool             _haveFrameId;
  uint32_t         _lastFrameId;
  sFrame_t         _frame;
  sConfig_t        _config;
  sConfig_t        _queryConfig;
  uint8_t          _queryMask;
  bool             _queryBad;
  eBaudRate_t      _queryBaud;
  bool             _baudFound;
  uint8_t          _query;
  uint8_t          _prefix[15];
  uint32_t         _wireCount;
  uint32_t         _wireTotal;
  bool             _store;
  uint16_t         _wireCRC;
  uint8_t          _end[4];
  char             _line[128];
  uint8_t          _lineLength;
  bool             _lineOverflow;
```

### `constructor-bus`

**结论：** 不满足

**reason：** 规范要求串口同时提供 Soft + Hard。当前仅有 `HardwareSerial *` 构造，无 `SoftwareSerial`（或等价软串）构造/重载。

**suggestion：**

```cpp
#include <SoftwareSerial.h>

/**
 * @fn DFRobot_TMF8829
 * @brief Hardware UART constructor
 * @param serial Borrowed HardwareSerial; null makes begin fail
 * @param baudRate Current module rate (eBaudRate_t)
 * @param rxPin ESP32 host RX pin; -1 uses board default
 * @param txPin ESP32 host TX pin; -1 uses board default
 * @n Construction performs no communication and does not own the UART
 */
DFRobot_TMF8829(HardwareSerial *serial, eBaudRate_t baudRate = eBaudRate115200, int8_t rxPin = -1, int8_t txPin = -1);

/**
 * @fn DFRobot_TMF8829
 * @brief Software UART constructor
 * @param serial Borrowed SoftwareSerial; null makes begin fail
 * @param baudRate Current module rate (eBaudRate_t)
 * @n Construction performs no communication and does not own the UART
 * @n Soft serial may not sustain high baud rates such as 921600 on all MCUs
 */
DFRobot_TMF8829(SoftwareSerial *serial, eBaudRate_t baudRate = eBaudRate115200);
```

### `readme-usable`

**结论：** 不满足

**reason：**  
1. 固定章节外多了禁止的「接线 / Wiring」「示例 / Examples」，以及非固定的「AT 指令」「数据与内存」。  
2. 「产品链接」仅为占位（SKU/链接待确认），未给出可用产品信息。  
3. 中文章节骨架含概述/库安装/方法/兼容性/历史/创作者，兼容性表头正确；安装两条基本具备。  
4. 目录链接文字须与删除多余章后的 `##` 标题互指。

**suggestion：**

```markdown
## Product Link

SKU: （填写确认后的 SKU）

- Product page: https://www.dfrobot.com/（填写确认后的商品链接）

## Table of Contents

* [Summary](#summary)
* [Installation](#installation)
* [Methods](#methods)
* [Compatibility](#compatibility)
* [History](#history)
* [Credits](#credits)

## Summary

Arduino driver for the TMF8829 point-cloud module over hardware UART (8N1).
Module power-on default baud is often 921600; construct with eBaudRate921600 unless you already changed the rate.
Wire module TX→MCU RX, RX→MCU TX, GND→GND, VCC→3.3V (confirm module levels).
Call begin(), configure mode/peak/output as needed, then update() and read points.
Parallel examples: getAllPointData, getPointData, getLineData, getConfig, saveConfig, setBaudRate.

## Installation

There are two methods:
1. Search for DFRobot_TMF8829 in the Arduino Library Manager and install it.
2. Download the library file, then extract it into the Arduino `libraries` directory.
```

```markdown
## 产品链接

SKU：（填写确认后的 SKU）

- 商品页：https://www.dfrobot.com/（填写确认后的商品链接）

## 目录

* [概述](#概述)
* [库安装](#库安装)
* [方法](#方法)
* [兼容性](#兼容性)
* [历史](#历史)
* [创作者](#创作者)

## 概述

面向 TMF8829 点云模块的 Arduino 库，经硬件 UART（8N1）问答读取 XYZ。
模块上电默认波特率常见为 921600；若未改过速率，构造时请传 eBaudRate921600。
接线：模块 TX→主板 RX，RX→主板 TX，GND→GND，VCC→3.3V（以模块电平为准）。
调用 begin()，按需设置模式/Peak/输出，再 update() 读点。
并列示例：getAllPointData、getPointData、getLineData、getConfig、saveConfig、setBaudRate。

## 库安装

这里有 2 种安装方法：
1. 在 Arduino 库管理器中搜索并安装 DFRobot_TMF8829。
2. 下载库文件，解压到 Arduino 的 `libraries` 目录。
```

（删除 `## Wiring`/`## 接线`、`## Examples`/`## 示例`、`## AT Commands`/`## AT 指令`、`## Data and Memory`/`## 数据与内存` 独立章节；必要说明并入 Overview/概述或 Methods。）

### `readme-methods`

**结论：** 不满足

**reason：** `@fn` 应只写方法名；README 中写成了 `@fn setMeasureMode(uint8_t, uint8_t)` / `@fn setMeasureMode(uint8_t)` / `@fn setMeasureMode(void)`。析构函数块也不完整。

**suggestion：**

```cpp
/**
 * @fn setMeasureMode
 * @brief Select one original-image pixel for POINT output
 * @param x Zero-based original column, less than preset width
 * @param y Zero-based original row, less than preset height
 * @n Output is 1x1 with all peaks. Cache coordinates are (0,0); XYZ is not translated
 * @n Clears frame validity; does not save
 * @return true on command and verification success
 */
bool setMeasureMode(uint8_t x, uint8_t y);

/**
 * @fn setMeasureMode
 * @brief Select one original-image row for LINE output
 * @param y Zero-based original row, less than preset height
 * @n Output is preset width x 1 with all peaks. Cache y is 0; XYZ is not translated
 * @n Clears frame validity; does not save
 * @return true on command and verification success
 */
bool setMeasureMode(uint8_t y);

/**
 * @fn setMeasureMode
 * @brief Restore full FRAME output at the current resolution
 * @n Clears frame validity; does not change preset/peak count or save configuration
 * @return true on command and verification success
 */
bool setMeasureMode(void);

/**
 * @fn ~DFRobot_TMF8829
 * @brief Release internally allocated point storage; the UART remains borrowed
 */
~DFRobot_TMF8829();
```

### `keywords-literal`

**结论：** 不满足

**reason：** LITERAL1 仅有宏，未列出枚举成员（`eMode*`、`eOutput*`、`eBaudRate*`）。

**suggestion：**

```text
# Constants (LITERAL1)
TMF8829_AT_TIMEOUT	LITERAL1
TMF8829_CONFIG_TIMEOUT	LITERAL1
TMF8829_MAX_POINTS	LITERAL1
TMF8829_POINT_BYTES	LITERAL1
eMode8X8	LITERAL1
eMode16X16	LITERAL1
eMode32X32	LITERAL1
eMode48X32	LITERAL1
eModeH8X8	LITERAL1
eModeH16X16	LITERAL1
eModeH32X32	LITERAL1
eModeH48X32	LITERAL1
eModeL8X8	LITERAL1
eOutputFrame	LITERAL1
eOutputPoint	LITERAL1
eOutputLine	LITERAL1
eBaudRate115200	LITERAL1
eBaudRate460800	LITERAL1
eBaudRate921600	LITERAL1
eBaudRate1000000	LITERAL1
eBaudRate2000000	LITERAL1
eBaudRate3000000	LITERAL1
eBaudRate4000000	LITERAL1
eBaudRate5000000	LITERAL1
```

## 体验

### `readme-guide`

**结论：** 不满足

**reason：** 不应保留独立接线/示例章；应在现有「概述 / Summary」中写清波特率默认差异、基本接线与六份并列示例各自用途。

**suggestion：**

```markdown
## Summary

Use hardware UART 8N1. If the module still has factory settings, pass eBaudRate921600 to the constructor (library default 115200 only matches a module already set to 115200).
Connect module TX→MCU RX, RX→MCU TX, VCC and GND; examples on ESP32 use Serial1 pins 25/26.
getAllPointData reads a full frame; getPointData / getLineData select one pixel or row; getConfig prints live settings; saveConfig writes NVS once; setBaudRate changes host and device rates together.
```

```markdown
## 概述

使用硬件 UART、8N1。若模块仍是出厂配置，构造时传入 eBaudRate921600（库默认 115200 仅适用于已改为 115200 的模块）。
接线：模块 TX→主板 RX，RX→主板 TX，以及 VCC/GND；ESP32 示例常用 Serial1 的 25/26。
getAllPointData 读整帧；getPointData / getLineData 选单点或单行；getConfig 查询配置；saveConfig 一次性写入 NVS；setBaudRate 同步修改主机与模块波特率。
```

### `demo-experience`

**结论：** 不满足

**reason：** README 写明模块上电默认波特率常为 921600，但 `getConfig` / `getLineData` / `getPointData` / `saveConfig` / `setBaudRate` 均以 `eBaudRate115200` 构造；仅 `getAllPointData` 使用 `eBaudRate921600`。按文档默认连接时，多数示例如法无法 `begin`。

**suggestion：**

五份示例中的构造统一为（与 `getAllPointData.ino` 一致）：

```cpp
DFRobot_TMF8829 tmf8829(&Serial1, DFRobot_TMF8829::eBaudRate921600, /*RX pin*/25, /*TX pin*/26);
```

`setBaudRate.ino` 在验证切换时仍可再调用 `setBaudRate(eBaudRate115200)`；文件头补充：

```cpp
 * @n 默认按模块上电常见波特率 921600 构造；若模块已被改为其他速率，请改构造参数与之一致
```

## 计数

- 要改：8
- 已符合：21
- 不适用：3
