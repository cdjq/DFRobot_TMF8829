# DFRobot_TMF8829

- [English Version](./README.md)

面向 TMF8829 点云传感器模块的 Arduino 库，通过硬件串口以问答方式读取 XYZ 点云。提供分辨率预设、Peak 数量、点/行/全帧输出及显式配置保存，主控通过 UART 连接传感器。

## 产品链接 ([https://www.dfrobot.com.cn](https://www.dfrobot.com.cn))

```
SKU: SEN0773
```

## 目录

* [概述](#概述)
* [库安装](#库安装)
* [方法](#方法)
* [UART 数据帧格式](#uart-数据帧格式)
* [兼容性](#兼容性)
* [历史](#历史)
* [创作者](#创作者)

## 概述

使用硬件 UART（8N1），主控以问答方式读取 XYZ 点云。库构造函数默认波特率为 **921600**，固件在没有有效保存配置时的上电默认波特率为 **921600**。六份例程统一显式使用 `eBaudRate921600`；若模块已保存其他速率，请修改例程中的构造参数，与模块当前速率一致。

ESP32 例程使用 `Serial1`，主控 RX 为 **GPIO25**、TX 为 **GPIO26**。模块 TX 接 GPIO25，模块 RX 接 GPIO26，VCC 接 **3.3V**，GND 共地；`Serial` 用于打印结果。

先调用 `begin()`，再按用途设置分辨率、Peak 数量和点/行/全帧输出。调用 `update()` 获取并校验新数据后，通过 `getAllPointData()` 或 `getPointData()` 读取缓存。枚举和结构体在类外定义，例如 `eMode48X32`、`eBaudRate921600` 和 `sPoint_t`，无需类名前缀。库自动管理点数组，切换模式无需修改数组大小。

`getAllPointData` 例程以毫秒打印请求前准备、请求到首字节、接收与解析、`update()` 总耗时及计时报告输出耗时。`getTiming()` 返回最近一次调用的 `sTiming_t` 微秒计时，失败请求也会记录。首字节时间以主控读到字节为准。报告输出耗时包含 `Serial.flush()`，不包含最后的耗时汇总行及点数据表格。

六份例程可按用途独立选择，无需按顺序运行：

* [getAllPointData](./examples/getAllPointData/getAllPointData.ino)：读取并打印整帧点云。
* [getPointData](./examples/getPointData/getPointData.ino)：通过顶部宏选择单点，读取其全部 Peak。
* [getLineData](./examples/getLineData/getLineData.ino)：通过顶部宏选择单行。
* [getConfig](./examples/getConfig/getConfig.ino)：查询实际配置，展示切换预设的影响。
* [saveConfig](./examples/saveConfig/saveConfig.ino)：在 setup 中保存一次配置到 NVS。
* [setBaudRate](./examples/setBaudRate/setBaudRate.ino)：从 921600 切换到 115200，并验证模块和主控速率。

## 库安装

* 将本库放入 Arduino 的 libraries/DFRobot_TMF8829，重新打开 IDE 后选择 examples 中的示例。
* 若将来发布到库管理器，可搜索 DFRobot_TMF8829 安装；当前不声明已上架。

## 方法

### 公开接口

以下为全部公开接口，类型定义见头文件。

```C++
  /**
   * @fn DFRobot_TMF8829
   * @brief 保存硬件串口及初始通信参数，构造时不通信。
   * @param serial 外部提供的硬件串口对象，不能为空；库不负责销毁。
   * @param baudRate 模块当前波特率，使用 eBaudRate_t 中的八种枚举值。
   * @param rxPin ESP32 接收引脚，-1 使用默认引脚。
   * @param txPin ESP32 发送引脚，-1 使用默认引脚。其他平台忽略引脚参数。
   */
  DFRobot_TMF8829(HardwareSerial *serial, eBaudRate_t baudRate = eBaudRate921600, int8_t rxPin = -1, int8_t txPin = -1);

  /**
   * @fn ~DFRobot_TMF8829
   * @brief 释放库自动分配的点缓存；串口由调用者管理。
   */
  ~DFRobot_TMF8829();

  /**
   * @fn setFrameBuffer
   * @brief 可选：用调用者的点数组替换自动缓存，须在空闲时调用。
   * @param buffer 非空数组，使用期间必须保持有效。
   * @param capacity 点槽位数量，不是像素数或字节数，必须大于零。
   * @n 清除帧有效状态并释放此前的内部缓存；不释放用户数组。
   * @return 绑定成功返回 true。
   */
  bool setFrameBuffer(sPoint_t *buffer, size_t capacity);

  /**
   * @fn begin
   * @brief 启动串口，强制 REPORT=0，检查 AT 并查询实际配置。
   * @n 未提供用户数组时自动分配 3072 个点；内存不足时返回 false。
   * @n 不修改分辨率、空间选择或 NVS；允许使用小缓存。
   * @return 问答模式及全部配置确认成功返回 true。
   */
  bool begin(void);

  /**
   * @fn getConfig
   * @brief 查询并校验全部实际配置。
   * @param config 仅在完全成功时更新。
   * @n 字段顺序任意，忽略未知字段；失败时配置缓存失效。
   * @return 全部字段有效且收到最终 OK 时返回 true。
   */
  bool getConfig(sConfig_t &config);

  /**
   * @fn setSensorMode
   * @brief 设置分辨率预设，并重新查询实际配置。
   * @param mode 普通预设 eMode8X8、eMode16X16、eMode32X32、eMode48X32。
   * @n H 系列预设 eModeH8X8、eModeH16X16、eModeH32X32、eModeH48X32；L 系列预设 eModeL8X8。
   * @n 切换到不同预设会将 Peak 重置为 1、输出重置为 FRAME；相同预设保持原设置。
   * @n 清除帧有效状态，不自动保存。
   * @return 命令及配置核对成功返回 true。
   */
  bool setSensorMode(eMeasurementMode_t mode);

  /**
   * @fn setPeakCount
   * @brief 设置每个像素的槽位数，不自动限幅。
   * @param peakCount 8/16 预设为 1..4，32 预设为 1..3，48 预设为 1..2。
   * @n 按实际预设检查参数；清除帧有效状态，不自动保存。
   * @return 命令及配置核对成功返回 true。
   */
  bool setPeakCount(uint8_t peakCount);

  /**
   * @fn setMeasureMode
   * @brief 选择原始画面中的一个像素，以 POINT 输出。
   * @param x 原始列坐标，从零开始，小于预设宽度。
   * @param y 原始行坐标，从零开始，小于预设高度。
   * @n 输出为 1x1，包含全部 Peak；缓存坐标为 (0,0)，XYZ 不作平移。
   * @n 清除帧有效状态，不自动保存。
   * @return 命令及配置核对成功返回 true。
   */
  bool setMeasureMode(uint8_t x, uint8_t y);

  /**
   * @fn setMeasureMode
   * @brief 选择原始画面中的一行，以 LINE 输出。
   * @param y 原始行坐标，从零开始，小于预设高度。
   * @n 输出为预设宽度 x 1，包含全部 Peak；缓存 y=0，XYZ 不作平移。
   * @n 清除帧有效状态，不自动保存。
   * @return 命令及配置核对成功返回 true。
   */
  bool setMeasureMode(uint8_t y);

  /**
   * @fn setMeasureMode
   * @brief 恢复当前分辨率的 FRAME 全帧输出。
   * @n 清除帧有效状态；不改变预设或 Peak，不自动保存。
   * @return 命令及配置核对成功返回 true。
   */
  bool setMeasureMode(void);

  /**
   * @fn getBaudRate
   * @brief 发送 BAUD? 并校验设备返回的波特率。
   * @param baudRate 仅在查询完整且收到最终 OK 时更新。
   * @n 仅接受 eBaudRate_t 中的八种速率，同时更新缓存速率。
   * @return 完整查询成功返回 true。
   */
  bool getBaudRate(eBaudRate_t &baudRate);

  /**
   * @fn setBaudRate
   * @brief 没有待处理取帧请求时，同步修改模块和主控的波特率。
   * @param baudRate eBaudRate_t 中的任意一种速率。
   * @n 在旧速率收到完整 OK 后切换主控，再用 AT 和 BAUD? 验证。
   * @n 立即生效，无需重启，不自动保存；失败可能造成两端速率不同步。
   * @n 按设备已知速率重新创建对象并 begin 恢复，不自动扫描波特率。
   * @return 确认两端均工作在目标速率时返回 true。
   */
  bool setBaudRate(eBaudRate_t baudRate);

  /**
   * @fn saveConfig
   * @brief 显式将当前全部配置保存到 NVS。
   * @n 无需重启；确有需要时在 setup 中调用一次，不要在 loop 中反复保存。
   * @n 下次上电时，主控应使用已保存的波特率。
   * @return 收到最终 OK 返回 true。
   */
  bool saveConfig(void);

  /**
   * @fn update
   * @brief 发送一次 GETFRAME，等待完整有效的响应。
   * @param timeoutMs 响应超时，单位毫秒，默认 5000。
   * @n 要求 begin 成功且 REPORT=0；恢复旧请求最多额外等待 5000 ms。
   * @n 调用时清除 valid/isNew；失败时 pointCount=0，不保留旧数组内容。
   * @n 长度、几何尺寸、Peak、容量、CRC 和帧尾全部通过后才发布缓存。
   * @return 完整响应成功返回 true，重复 frameId 也属于成功。
   */
  bool update(uint32_t timeoutMs = 5000);

  /**
   * @fn getAllPointData
   * @brief 返回最近一次成功 update 写入的缓存引用，不通信。
   * @n 先调用 update 并检查 valid；索引为 (y * width + x) * peakCount + peak。
   * @n 保留 confidence 为零的槽位；引用及指针仅在下次更新、配置或缓存变更、对象销毁前有效。
   * @return 帧元数据及借用的点数组；valid 表示协议有效，不代表每个点的质量。
   */
  const sFrame_t &getAllPointData(void) const;

  /**
   * @fn getPointData
   * @brief 从有效缓存复制一个点，不通信。
   * @param x 输出区域的局部列坐标，从零开始。
   * @param y 输出区域的局部行坐标；POINT 和 LINE 必须为零。
   * @param peak Peak 索引，从零开始，小于缓存的 peakCount。
   * @param point 失败时保持原值。
   * @n 先调用 update；POINT 使用 (0,0)，不是原画面中所选像素的坐标。
   * @return 缓存有效且所有索引均合法时返回 true。
   */
  bool getPointData(uint8_t x, uint8_t y, uint8_t peak, sPoint_t &point) const;

  /**
   * @fn isDataNew
   * @brief 读取缓存的新帧标志，不清除标志。
   * @n 首次成功为新帧；以后比较 ID 是否不同，允许计数回绕。
   * @return 缓存的 isNew；更新失败或缓存失效后为 false。
   */
  bool isDataNew(void) const;

  /**
   * @fn isDataValid
   * @brief 读取缓存的整帧协议校验结果。
   * @n 先调用 update；单点 confidence/signal 不决定协议是否有效。
   * @return 当前缓存为完整有效帧时返回 true。
   */
  bool isDataValid(void) const;

```

### 上电默认配置

Flash 中没有有效保存配置时，模块上电使用以下默认值；存在有效保存配置时，恢复已保存的配置。

| 配置项 | 默认值 |
| --- | --- |
| 测量模式 | `48X32`，普通模式 |
| 测量周期 | `66 ms`，约 15 帧/秒 |
| 峰值数量 | `1` |
| 输出范围 | `FRAME`，整帧 |
| 自动上报 | 关闭，即 `REPORT=0` |
| 信号强度数据 | 开启 |
| 噪声数据 | 关闭 |
| 串扰数据 | 关闭 |
| UART 波特率 | `921600` |
| UART 格式 | 8 位数据、无校验、1 位停止位（8N1） |

库构造函数的默认波特率为 `921600`，与上述模块上电默认配置一致。若模块已保存其他波特率，应显式传入对应速率。

### 指令格式与功能

模块通过 UART 接收区分大小写的 ASCII 指令，串口格式为 8N1，无硬件流控。指令及参数中不加空格，每条指令以 `\r\n`（CRLF）结束，库会自动追加；单条指令长度必须小于 128 字节。逐条发送指令，完整接收当前回复后再发送下一条。

普通指令执行成功返回 `\r\nOK\r\n`；未知指令、参数无效或执行失败返回 `\r\nERROR\r\n`。查询指令先返回字段行，最后返回 `OK`。`AT+GETFRAME` 成功时直接返回二进制帧，末尾不附加 `OK`。

| 指令 | 参数与功能 | 对应库接口 |
| --- | --- | --- |
| `AT` | 通信检查，成功返回 `OK`。 | `begin()` 内部使用 |
| `AT+CONFIG?` | 查询 MODE、PEAK、REPORT、SELECT、X、Y、BAUD。 | `getConfig()` |
| `AT+MODE=<mode>` | 设置测量预设，取值见下表。 | `setSensorMode()` |
| `AT+PEAK=<count>` | 每像素回波峰数量，从 1 开始，上限由测量预设决定。 | `setPeakCount()` |
| `AT+REPORT=<mode>` | `0` 为问答模式，`1` 为固件主动上报模式；两种模式都不停止传感器测量。 | `begin()` 强制设为 `0`，本库仅支持问答模式 |
| `AT+GETFRAME` | 问答模式下请求下一完整 XYZ 帧；已有未完成请求或处于主动模式时返回 `ERROR`。 | `update()` |
| `AT+POINT=<x>,<y>` | 选择原画面中从 0 开始的列、行，输出 1×1 像素及其全部 Peak；越界返回 `ERROR`。 | `setMeasureMode(x, y)` |
| `AT+LINE=<y>` | 选择原画面中从 0 开始的行，输出当前预设宽度 × 1 及其全部 Peak；越界返回 `ERROR`。 | `setMeasureMode(y)` |
| `AT+FRAME` | 恢复当前预设的全画面输出，保留 Peak 数量。 | `setMeasureMode()` |
| `AT+BAUD?` | 返回 `+BAUD:<速率>`，随后返回 `OK`。 | `getBaudRate()` |
| `AT+BAUD=<rate>` | 设置 UART 波特率，必须使用下文列出的整数值。 | `setBaudRate()` |
| `AT+SAVE` | 将 MODE、PEAK、REPORT、SELECT、X、Y、BAUD 保存到 NVS，下次上电恢复。 | `saveConfig()` |

### 测量预设与 Peak 上限

| `<mode>` | 宽 × 高 | `<count>` 上限 |
| --- | --- | --- |
| `8X8` | 8×8 | 4 |
| `16X16` | 16×16 | 4 |
| `32X32` | 32×32 | 3 |
| `48X32` | 48×32 | 2 |
| `H_8X8` | 8×8 | 4 |
| `H_16X16` | 16×16 | 4 |
| `H_32X32` | 32×32 | 3 |
| `H_48X32` | 48×32 | 2 |
| `L_8X8` | 8×8 | 4 |

成功切换到不同预设后，固件将 PEAK 重置为 1，并恢复 FRAME 输出；重复设置当前预设不会执行这组重置。因此应先设置预设，再设置 Peak 数量及点/行选择。

### 配置查询示例

发送 `AT+CONFIG?\r\n`，回复示例如下。每行以 CRLF 结束，字段顺序可能不同：

```text
+CONFIG:MODE=48X32
+CONFIG:PEAK=1
+CONFIG:REPORT=0
+CONFIG:SELECT=FRAME
+CONFIG:X=-1
+CONFIG:Y=-1
+CONFIG:BAUD=921600
OK
```

SELECT 取值为 `FRAME`、`LINE` 或 `POINT`。FRAME 时 X/Y 均为 -1；LINE 时 X=-1、Y 为所选行号；POINT 时 X/Y 为所选原画面列、行索引。POINT/LINE 不平移 XYZ 空间坐标，二进制帧中也不携带原画面的选择索引。

### 取帧、切换波特率与保存配置

例如依次发送以下指令，每条追加 CRLF，并等待当前回复完整接收后再继续：

```text
AT
AT+REPORT=0
AT+MODE=48X32
AT+PEAK=2
AT+FRAME
AT+GETFRAME
```

前五条指令返回 `OK`，GETFRAME 返回以 `AA 55` 开头、以 `55 AA` 结尾的 XYZ 二进制帧。每点共 9 字节，依次为小端 int16 x/y/z（毫米）、uint16 signal 和 uint8 confidence。使用本库时通过 `update()` 接收并校验帧，再通过 getter 读取缓存。当前取帧事务结束前不要发送下一条指令，也不要同时手动读取库使用的 UART。

支持的波特率为 `115200`、`460800`、`921600`、`1000000`、`2000000`、`3000000`、`4000000`、`5000000`，不接受 `1M` 等缩写。库构造函数默认值为 **921600**，所参考固件在无保存配置时默认为 **921600**。构造时应传入模块实际使用的速率，库不会自动扫描波特率。

手动切换速率时，先完成未结束的取帧事务，发送 `AT+REPORT=0` 并等待 `OK`，再以旧速率发送如 `AT+BAUD=921600`。完整接收旧速率下的 `OK` 后，将主机 UART 切到 921600，再用 `AT`、`AT+BAUD?` 验证。本库的 `setBaudRate()` 会完成两端速率切换及验证。

普通设置生效时不会自动写入 NVS，需要断电保存时显式发送 `AT+SAVE`；当前配置无需重启即可生效。保存新波特率后，下次上电应使用该速率连接，避免在 `loop()` 中反复保存。

### 数据与内存

`sPoint_t` 保存 int16_t 的 x/y/z（毫米）、uint16_t 的 signal 和 uint8_t 的 confidence。`sFrame_t` 包含 width、height、peakCount、frameId、pointCount、points、isNew、valid。`sConfig_t` 包含 mode、peakCount、reportMode、outputMode、x、y、baudRate。完整枚举及字段注释见[头文件](./src/DFRobot_TMF8829.h)。

索引顺序为 `(y * width + x) * peakCount + peak`。POINT 使用缓存局部 (0,0)，LINE 的局部 y=0；选择原画面坐标不会平移 XYZ。valid 表示协议完整合法，每个点的质量需另看 confidence/signal。重复 frameId 成功但 isNew=false，首次成功及 ID 变化（包括回绕）时为新帧。失败后 valid/isNew=false、pointCount=0。

| 输出 | 槽位数 | sizeof(sPoint_t)=10 时的数组 RAM |
| --- | ---: | ---: |
| 8x8，Peak=1 | 64 | 640 B |
| 48x32，Peak=2 / 32x32，Peak=3 | 3072 | 30720 B |
| POINT，Peak=4 | 4 | 40 B |
| LINE 8x1，Peak=4 | 32 | 320 B |

默认由 begin() 一次分配 3072 个点，覆盖所有模式及合法 Peak 数，切换模式时复用。用户无需声明 points 数组或调用 setFrameBuffer()。sizeof(sPoint_t)=10 时占 30,720 字节（30 KiB），另加对象及分配器开销；内存不足时 begin() 返回 false，析构时释放内部缓存。update() 不分配内存，线格式在同一缓存内反向展开，不额外分配整帧。

上表表示使用可选 setFrameBuffer() 接口时各输出的最低容量。此接口供需要自行管理内存的用户使用，会释放此前的内部缓存，但不会释放用户数组。用户缓存不足时，超容量帧被完整丢弃并返回 false。下次 update、配置变更、替换缓存或析构会使旧数据引用失效；失败时数据也可能已被改写。

普通 AT 超时 1000 ms，模式/SAVE 为 5000 ms，update 默认 5000 ms。处理旧事务可能额外等待最多 5000 ms。取帧超时后，在旧响应完整到达或收到 ERROR 前不会发送新 GETFRAME；连接无法恢复时，检查线路并按设备已知波特率重新初始化，不自动扫描速率。

setBaudRate 等待旧速率完整 OK，再切换主控并验证 AT/BAUD?。失败可能失步，按设备已知速率重新创建对象并 begin。配置只在显式 saveConfig 时写 NVS，不在 loop 中反复保存；保存新波特率后，下次上电应使用新速率构造。

## UART 数据帧格式

`AT+GETFRAME\r\n` 成功返回二进制 XYZ 点云帧。串口使用 **8N1**；以下偏移从帧头第一个字节开始，以 0 为起点。所有多字节整数均为**小端序（低字节先传）**，二进制帧不以 `\r\n` 结束；普通 AT 文本回复仍按行解析。

设 `W` 为输出宽度、`H` 为输出高度、`P` 为每像素 Peak 数，`D = W × H × P × 9` 为点数据字节数。

| 字节偏移 | 字段 | 长度（字节） | 类型 / 内容 |
|---|---|---:|---|
| 0 | 帧头 | 2 | 固定 `AA 55` |
| 2 | 长度 L | 2 | `uint16_t`，`L = D + 9`；从本长度字段开始，至点数据末尾，共 L 字节 |
| 4 | 输出宽度 W | 1 | `uint8_t` |
| 5 | 输出高度 H | 1 | `uint8_t` |
| 6 | 帧编号 | 4 | `uint32_t`，对应 `sFrame_t::frameId` |
| 10 | 每像素 Peak 数 P | 1 | `uint8_t` |
| 11 | 点数据 | D | 每个 Peak 槽位占 9 字节，见下表 |
| 11 + D | CRC16 | 2 | 小端序 CRC-16/MODBUS |
| 13 + D | 帧尾 | 2 | 固定 `55 AA` |

**单个 Peak 槽位（9 字节）**：

| 槽内偏移 | 字段 | 长度（字节） | 类型 / 单位 |
|---|---|---:|---|
| 0 | X | 2 | `int16_t`，mm |
| 2 | Y | 2 | `int16_t`，mm |
| 4 | Z | 2 | `int16_t`，mm |
| 6 | Signal | 2 | `uint16_t`，信号强度 |
| 8 | Confidence | 1 | `uint8_t`，原始置信度 |

像素按行排列，同一个像素的 P 个 Peak 连续存储。点数组索引为 `((y × W) + x) × P + peak`，坐标及 Peak 索引均从 0 开始；单行和单点输出的数组坐标为输出范围内的局部索引。

- 全帧输出：W、H 为当前模式分辨率。
- 单行输出：W 为当前模式宽度，H 为 1。
- 单点输出：W、H 均为 1；仍包含配置的 P 个 Peak 槽位。

**CRC 规则**：初始值 `0xFFFF`，反射多项式 `0xA001`，逐字节异或后右移处理，无最终异或。校验范围为偏移 2 至 `10 + D`（包含长度字段、尺寸、帧编号、Peak 数和全部点数据），不含帧头、CRC 本身和帧尾。CRC 低字节先发送；标准校验串 `123456789` 的结果为 `0x4B37`。

```text
完整帧字节数 = D + 15 = L + 6
48×32、Peak=1：D = 13824，L = 13833，完整帧 = 13839 字节
UART 理论传输时间(ms) = 完整帧字节数 × 10 × 1000 ÷ 波特率
```

例如，单点输出、Peak=1、帧编号=1，X=-7 mm、Y=10 mm、Z=31 mm、Signal=36102、Confidence=183：

```text
AA 55 12 00 01 01 01 00 00 00 01 F9 FF 0A 00 1F 00 06 8D B7 5E B8 55 AA
```

其中长度字段为 `12 00`（18），完整帧为 24 字节。线上点数据固定为每槽 9 字节，不应直接使用可能含对齐填充的 `sizeof(sPoint_t)` 计算帧长度。

## 兼容性

| 主板 | 通过 | 未通过 | 未测试 | 备注 |
| ------------ | :--: | :----: | :----: | ---- |
| Mega2560 | | | √ | |
| Leonardo | | | √ | |
| ESP32 | | | √ | |
| ESP8266 | | | √ | |

## 历史

- 2026/09/03 - V1.0.0 版本

## 创作者

Written by PLELES(li.jia@dfrobot.com), 2026. (欢迎访问我们的[网站](https://www.dfrobot.com/))
