# AGV 扫码板硬件配置与 ESP32-P4 Demo

基于 ESP-IDF 6.0，目标芯片 ESP32-P4 v3.2，最低芯片版本设为 v3.1。

硬件芯片型号按设计记录为 ESP32P4RW32X。本文的 `GPIOxx` 表示 GPIO 编号，`PINxx` 表示芯片封装物理脚号，两者不可混用。

## 当前 demo 功能

| 引脚 | 功能 | 电平 |
| --- | --- | --- |
| GPIO53 | 红灯循环闪烁，亮 500ms、灭 500ms | 低电平点亮 |
| GPIO4 | 按住按钮亮，松开按钮灭 | 低电平点亮 |
| GPIO35 | 按钮输入，启用内部上拉，30ms 消抖 | 按下接地 |

程序启动时两灯熄灭，然后开始红灯闪烁。按钮独立控制绿灯。
启动信息包含 RAM、PSRAM、自检结果、Flash 和时钟配置，并每 5 秒重发，便于 USB Serial/JTAG 复位后重新连接时查看。
这是应用信息重发，不是 ROM/bootloader/FreeRTOS 原始日志重放；原始早期日志如需完整抓取，可使用独立 USB 转串口连接配置后的 UART 控制台。

## 硬件 GPIO 分配与目标功能

以下为整板目标功能。当前 demo 的 GPIO4、GPIO53、GPIO35 行为以“当前 demo 功能”为准，尚未实现扫码、相机供电和照明控制。

### 指示灯与按钮

| GPIO | 网络名称 | 方向 / 配置 | 硬件目标功能 |
| --- | --- | --- | --- |
| GPIO4 | LED_OK | 输出，低电平点亮，高电平 3.3V 熄灭 | 绿色 LED，解码成功闪一下 |
| GPIO53 | LED_ERR | 输出，低电平点亮，高电平 3.3V 熄灭 | 红色 LED，解码失败闪一下 |
| GPIO35 | BOOTMODE | 上拉输入，按钮按下为低电平，需消抖 | 按下瞄准灯亮、松开瞄准灯灭；每次有效按下将内、外补光灯开关状态各反转一次 |

`BOOTMODE` 是板上网络名称；本项目将 GPIO35 用作业务按钮，不据此认定它是芯片 ROM 下载模式的绑带引脚。

### 相机电源使能

| GPIO | 网络名称 | 控制信号电平 | 对应电源 / 功能 |
| --- | --- | --- | --- |
| GPIO33 | EN_VSYS | 低电平使能，高电平关闭（按用户最新更正） | VSYS，输出 3.3V |
| GPIO6 | EN_1V8 | 高电平 3.3V 使能，低电平关闭 | DOVDD，输出 1.8V |
| GPIO26 | EN_2V8 | 高电平 3.3V 使能，低电平关闭 | AVDD，输出 2.8V |
| GPIO28 | EN_1V2 | 高电平 3.3V 使能，低电平关闭 | DVDD，输出 1.2V |

控制 GPIO 的高电平电压与被使能电源的输出电压是两个概念，例如 EN_1V8 使用 3.3V 高电平控制 DOVDD 输出 1.8V。

### 相机控制、I2C 与照明

| GPIO | 网络名称 | 方向 / 电平要求 | 功能 |
| --- | --- | --- | --- |
| GPIO16 | I2C_SDA | I2C 开漏，目标高电平 1.8V；保留外部 2kΩ 上拉至 1.8V | 相机及补光驱动的 I2C 数据线 |
| GPIO17 | I2C_SCL | I2C 开漏，目标高电平 1.8V；保留外部 2kΩ 上拉至 1.8V | I2C 时钟线 |
| GPIO18 | EN_AIM | 输出，高电平 1.8V 开启，低电平关闭 | 扫码头瞄准灯 |
| GPIO19 | EN_FLASH | 输出，高电平 1.8V 开启，低电平关闭 | 扫码头内补光灯 |
| GPIO20 | XSHUTDOWN | 输出，高电平 1.8V 开启，低电平关闭 | OV01C1B sensor 使能 |
| GPIO21 | EN_MCLK | 输出，高电平 1.8V 使能，低电平关闭 | 正常相机模式：使能外部 24MHz 有源晶振；当前测试模式：GPIO21 自身输出 24MHz，见文末 |
| GPIO22 | STROBE | 输出，高电平 1.8V 使能，低电平关闭 | 外补光驱动 AW36515FCR；输出电流通过 I2C 配置 |
| GPIO23 | E_STROB | 下拉输入 | 扫码头曝光信号，当前预留未使用 |

