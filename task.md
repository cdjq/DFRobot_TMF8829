# DFRobot_TMF8829 Arduino 封库需求

## 1. 目标与依据

本次先确定封库需求，后续依据本文实现库；本次不生成 `.h`、`.cpp` 或示例程序。

- 库名、主类名：`DFRobot_TMF8829`；初始版本：`1.0.0`。
- 产品：运行所提供固件的 TMF8829 点云传感器模块，Arduino 主机与模块通信。
- 固件目录：`D:\DFRoboter_JL\firware\DFRobot_TMF8829_FW\DFRobot_TMF8829_FW`。
- 格式参考：`E:\ArduinoProject\libraries\DFRobot_SGX6410`，参考文件组织、Doxygen、命名、缓存读取方式和示例风格。
- 协议依据：固件 `components/at_cmd/at.c`、`components/uart_device/uart_device.c`、`components/usb_device/depth_protocol.h/.c`、`main/tmf8829_app.c`；文档参考 `docs/at_commands.md`、`docs/AT_README.md`、`docs/depth_protocol.md`。
- 旧 `docs/uart_depth.md` 中的 2 Mbps、TOF1 帧及不支持 UART AT 的描述不适用于当前实现；冲突处以上述实际代码为准。
- 不能复制 SGX6410 的产品协议、地址或业务字段；不能把模块内部访问 TMF8829 芯片的 I2C/SPI 当作本库的主机接口。

## 2. 通信接口和类结构

使用一个可直接实例化的 `DFRobot_TMF8829` 类，串口对象由用户注入。主控仅通过 UART 串口接入设备，全部配置指令和点云数据均走同一 UART；不适配 USB 接入，不提供 USB CDC、USB Host 或 USB 接口类，也不建立 I2C/SPI 派生类。

库仅支持问答模式：初始化时设置 `AT+REPORT=0`，之后每次取帧由主机发送 `AT+GETFRAME`。不提供主动模式选项或上报模式切换 API。

| 项目 | 要求 |
| --- | --- |
| UART | 默认 921600 baud，8N1，无硬件流控 |
| 波特率来源 | 保存配置可能改变开机波特率，用户应传入模块当前值；不自动扫描 |
| 接线 | 主机 TX → 模块 RX，主机 RX ← 模块 TX，共地 |
| 固件引脚 | 模块内部 UART TX=GPIO14、RX=GPIO15；不是 Arduino 主机固定引脚 |
| 串口选择 | 使用用户提供的 `HardwareSerial *`，不得硬编码全局 `Serial` |
| ESP32 | 支持用户指定 RX/TX 引脚；均为 -1 时采用板卡串口默认引脚 |
| 其他平台 | 调用其支持的串口初始化接口；不能直接调用 ESP32 专用重载 |

优先以 ESP32 + 硬件串口作为开发、编译及实测基线。其他板卡按硬件串口速率和 RAM 能力声明支持情况，不直接套用 SGX6410 的 SoftwareSerial 分支，也不宣称 UNO 可以运行全部配置。

## 3. 固件协议

### 3.1 AT 指令

命令区分大小写，字段不带空格；库统一追加 `\r\n`，单条命令小于 128 字节。
普通成功回复为 `\r\nOK\r\n`，失败为 `\r\nERROR\r\n`；查询指令先返回字段行，再返回 OK。

| 指令 | 功能及约束 |
| --- | --- |
| `AT` | 通信检查 |
| `AT+CONFIG?` | 查询 MODE、PEAK、REPORT、SELECT、X、Y、BAUD |
| `AT+MODE=<模式字符串>` | 设置测量模式，见 3.2 |
| `AT+PEAK=<数量>` | 每像素 Peak 数，从 1 开始，最大值由模式决定 |
| `AT+REPORT=0` | 问答输出；不停止传感器测量 |
| `AT+GETFRAME` | 仅问答模式有效，返回下一完整二进制帧，成功时没有 OK |
| `AT+POINT=<x>,<y>` | 输出指定像素及其全部 Peak，索引从 0 开始 |
| `AT+LINE=<y>` | 输出指定行及其全部 Peak，行索引从 0 开始 |
| `AT+FRAME` | 输出完整画面 |
| `AT+BAUD?` | 返回 `+BAUD:<数值>`，随后 OK |
| `AT+BAUD=<数值>` | 修改模块 UART 波特率 |
| `AT+SAVE` | 将 MODE、PEAK、REPORT、SELECT、X、Y、BAUD 保存到 NVS |

