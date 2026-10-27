# DFRobot_TMF8829

- [English Version](./README.md)

面向 task.md 所述 TMF8829 模块固件的 Arduino 库，通过硬件串口以问答方式读取 XYZ 点云。提供分辨率预设、Peak 数量、点/行/全帧输出及显式配置保存，主控通过 UART 连接传感器。

## 产品链接

产品 SKU、销售名称、供电与 UART 电平、图片、最终作者/维护者、仓库和产品链接待确认。保留项目现有 MIT 授权信息，不套用其他产品参数。

## 目录

* [概述](#概述)
* [库安装](#库安装)
* [接线](#接线)
* [数据与内存](#数据与内存)
* [示例](#示例)
* [方法](#方法)
* [兼容性](#兼容性)
* [历史](#历史)
* [创作者](#创作者)

## 概述

* 硬件 UART，8N1，默认 921600；初始化切换到 REPORT=0，每次 update 请求一帧。
* 9 种测量预设；setSensorMode 设置分辨率，setMeasureMode 三个重载选择点、行或全帧。
* 校验长度、几何尺寸、CRC16/Modbus 和帧尾，保留 signal/confidence 为零的槽位。
* 库自动管理点数组；切换模式无需修改数组，update 不申请堆内存。
* 参数使用枚举；不提供主动上报、USB 接入及独立错误枚举。

## 库安装

* 将本库放入 Arduino 的 libraries/DFRobot_TMF8829，重新打开 IDE 后选择 examples 中的示例。
* 若将来发布到库管理器，可搜索 DFRobot_TMF8829 安装；当前不声明已上架。

## 接线

| 模块 | ESP32 |
| --- | --- |
| TX | GPIO16（Serial1 RX） |
| RX | GPIO17（Serial1 TX） |
| GND | GND |

模块供电和 UART 电平待确认，按模块规格核对。示例用 Serial 作调试控制台、Serial1 接传感器，主控引脚可在构造函数修改。

## 数据与内存

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

## 示例

* [getAllPointData](./examples/getAllPointData/getAllPointData.ino)：自动管理缓存、48X32、Peak=1、全帧请求及打印。
* [getPointData](./examples/getPointData/getPointData.ino)：原画面选点，用局部 (0,0) 读取所有 Peak。
* [getLineData](./examples/getLineData/getLineData.ino)：原画面选行，局部 y=0。
* [getConfig](./examples/getConfig/getConfig.ino)：完整配置、Peak 上限和切换模式副作用。
* [saveConfig](./examples/saveConfig/saveConfig.ino)：只在 setup 中保存一次。
* [setBaudRate](./examples/setBaudRate/setBaudRate.ino)：空闲时切换两端波特率并验证。

## 方法

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
  DFRobot_TMF8829(HardwareSerial *serial, eBaudRate_t baudRate = eBaudRate115200, int8_t rxPin = -1, int8_t txPin = -1);

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
   * @fn setMeasureMode(uint8_t, uint8_t)
   * @brief 选择原始画面中的一个像素，以 POINT 输出。
   * @param x 原始列坐标，从零开始，小于预设宽度。
   * @param y 原始行坐标，从零开始，小于预设高度。
   * @n 输出为 1x1，包含全部 Peak；缓存坐标为 (0,0)，XYZ 不作平移。
   * @n 清除帧有效状态，不自动保存。
   * @return 命令及配置核对成功返回 true。
   */
  bool setMeasureMode(uint8_t x, uint8_t y);

  /**
   * @fn setMeasureMode(uint8_t)
   * @brief 选择原始画面中的一行，以 LINE 输出。
   * @param y 原始行坐标，从零开始，小于预设高度。
   * @n 输出为预设宽度 x 1，包含全部 Peak；缓存 y=0，XYZ 不作平移。
   * @n 清除帧有效状态，不自动保存。
   * @return 命令及配置核对成功返回 true。
   */
  bool setMeasureMode(uint8_t y);

  /**
   * @fn setMeasureMode(void)
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

## 兼容性

| 主板 | 通过 | 未通过 | 未测试 | 备注 |
| --- | --- | --- | --- | --- |
| ESP32 | | | √ | 软件验证记录见 tests/host/README.md；硬件未实测。 |

其他主控需硬件串口、足够 RAM 和准确波特率；未验证 UNO/SoftwareSerial。编译与模拟测试不代表硬件兼容性已验证。

## 历史

* 2026/10/23 - V1.0.0 版本

## 创作者

Written by cdjq, 2026. 最终维护者及联系方式待确认。欢迎访问 [DFRobot](https://www.dfrobot.com/)。