I2C 的原始配置要求包含内部上拉。内部上拉跟随对应 IO 电源域，不能仅靠软件将上拉电压设为 1.8V；启用前需核对 GPIO16/17 的 IO 域供电。若内部上拉会引入高于 1.8V 的电压，应关闭内部上拉并使用外部 1.8V 上拉。
GPIO18～GPIO22 的 1.8V 输出同样依赖对应 IO 域的实际供电，不能通过 `gpio_set_level()` 单独选择输出电压。瞄准灯与内补光还需先使能VSYS：GPIO33=0开启3.3V，GPIO33=1关闭；修正极性后两种灯已由用户确认正常点亮。

### 用户 IO 与串口

| GPIO | 网络名称 | 配置 | 功能 |
| --- | --- | --- | --- |
| GPIO40 | ESP_IN | 上拉输入 | 用户 IO 输入 |
| GPIO41 | ESP_OUT | 输出 | 用户 IO 输出 |
| GPIO42 | ESP_RX0 | 串口 RX，启用上拉 | 用户串口接收 |
| GPIO43 | ESP_TX0 | 串口 TX 输出 | 用户串口发送 |

GPIO39～GPIO48 的供电域为 VDD_IO_5，板上连接 LDO_VO4，因此这组 IO 的电平随 LDO_VO4 的 1.8V / 3.3V 配置变化。`ESP_RX0` / `ESP_TX0` 为网络名称，后续软件需显式选择 UART 控制器及引脚映射。

## 专用接口与存储器

### USB 接口

| 编号类型 | 引脚 | 网络 / 信号 | 用途 |
| --- | --- | --- | --- |
| GPIO | GPIO24 | USB_JTAG D− | USB Serial/JTAG，烧录与日志，当前设备端口 COM62 |
| GPIO | GPIO25 | USB_JTAG D+ | USB Serial/JTAG，烧录与日志 |
| 封装物理脚号 | PIN49 | USB_DM | 高速 USB，目标功能 UVC + CDC |
| 封装物理脚号 | PIN50 | USB_DP | 高速 USB，目标功能 UVC + CDC |

高速 USB 的 UVC + CDC 与 GPIO24/25 的 USB Serial/JTAG 是不同接口；当前 demo 尚未实现 UVC + CDC。

### 外挂 Flash 与 PSRAM

Flash 设计记录型号为 `GD25Q256EYIGR`，引脚连接如下，均为封装物理脚号：

| 物理脚号 | 网络名称 |
| --- | --- |
| PIN27 | FLASH_CS_A |
| PIN28 | FLASH_Q_A |
| PIN29 | FLASH_WP_A |
| PIN31 | FLASH_HD_A |
| PIN32 | FLASH_CK_A |
| PIN33 | FLASH_D_A |

容量记录存在差异：原始说明写为“16MBit”，型号中的 `256` 通常表示 256Mbit（32MB）；但当前 COM62 板卡实测 JEDEC ID 为 `0xC84018`，检测容量为 **16MB（128Mbit）**。当前工程按实测配置为 16MB；最终物料型号需结合芯片丝印及 BOM 确认，勿将 Mbit 与 MB 混用。
PSRAM 当前实测为 **32MB**，已启用启动初始化与内存测试。

### OV01C1B 与 MIPI CSI

| 项目 | 配置 |
| --- | --- |
| Sensor | OV01C1B |
| I2C 地址 | 7-bit `0x10` |
| 数据接口 | MIPI CSI，2 条数据 lane |
| 数据格式 | RAW10 |
| 目标分辨率 | 1024 × 1024 |
| 目标帧率 | 50 FPS |
| Sensor MCLK | 外部有源晶振 24MHz，由 GPIO21 使能 |
| ESP32-P4 MIPI PHY 电源 | LDO_VO3 → VDD_MIPI_DPHY，2.5V |

以下连接均为封装物理脚号，不是 GPIO 编号：