`AT+GETFRAME` 在已有未完成取帧请求等情况下会返回 ERROR。库只发送正确拼写，不公开固件兼容的错拼命令。若模块此前保存为主动模式，初始化时通过 REPORT=0 切回问答模式，并处理完已开始发送的在途帧；这仅用于初始化恢复，不提供主动取帧功能。

配置查询示例（行结束符为 CRLF）：

```text
+CONFIG:MODE=48X32
+CONFIG:PEAK=1
+CONFIG:REPORT=0
+CONFIG:SELECT=POINT
+CONFIG:X=12
+CONFIG:Y=12
+CONFIG:BAUD=921600
OK
```

配置字段顺序不作假设，可忽略未知字段；要求全部必需字段合法且收到最终 OK，才能更新输出结构体与配置缓存。
SELECT 为 FRAME 时 X/Y 均为 -1；LINE 时 X=-1、Y=行号；POINT 时 X/Y 为原画面像素索引。

无有效 NVS 时，当前固件常规默认配置为 48X32、Peak=1、REPORT=0、FRAME；编译选项可能改变默认分辨率。`begin()` 必须查询真实配置。

### 3.2 测量模式及 Peak 限制

以下周期是固件测量配置，不承诺串口可以达到相同的完整画面输出帧率。

| 模式字符串 | 宽×高 | 周期配置/ms | 最大 Peak |
| --- | --- | --- | --- |
| `8X8` | 8×8 | 33 | 4 |
| `16X16` | 16×16 | 33 | 4 |
| `32X32` | 32×32 | 66 | 3 |
| `48X32` | 48×32 | 66 | 2 |
| `H_8X8` | 8×8 | 66 | 4 |
| `H_16X16` | 16×16 | 66 | 4 |
| `H_32X32` | 32×32 | 66 | 3 |
| `H_48X32` | 48×32 | 66 | 2 |
| `L_8X8` | 8×8 | 66 | 4 |

切换到不同模式成功后，固件将 Peak 重置为 1，并恢复 FRAME 输出。设置相同模式不会执行这组重置。模式切换后重新查询配置，不仅修改主机枚举缓存；失败时也应使不确定的配置缓存失效并允许重新查询。

### 3.3 XYZ 二进制帧

当前主流程调用 `send_pointcloud(..., NULL)`，实际输出每点 9 字节的 XYZ 数据。本期不封装固件通用编码函数中预留的 5 字节原始距离格式，不新增固件没有提供的格式选择指令。

设 `N = width × height × peakCount × 9`。多字节字段均为小端序。

| 偏移/字节 | 长度/字节 | 内容 |
| --- | --- | --- |
| 0 | 2 | 帧头 `AA 55` |
| 2 | 2 | Length，值为 `N + 9` |
| 4 | 1 | width，输出宽度 |
| 5 | 1 | height，输出高度 |
| 6 | 4 | frameId，采集帧序号 |
| 10 | 1 | peakCount，每像素 Peak 数 |
| 11 | N | 逐点 XYZ 数据 |
| 11+N | 2 | CRC16，小端 |
| 13+N | 2 | 帧尾 `55 AA` |

整帧字节数为 `N + 15 = Length + 6`。每个点依次为：

| 点内偏移 | 类型 | 字段 | 含义 |
| --- | --- | --- | --- |
| 0 | int16_t | x | X 坐标，mm |
| 2 | int16_t | y | Y 坐标，mm |
| 4 | int16_t | z | Z 坐标，mm |
| 6 | uint16_t | signal | 固件原始信号值，未启用时可能为 0 |
| 8 | uint8_t | confidence | 固件原始置信度值 |