| 物理脚号 | 信号 |
| --- | --- |
| PIN42 | CSI_A_DATA0N |
| PIN43 | CSI_A_DATA0P |
| PIN44 | CSI_A_CLKP |
| PIN45 | CSI_A_CLKN |
| PIN46 | CSI_A_DATA1N |
| PIN47 | CSI_A_DATA1P |

RAW10、1024 × 1024、50 FPS 为相机目标配置，当前 GPIO demo 尚未写入 sensor 寄存器，也未启动 CSI 采集。

## 相机电源时序

### 上电

开始上电前，补光灯、瞄准灯和 XSHUTDOWN 应保持低电平；各电源使能按以下顺序操作：

| 步骤 | 操作 | GPIO / 参数 |
| --- | --- | --- |
| 1 | 打开 VSYS | GPIO33 = 0，VSYS 输出 3.3V |
| 2 | 打开 24MHz MCLK | GPIO21 = 1，外部有源晶振输出时钟 |
| 3 | 打开 DOVDD | GPIO6 = 1，DOVDD 输出 1.8V |
| 4 | 打开 AVDD | GPIO26 = 1，AVDD 输出 2.8V |
| 5 | 打开 DVDD | GPIO28 = 1，DVDD 输出 1.2V |
| 6 | 延时 | 5ms |
| 7 | 打开 OV01C1B | GPIO20 = 1，XSHUTDOWN 拉高至 1.8V |
| 8 | 延时 | 5ms |
| 9 | I2C 配置 OV01C1B 寄存器 | 7-bit 地址 `0x10`，写入相机工作模式 |

上述顺序按板级设计要求记录，未给出延时的相邻步骤不额外假定具体等待时间。后续驱动需结合 sensor 规格书和电源稳定时间验证。
LDO_VO3 的 2.5V 是 ESP32-P4 CSI PHY 的电源，应在 CSI 初始化前单独申请，不替代以上相机供电步骤。

### 下电

停止采集并协调释放 CSI 资源后，相机 GPIO 按以下顺序关闭：

| 步骤 | 操作 | GPIO |
| --- | --- | --- |
| 1 | 关闭内、外补光灯 | GPIO19 = 0，GPIO22 = 0 |
| 2 | 关闭瞄准灯 | GPIO18 = 0 |
| 3 | 关闭 OV01C1B | GPIO20 = 0 |
| 4 | 关闭 24MHz MCLK | GPIO21 = 0 |
| 5 | 关闭 DVDD | GPIO28 = 0 |
| 6 | 关闭 AVDD | GPIO26 = 0 |
| 7 | 关闭 DOVDD | GPIO6 = 0 |
| 8 | 关闭 VSYS | GPIO33 = 1 |

若 MIPI PHY 电源没有其他使用者，可在 CSI 资源释放后释放 LDO_VO3。相机断电期间应避免 I2C 和控制线通过上拉或高电平向 sensor 反向供电。

## 时钟配置

| 时钟 | 当前配置 / 用途 |
| --- | --- |
| ESP32-P4 主晶振 | 外部 40MHz，`CONFIG_XTAL_FREQ=40` |
| RTC 慢时钟 | 外部 32.768kHz 无源晶体，`CONFIG_RTC_CLK_SRC_EXT_CRYS=y`；启动校准 3000 周期 |
| 相机 MCLK | 外部 24MHz 有源晶振，由 EN_MCLK 控制；与主晶振、RTC 时钟独立 |

RTC 波形测试：烧录后可在外部 32.768kHz 晶体两端对应的测试点观察振荡，周期约 30.52µs。使用低电容探头（优先有源探头，或低电容 10× 探头）和短接地，避免探头负载使晶体停振；晶体端波形不一定是数字方波。
串口每 5 秒打印 RTC 当前实际时钟源及启动校准得到的频率。若晶体起振失败，IDF 可能回退至内部 RC，应以实际源日志为准，不能仅根据 sdkconfig 判定起振成功。

## LDO_VO 供电分配

| LDO 通道 | 用途 / 硬件连接 | 工作电压 | 当前 demo 状态 |
| --- | --- | --- | --- |
| LDO_VO1 | Flash 供电 | 3.3V（3300mV） | ESP-IDF 已申请并保留 |
| LDO_VO2 | PSRAM 供电 | 1.8V（1800mV） | ESP-IDF 已申请并保留 |
| LDO_VO3 | 连接 VDD_MIPI_DPHY，为 MIPI CSI/DSI PHY 供电 | 2.5V（2500mV） | 尚未申请；启用 CSI 前需配置 |
| LDO_VO4 | 连接 VDD_IO_5，决定 GPIO39～GPIO48 所在供电组的 IO 电平 | 按接口需求选择 1.8V 或 3.3V | 当前主工程默认3300mV，GPIO39～48使用3.3V |