排列顺序：先行后列，每像素内依次存放各 Peak；数组下标为 `(y * width + x) * peakCount + peak`。
POINT 帧为 1×1，LINE 帧为当前画面宽×1，FRAME 帧为当前画面宽×高。
POINT/LINE 的 XYZ 仍是传感器空间坐标，不进行平移；帧内不包含原画面选中的行列索引，必须通过配置另行获知。

CRC：CRC-16/MODBUS 算法，初值 `0xFFFF`，反射多项式 `0xA001`，无最终异或；覆盖 offset=2 起连续 `N+9` 字节，即 Length 到最后一个点。校验样例：ASCII `123456789` → `0x4B37`。这里只复用 CRC 算法，协议不是 Modbus RTU，不依赖 DFRobot_RTU。

### 3.4 混合流解析及缓存

- AT 回复与 GETFRAME 返回的二进制帧复用串口，必须按当前事务区分文本和二进制；初始化切回问答模式时还需识别并丢弃此前的在途帧。
- 不得在二进制 payload 内搜索 OK/ERROR 或凭帧头截断当前帧，不得发送命令前直接清空 RX 缓冲。
- 校验宽高、Peak、Length、容量、CRC、帧尾后才发布一帧；长度乘法使用至少 32 位的中间值，避免 AVR 的 16 位整数溢出。
- 支持分段到达、粘包、垃圾字节、超时和错帧后恢复同步。错误候选帧应允许从候选起点后继续寻找合法帧，不能永久卡死。
- frameId 是采集序号，可跳号、可回绕，不要求相邻输出差值为 1。
- confidence=0 的槽位仍存在，不删除、不改变数组布局；z=0 或 confidence=0 不等于串口通信失败。
- 不擅自把 signal/confidence 换算成百分比，不内置没有依据的置信度阈值。
- 公共操作采用单线程同步调用，不要求可重入；调用者不得同时从同一串口读取原始数据。

## 4. 数据类型与内存要求

以下类型放在 `DFRobot_TMF8829` 类的 public 区域，采用 `typedef enum`、`typedef struct` 形式；枚举采用 `eXxx_t`，结构体采用 `sXxx_t`。

```cpp
typedef enum {
  eMode8X8 = 0, eMode16X16, eMode32X32, eMode48X32,
  eModeH8X8, eModeH16X16, eModeH32X32, eModeH48X32,
  eModeL8X8
} eMeasurementMode_t;

typedef enum { eOutputFrame = 0, eOutputPoint = 1, eOutputLine = 2 } eOutputMode_t;

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

typedef struct {
  int16_t x;                 // mm
  int16_t y;                 // mm
  int16_t z;                 // mm
  uint16_t signal;           // 原始值
  uint8_t confidence;        // 原始值
} sPoint_t;

typedef struct {
  uint8_t width;             // 当前输出帧的宽度
  uint8_t height;            // 当前输出帧的高度
  uint8_t peakCount;
  uint32_t frameId;
  uint16_t pointCount;       // width * height * peakCount
  const sPoint_t *points;    // 指向调用者提供的缓存，不转移所有权
  bool isNew;
  bool valid;               // 完整帧通过协议校验，不代表每个点均可信
} sFrame_t;

typedef struct {
  eMeasurementMode_t mode;
  uint8_t peakCount;
  uint8_t reportMode;        // CONFIG 返回的 REPORT 原始值，正常工作时必须为 0
  eOutputMode_t outputMode;
  int16_t x;                // 原画面选择索引，无对应维度时为 -1
  int16_t y;
  eBaudRate_t baudRate;
} sConfig_t;
```

测量枚举与模式字符串建立显式映射，不能把枚举序号作为二进制命令值。REPORT 仅作为配置查询字段保留，正常工作要求为 0，不提供对应设置枚举；输出选择枚举对应 FRAME/POINT/LINE 字符串。

波特率枚举的值为实际 baud 数值，显式使用 `uint32_t` 底层类型以容纳全部速率。构造、设置、查询及配置结构体统一使用 `eBaudRate_t`；内部调用串口接口或生成 AT 指令时转换为数值，解析查询结果时先检查是否为枚举支持值再赋值。

默认由库在 `begin()` 中一次分配 3072 个 `sPoint_t`，覆盖全部模式与合法 Peak 数；用户无需声明数组，切换模式无需调整容量。`update()` 不动态分配内存，析构释放内部缓存；分配失败时 `begin()` 返回 false。保留可选 `setFrameBuffer()` 供高级用户提供数组，容量单位为点，用户数组须在使用期间始终有效且库不负责释放。

| 完整画面 | Peak=1 的点数 | Peak=1 的线上帧长/字节 | 最大合法 Peak 下的点数 |
| --- | --- | --- | --- |
| 8×8 | 64 | 591 | 256 |
| 16×16 | 256 | 2319 | 1024 |
| 32×32 | 1024 | 9231 | 3072 |
| 48×32 | 1536 | 13839 | 3072 |

最大合法线上帧为 27663 字节。C++ `sizeof(sPoint_t)` 可能因对齐而大于 9；不能直接将线上数据 memcpy 为结构体数组，内存预算以实际 sizeof 为准。解析额外工作区必须计入 RAM 说明，不能在栈上分配完整最大帧。

允许使用单个库内部或用户提供的点缓存流式解码：接收过程中 `valid=false`，CRC 通过后再提交元数据并置有效。失败时 `pointCount=0`、`valid=false`、`isNew=false`；不承诺保留上一帧的点内容，任何 getter 都不得把未通过 CRC 的部分数据视为有效。若实现采用额外缓存换取原子替换，必须明确额外内存开销。

## 5. 固定 public API

以下为后续实现需遵循的名称和签名。所有 `bool` 操作成功返回 true，失败返回 false；纯缓存 getter 不发送串口命令。

```cpp
DFRobot_TMF8829(HardwareSerial *serial, eBaudRate_t baudRate = eBaudRate115200,
               int8_t rxPin = -1, int8_t txPin = -1);

bool setFrameBuffer(sPoint_t *buffer, size_t capacity);
bool begin(void);
bool getConfig(sConfig_t &config);
bool setSensorMode(eMeasurementMode_t mode);
bool setPeakCount(uint8_t peakCount);
bool setMeasureMode(uint8_t x, uint8_t y);
bool setMeasureMode(uint8_t y);
bool setMeasureMode(void);
bool getBaudRate(eBaudRate_t &baudRate);
bool setBaudRate(eBaudRate_t baudRate);
bool saveConfig(void);

bool update(uint32_t timeoutMs = 5000);
const sFrame_t &getAllPointData(void) const;
bool getPointData(uint8_t x, uint8_t y, uint8_t peak,
                  sPoint_t &point) const;
bool isDataNew(void) const;
bool isDataValid(void) const;
```