VO1/VO2 由 ESP-IDF 用于 Flash/PSRAM，应用不得随意修改其电压或释放通道。

### LDO_VO3：MIPI PHY 电源

使用 OV01C1B 的 MIPI CSI 接口时，应在初始化 CSI 前申请 LDO 通道 3，设置为 2500mV：

```c
#include "esp_ldo_regulator.h"

static esp_ldo_channel_handle_t mipi_ldo;

const esp_ldo_channel_config_t config = {
    .chan_id = 3,
    .voltage_mv = 2500,
};
ESP_ERROR_CHECK(esp_ldo_acquire_channel(&config, &mipi_ldo));
```

CSI 工作期间保持该通道供电；停止并释放 CSI 后，才释放对应 LDO 句柄。
CSI 和 DSI 共用 VDD_MIPI_DPHY 时，需协调通道引用，确保所有使用者停止后才断电。
该 2.5V 电源用于 ESP32-P4 的 MIPI PHY，与 OV01C1B 的 AVDD、DOVDD、DVDD 分别管理。

### LDO_VO4：VDD_IO_5 电压切换

修改 `main/board_config.h` 中的 `BOARD_VDDO4_MV`：当前主工程默认`3300`，使用3.3V输入电源直通；`1800`设置为1.8V，`0`保留启动状态。本目录为历史demo，当前代码以主工程`main/board_config.h`为准。
重新编译、烧录后生效；这是 app_main 阶段的配置，不保证复位期间电压。
板上已确认 LDO_VO4 输出连接 VDD_IO_5；切换前需确认没有外部电源与其冲突，且外接器件支持对应电压。
切换影响 GPIO39～GPIO48 整组供电域，包括 GPIO40（ESP_IN）、GPIO41（ESP_OUT）、GPIO42（ESP_RX0）、GPIO43（ESP_TX0），不能为其中单个 GPIO 独立选择供电电压。

### 电压状态说明

LDO dump 表显示软件请求值；未被驱动申请的通道显示 ID=-1、voltage_mv=0，不代表引脚实测为 0V。
上述配置值不等于万用表实测值；VO3 未启用及 VO4 保留启动状态时，实际输出电压需结合硬件和测量确认。

## 编译与烧录

VS Code 打开本目录，工程设置已选择 `C:\esp\v6.0\esp-idf`。
使用 ESP-IDF 扩展的 Build、Flash、Monitor 命令即可。

也可以在 PowerShell 中加载本机 ESP-IDF 6.0 环境后编译：

```powershell
. C:\Espressif\tools\Microsoft.v6.0.PowerShell_profile.ps1
idf.py set-target esp32p4
idf.py build
idf.py -p COM端口号 flash monitor
```

将 `COM端口号` 替换为设备实际端口，例如 `COM7`。使用 GPIO24/25
对应的 USB Serial/JTAG 接口烧录和查看日志；按 `Ctrl+]` 退出日志。

本 demo 保留红灯闪烁、按钮控制绿灯，并初始化 AW36515 的 I2C 与 STROBE 低电平。摄像头电源、CSI 和高速 USB 功能暂未初始化。

## AW36515 外部补光驱动

依据 `AW36515.pdf`（Oct. 2019 V1.1），器件支持最高 400 kHz I2C；当前波形测试配置为 400 kHz，固定 7 位地址为 **0x63**（写地址 0xC6，读地址 0xC7）。SDA=GPIO16、SCL=GPIO17，GPIO 内部上拉已开启，当前板上的 2 kΩ/1.8 V 外部上拉未焊接，仅使用内部上拉；STROBE=GPIO22，高电平触发。初始化等待 VIN 超过 POR 至少 2 ms，读取 0x00 芯片 ID，然后保持待机并写入：0x01=0x80（关闭输出）、0x03/04=0x7F（Flash 电流约 1 A）、0x05/06=0x7F（Torch 电流约 250 mA）、0x07=0x09（2.8 A 限流、2 MHz）、0x08=0x1A（Torch 1 ms 斜坡、Flash 超时 600 ms）。寄存器 0x0A/0x0B 为故障状态，读取可清除相应故障。