| API | 行为及必须写入 Doxygen 的说明 |
| --- | --- |
| 构造函数 | 保存外设和引脚参数，不在构造阶段发命令；serial 为空时初始化失败 |
| `setFrameBuffer()` | 可选接口，默认无需调用；可在 begin 前绑定，也可空闲时替换；拒绝空指针和零容量；释放此前内部缓存并清除旧帧有效状态，不释放用户内存 |
| `begin()` | 未提供用户缓存时自动分配 3072 个点，内存不足返回 false；初始化串口，设置 REPORT=0 并检查通信，处理在途帧后查询真实配置，确认 REPORT=0 才成功；不修改测量模式、输出选择或 NVS；可选用户缓存不足时允许初始化，取帧时返回 false |
| `getConfig()` | 发送 CONFIG?，返回完整实际配置；失败不改调用者传入结构体，缓存标记不可用时后续依赖操作先重查 |
| `setSensorMode()` | 映射并发送 MODE；处理不同模式重置 Peak/FRAME 的副作用，再查询实际状态 |
| `setPeakCount()` | 先按当前模式检查 1..max，不合法时不发送；不悄悄限幅 |
| `setMeasureMode(uint8_t x, uint8_t y)` | 点输出，发送 POINT；检查 x、y 在当前原画面范围内；最终帧宽高为 1×1，仍包含全部 Peak |
| `setMeasureMode(uint8_t y)` | 行输出，发送 LINE；检查 y 在当前原画面范围内；最终帧高度为 1，仍包含全部 Peak |
| `setMeasureMode(void)` | 全帧输出，发送 FRAME，恢复当前模式的完整画面输出 |
| `getBaudRate()` | 发送 BAUD?，完整成功后才更新调用者输出及缓存 |
| `setBaudRate()` | 仅接受 115200、460800、921600、1000000、2000000、3000000、4000000、5000000；完整收到旧速率的 OK 后切主机速率并检查通信；失败时不宣称同步成功 |
| `saveConfig()` | 显式执行 SAVE；其他 setter 均不自动保存；NVS 写入不可放在 loop 中反复执行，不需要模块重启 |
| `update()` | 仅在初始化成功且确认 REPORT=0 后，发送一次 GETFRAME 并等待一帧；有完整合法且容量足够的响应帧才返回 true；不将未经本次请求的帧作为成功响应 |
| `getAllPointData()` | 返回最近一次 update 的缓存 const 引用；必须先检查 valid，不发命令、不重新取帧 |
| `getPointData()` | 从有效缓存取一个点；x/y 为输出帧局部索引，peak 从 0 开始；POINT 只能传 0,0，LINE 的 y 只能为 0；越界/无有效帧返回 false，且不修改 point |
| `isDataNew()` | 返回缓存 isNew；首次完整有效帧为新，后续成功帧 ID 与上次成功帧不同即为新，允许回绕；读取本身不清除标志 |
| `isDataValid()` | 返回完整帧协议校验结果，与某点的 confidence 分开判断 |

补充时序和缓存约定：

- `setMeasureMode` 根据参数个数选择点、行、全帧输出，不使用默认参数；`setSensorMode(eMeasurementMode_t)` 仍独立用于设置分辨率/测量预设，二者不可混淆。示例分别使用 `setMeasureMode(x, y)`、`setMeasureMode(y)`、`setMeasureMode()`。
- 参数、模式和范围检查在发命令之前完成。设置成功收到 OK 后才更新配置；回复丢失时模块状态可能已经变化，不能盲用旧缓存。
- 配置或缓存改变后清除帧有效性，下一次 update 重新请求；不得把切换前残留的旧尺寸帧作为新配置数据发布。超时后须处理未完成请求和迟到帧，再开始新的取帧事务。
- `update()` 每次调用开始清除 valid/isNew；失败时 pointCount=0。重复 frameId 的完整合法帧仍可返回 true，但 isNew=false。
- 返回的帧引用和点指针只在下一次 update、配置修改、缓存替换或对象销毁之前可用；不能跨这些操作保留点引用使用。
- 控制指令需要明确、有界的等待时间；建议普通 AT 1000 ms，模式切换及 SAVE 5000 ms。帧等待默认 5000 ms；115200 下最大帧仅线上传输就约 2.4 s，不采用过短默认超时。
- `setBaudRate()` 应在没有未完成取帧事务且在途帧处理完后执行，保持问答模式。超时后提供用户按已知速率重新 begin 的恢复说明，不自动保存波特率。
- SET BAUD 修改设备 UART 波特率，主机串口需同步切换。重新上电后按已保存速率初始化主机。
- AT 收发、CRC、字节读取、模式映射、解析状态机等均为 private/protected，不作为用户业务 API。

## 6. 示例要求

示例目录与文件同名，使用 camelCase；注释、接线、初始化、错误检查和数据打印完整。示例优先用 ESP32 硬件串口，串口监视器与传感器使用不同串口，主机引脚在代码和接线表中保持一致。

| 示例 | 要求 |
| --- | --- |
| `getAllPointData/getAllPointData.ino` | 主示例：无需声明点数组，begin（内部自动分配缓存并设问答模式）、设 48X32、Peak=1、FRAME，检查全部返回值，loop 调用 update 请求一帧后逐点打印 XYZ、signal、confidence、frameId |
| `getPointData/getPointData.ino` | 配置 POINT，区分原画面选择坐标与缓存局部索引；示范读取每个 Peak，库自动管理缓存容量 |
| `getLineData/getLineData.ino` | 配置 LINE，按输出 width 和 peakCount 遍历，说明输出局部 y=0 |
| `getConfig/getConfig.ino` | 打印完整配置；展示不同模式对应的合法 Peak 数与切换副作用 |
| `saveConfig/saveConfig.ino` | 一次性配置/保存示例，SAVE 只在 setup 显式调用；说明写入后开机恢复 |
| `setBaudRate/setBaudRate.ino` | 演示旧速初始化、在取帧事务之外切换两端速率及验证，提醒保存后下次需用新速率初始化 |

示例不得只包含初始化；主示例必须完成配置链和真实数据读取，不默认打印缓存中的无效槽位为有效测量。

## 7. 后续库交付格式

```text
DFRobot_TMF8829/
  src/DFRobot_TMF8829.h
  src/DFRobot_TMF8829.cpp
  examples/<exampleName>/<exampleName>.ino
  resources/images/
  library.properties
  keywords.txt
  README.md
  README_CN.md
  LICENSE
  task.md
```

- README.md 为英文，README_CN.md 为中文，参照 SGX6410 的章节组织；当前空白 README_EN.md 后续是否移除在实际封库阶段处理，本次不修改。
- 头文件含 include guard；文件头包含 @file、@brief、@details、@n、@copyright、@license、@author、@version、@date、@url。
- public API 使用 @fn、@brief、@param、@return/@retval；模式切换副作用、缓存前置条件、坐标索引、保存和波特率变更流程必须写成 @n 说明。
- 字段、枚举、协议常量均有注释；协议数据逐字段解码，不依赖 packed 结构体或主机字节序。
- README 包含产品说明、接线、安装、兼容性、API、数据结构、示例和内存预算；只引用真实存在的图片。
- library.properties 的 name=DFRobot_TMF8829、version=1.0.0，其他字段按实际产品填写；不能复制 SGX6410 的作者、SKU、产品链接。
- keywords.txt 覆盖类、公开方法和业务类型；库以 MIT 格式组织，发布前确认项目授权信息。

## 8. 验收与待补信息

后续实现验收：

1. ESP32 基线能编译全部示例，实测 begin、查询、模式/Peak、POINT/LINE/FRAME 和问答取帧；每次 update 发送一次 GETFRAME，不发送 REPORT=1。
2. 校验 9 种模式及 Peak 上限，检查切换不同模式后 Peak=1、FRAME 的实际结果。
3. 检查 XYZ 有符号小端解析、Peak 顺序、索引范围、confidence=0 和 signal=0 的保存。
4. 检查错误 CRC、错误长度/帧尾、分段帧、连续帧、payload 含 AA55/55AA/OK、超时恢复及缓存不足，不越界、不发布半帧。
5. 检查 AT 回复与问答帧的正确解析、超时后迟到帧的处理，以及模块原有主动配置下 begin 能切回 REPORT=0；禁止用简单清空串口处理配置响应。
6. 检查波特率切换和 SAVE 后重启恢复；未连接硬件时明确标注未实测，不把编译通过写成硬件验证通过。

发布前待补：产品 SKU/销售名称、模块供电与 UART 电平、最终作者/维护者、仓库和产品链接、接线图片、实际需支持的主控板及硬件实测记录。上述缺项不以裸芯片资料或 SGX6410 参数代填。