## ESP32-P4 v3.2 高速配置

PCN202600801 说明 v3.2 的 CPU 稳定频率为 400 MHz、PSRAM 接口稳定支持最高 250 MHz。本工程已选择 CPU=400 MHz、PSRAM=250 MHz；Flash 保持 80 MHz，实际配置为 DIO（双线读取），容量为实测 16 MB、JEDEC 0xC84018；此配置不代表已确认该具体 Flash 型号的绝对最高频率。Flash 频率只有在确认具体器件数据手册、信号完整性和启动模式后才可继续提高。

## I2C 波形测试

CPU 保持 400 MHz、PSRAM 250 MHz、Flash DIO 80 MHz 配置；固件不再执行 CPU、PSRAM 或 Flash 压力测试。

- I2C：启动后约 5 秒内每 2 ms 读取 AW36515 的 `0x00` 芯片 ID，并只打印一次累计结果；之后继续读取 AW36515，但不再重复打印汇总，以便持续测量波形。

示波器/逻辑分析仪：SDA 接 GPIO16，SCL 接 GPIO17，地接板 GND。设置 I2C 解码、7 位地址 `0x63`、SCL 目标 400 kHz（周期约 2.5 μs）；每笔读事务为 START、0xC6、寄存器 0x00、重复 START、0xC7、数据 0x30、NACK、STOP。约 2 ms 为事务间隔，不是时钟周期。逻辑高电平由板上 IO 电源与上拉电源决定，软件的 400 kHz 设置不证明实测电平或上升时间合格。

启动信息只打印一次；按钮状态仅在变化时打印。I2C 汇总在启动后约 5 秒打印一次，之后持续读取 AW36515 以便测量波形，不再重复打印汇总。

本版本不运行 CPU、PSRAM、Flash 稳定性压力测试；I2C 持续读取 AW36515，启动约 5 秒后输出一次计数汇总。

## GPIO21（EN_MCLK）24 MHz 输出测试

`main/board_config.h` 中 `BOARD_MCLK_TEST_ENABLE=1` 开启此测试。程序通过 ESP-IDF 6.0 的 `esp_clock_output_start()` 把 SPLL 时钟路由到 GPIO21，并用 `esp_clock_output_set_divider(..., 20)` 配置 480 MHz / 20 = 24 MHz。先保持引脚低电平，完成时钟路由与分频后再解除保持。该硬件输出不依靠任务翻转 GPIO；CPU 保持 400 MHz、PSRAM 250 MHz、Flash 80 MHz 配置。

示波器探头接 GPIO21/EN_MCLK，地接就近 GND，建议 10× 探头和短接地弹簧。目标频率 24 MHz、周期约 41.67 ns；记录高低电平宽度、幅度和过冲。日志 `GPIO21 MCLK test: configured=24000000 Hz` 仅表示软件的时钟源/分频配置，不代替示波器实测频率、占空比或抖动。

现有板级连接是 GPIO21 → 24 MHz 有源晶振的 EN/OE，不是相机 MCLK 输入。当前测试会把晶振使能端高速切换，其输出不能据此视作正常相机时钟。若将 MCU 输出用作相机 MCLK，需要修改走线/跳线，连接 GPIO21 到相机 MCLK，同时隔离原有晶振输出，避免两个时钟输出相连。GPIO21 的电平取决于实际 I/O 供电域，本测试不改变其电压。

测试结束可将 `BOARD_MCLK_TEST_ENABLE` 改为 0 后重新编译；该选项关闭时本函数不接管 GPIO21，未来相机驱动应恢复 EN_MCLK 的静态使能控制。

### 无外部上拉时的 I2C 波形测试

当前 SDA/SCL 外部上拉电阻未焊接，仅启用 ESP32-P4 内部弱上拉。板上实测：I2C 设为 **10 kHz** 时，SCL 波形为方波；未焊外部上拉时，只有 10 kHz 能保证获得方波，较高的时钟速率不作方波保证。当前固件已配置为 400 kHz，用于外部上拉焊接后的波形与通信测试。内部上拉电压取决于 GPIO16/17 所在 IO 电源域，不能通过 I2C 频率配置成 1.8 V。若需提高速率并满足上升时间要求，应补焊合适的外部上拉至 1.8 V。
