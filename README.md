# AGV SCAN / ESP32-P4 — E6 与 OV9281

将 `ESP32P4_XCF_OV9281` 的实时 DataMatrix 解码、同帧角点精定位、双核最新帧流水线、USB CDC 图片/结果、MJPEG UVC 和 UART v6 定位协议移植到当前 AGV 板。E6（OV01C1B）采集与寄存器驱动参考 `ESP32P4_XCF`。原 GPIO 测试工程保存在 `legacy_gpio_demo/`。


## GPIO35补光修正与GPIO36换头控制

GPIO35仍为低有效、30ms消抖；每次按下同时切换内/外补光，松开保持补光状态；瞄准随按住/松开。AW36515改为软件Torch模式，0x01=0x0B开启两通道、0x00关闭，禁用TX/STROBE/TORCH硬件触发。GPIO22随补光状态保持，切换瞄准不再先将GPIO22拉低。电流码仍为0x05/0x06=0x10（约32.34mA/路，非实测）。旧文0x13硬件Torch记录为历史配置。

新增GPIO36上拉输入、低有效、30ms消抖：工作时第一次有效按下，将断电状态保存到独立NVS命名空间head_power，关闭灯控、sensor、晶振、DVDD/AVDD/DOVDD，最后GPIO33=1关闭VSYS，并重启进入换头等待。GPIO16/17/23配置为无上下拉高阻输入。红灯常亮、USB视频/CDC断开，COM62日志输出HEAD OFF；此状态下GPIO35不控制灯。意外重启或整机重上电仍保持换头等待。

先松开GPIO36后再次按下，清除断电状态并重启，按既有上电时序重新探测I2C地址和芯片ID，重新初始化采集、曝光、标定和UVC原生尺寸。不会复用旧扫码头的缓冲或内参。恢复后补光默认关闭，GPIO35重新控制。主控与COM62调试口保持供电；这是关闭扫码头相关电源，非关闭整板。

首次实物换头前应确认接口端VSYS、DOVDD、AVDD、DVDD均已降至断电电压；红灯/数字回读仅表示控制状态，不能测出电源残压。固件没有电源电压反馈。NVS错误时停留在禁止相机上电的错误状态，不自动擦除标定或断电状态。

## 当前默认：启动自动识别，一个固件通用

默认`./build_p4.ps1`与`./flash_p4.ps1 -Port COM62`使用`sdkconfig.auto`，产物`build_auto/agv_scan_auto.bin`。通用固件包与SHA256见[dist/auto_sensor_20261124](dist/auto_sensor_20261124/README.md)。`-Sensor e6|ov9281`仍可显式编译旧单传感器配置；下文保留历史记录。

按AGV板时序上电后，检查7位I2C地址0x10、0x60并核对芯片ID：0x10/0x56014311选择E6；0x60/0x9281选择OV9281。零个或两个地址应答、ID不符或总线错误均报错，并在返回失败前关闭相机供电。仅启动时检测，更换相机需断电重启，无需重烧；不支持热插拔。当前OV9281支持用户确认的SID低地址0x60。

识别结果决定CSI模式、图像尺寸、专用曝光与降噪/解码路径；先识别相机，再初始化USB和UART标定服务。UVC枚举当前原生尺寸，USB序列号按E6/OV9281区分，避免Windows缓存另一型号尺寸。当前OV9281 CDC为COM69。标定独立使用scan_pose和pose_ov9281，保留已有NVS。

通用固件已通过COM62烧录和哈希校验；自动识别OV9281地址0x60、PID0x9281，首帧921600字节。UVC5.281秒收到145帧1280×720图像。独立重启后内补光关/开/关各6.5秒，成功/处理分别106/117、119/121、126/127，开灯2秒后的87帧全部成功；351条012345均为精修角点质量2，最大结果年龄99.756ms，9张快照CRC通过，协议错误0，结束恢复按钮自动模式。证据为docs/auto_ov9281_boot.log、docs/auto_ov9281_lighting_validation.json及docs/auto_ov9281_uvc_validation.json。

UVC与CDC补光性能分别测量。并发试验中预览增加处理负载，关闭主机视频后曾持续收到预览快照，因此CDC验收采用独立重启后的结果，不能据此声称并发性能相同。

主机测试编译实际探测、运行时配置与UVC描述符修补C代码，覆盖两型号、错误ID、无设备/双地址、I2C失败及清理、动态尺寸和独立标定。两种运行时尺寸的定位回归各815组通过；证据见docs/sensor_auto_host_validation.json、docs/e6_pose_auto_validation.json和docs/ov9281_pose_auto_validation.json。用户选择本轮只验证OV9281，通用固件换回E6的实机验证待完成；外补光、UART电气通信、镜头标定和运动场景仍未验收。

## OV9281 支持与版本保存（2026-11-24）

当前E6完整基线已提交至 https://github.com/jq8718/AGV_TEST ，提交`a4828d3`，标签`e6-baseline-20261124`。以下双传感器版本保留同一套DataMatrix/QR解码、同帧精修角点、100ms结果时限、四CSI缓冲与双核流水线、MJPEG UVC、CDC快照/结果/诊断、UART0 v6 CRC16、标定下载及NVS保存、GPIO35按钮与LED反馈。

通过编译配置选择传感器，默认仍为E6；一次固件仅启用所选传感器的探测器，不会误探测另一种。两种配置使用独立构建目录和应用文件，禁止混用。

| 项目 | E6 / OV01C1B | OV9281 |
| --- | --- | --- |
| 配置 | sdkconfig.e6 | sdkconfig.ov9281 |
| 编译 | ./build_p4.ps1 -Sensor e6 | ./build_p4.ps1 -Sensor ov9281 |
| 烧录 | ./flash_p4.ps1 -Sensor e6 -Port COM63 | ./flash_p4.ps1 -Sensor ov9281 -Port COM62 |
| 应用 | build_e6/agv_scan_e6.bin | build_ov9281/agv_scan_ov9281.bin |
| 输入模式 | 1024×1024 RAW10，单lane，50fps | 1280×720 RAW8，双lane，50fps |
| UVC | 1024×1024 MJPEG / 30fps | 1280×720 MJPEG / 30fps |
| SCCB地址（7位） | 0x10 | 0x60（SID低；配置项可更改） |
| 曝光控制 | 实测RAW8灰度反馈，目标65 | 参考OV9281的ISP/IPA自动曝光 |
| 标定NVS命名空间 | scan_pose，兼容原E6已存标定 | pose_ov9281 |

OV9281驱动及1280×720模式表已存在于本地esp_cam_sensor组件，并与参考ESP32P4_XCF_OV9281一致。OV9281沿用当前AGV板的GPIO16/17 SCCB、GPIO20高有效sensor使能、GPIO21外部24MHz晶振使能、GPIO33低有效VSYS和其他电源/灯控；不迁移参考板GPIO7/8的I2C与GPIO0的供电接法。接入模块/转接板需匹配这些电气连接以及两条CSI数据lane；用户已确认OV9281接线完成，以上I2C、复位、晶振、电源与灯控均沿用AGV板。

E6专用3×3 ROI去噪、ISP降噪和补光曝光修复仅作用于E6；OV9281使用原参考解码搜索和ISP/IPA曝光路径。GPIO35按住瞄准、每次按下切换补光，VSYS低有效与VDDO_4默认3.3V均由两种配置共用。外补光仍需接入实物后验收。

标定按传感器独立保存；切换到OV9281后不会读取E6内参，也不导入参考工程的镜头内参。分别使用`@CAL,SET,1024,1024,...`或`@CAL,SET,1280,720,...`配置本机实测值，尺寸不符则拒绝，未标定时pose_valid=0。内容、同帧角点仍可输出。

OV9281已于2026-11-24通过COM62烧录和实机验证；高速USB CDC枚举为COM68。启动确认PID=0x9281、1280×720 RAW8、四CSI缓冲和921600字节首帧；VDDO_4=3300mV。采图、静止标签识码、同帧精修角点、内补光切换及UVC均通过，详见文末实测记录。GPIO UART电气通信、外补光和本机镜头标定仍待现场验证。下文E6数据为保留的历史实测记录。

## 硬件与默认行为

- ESP32-P4 v3.2；ESP-IDF 6.0；CPU 400MHz、PSRAM 32MB/250MHz、Flash 16MB/DIO/80MHz。
- E6：I2C0 SDA=GPIO16 / SCL=GPIO17，7 位地址 `0x10`；1024×1024 RAW10，50fps，24MHz 外部晶振。使用参考工程历史版本的原厂 1-lane 模式，CSI lane0 接收，lane1 不启用。连线有两条数据通道不等于当前传感器模式使用两条。
- 相机电源：GPIO33=0开启VSYS（高电平关闭） → GPIO21 晶振使能 → GPIO6 DOVDD → GPIO26 AVDD → GPIO28 DVDD → 等待5ms → GPIO20 XSHUTDOWN → 等待5ms。GPIO21 使用静态高电平，已停止旧的24MHz测试输出。MIPI PHY由视频驱动申请 LDO3=2.5V。
- VSYS供电与灯控：EN_VSYS/GPIO33低电平使能3.3V、高电平关闭；瞄准灯和内补光都依赖VSYS。点灯前确保GPIO33=0并等待5ms，GPIO18/19高有效；下电先关闭灯控，最后GPIO33=1关闭VSYS。修正后用户已确认两种灯均正常点亮。外补光尚未连接。
- GPIO35 低有效，30ms消抖；按住 GPIO18 瞄准灯开启，松开关闭，每次有效按下切换内/外补光，启动时关闭补光。
- GPIO19 控制内补光；AW36515 `0x63` 控制外补光，初始Torch电流码 `0x10`，两通道由GPIO22 STROBE/TORCH高电平使能，寄存器0x01=0x13启用硬件Torch，关闭时GPIO22先拉低再写0x80。实际光照、电流和温升仍需现场验收。
- GPIO4绿灯/ GPIO53红灯均低有效，显示扫码成功/失败；GPIO40上拉输入、GPIO41初始低输出、GPIO23下拉输入预留曝光信号。
- UART0：GPIO43 TX、GPIO42 RX，921600、8N1。VDD_IO_5连接VDDO_4（LDO通道4），为GPIO39～48供电；`main/board_config.h` 默认 `BOARD_VDDO4_MV=3300`，在初始化这组IO前配置为3.3V（输入电源直通模式）。已编译并经COM63烧录，运行时LDO状态表确认通道4为3300mV，记录见 `docs/e6_vddo4_3300_validation.log`。
- E6曝光：实时采集任务每帧按32像素间隔采样RAW8灰度网格，每80ms至多调节一次曝光/增益，目标灰度65、稳定区间60～70。仅E6停用ISP算法对曝光/增益的写入，避免两个控制器相互覆盖；曝光上限10ms、增益上限15倍，保留50fps采集。开关补光后约1秒收敛，切换期间可能短暂识码失败。
- I2C继续沿用本板当前内部上拉配置，传感器访问100kHz；软件配置不能改变I/O域电压。板卡原始接线和电平记录见 `legacy_gpio_demo/README.md`。

## 功能与主机接口

默认连续 DataMatrix 模式；`CONFIG_XCR_DATAMATRIX_ONLY` 可切换QR模式。保留参考工程的小码、局部阈值、阴影补救和透视解码改进，不使用上一帧位置ROI。四块CSI缓冲；Core0采集/前段处理，Core1后段解码/角点精修；超过100ms的结果丢弃，持续5秒停帧触发重启。

高速USB设备 VID:PID=`303A:8001`，产品名称随配置为 `AGV SCAN E6 UVC+CDC` 或 `AGV SCAN OV9281 UVC+CDC`；UVC分辨率分别为1024×1024、1280×720，均为MJPEG/30fps。CDC结果及快照格式见 [USB_PROTOCOL.md](USB_PROTOCOL.md)。本次OV9281调试CDC为COM68，烧录/日志USB Serial/JTAG为COM62，端口随Windows枚举可能变化。

UART保留CRC16、完整/精简帧、前导零、30/50/70mm码体尺寸、相机内参下载及NVS保存。见 [UART协议](docs/STM32_UART_PROTOCOL.md) 与 [STM32 C解析示例](examples/stm32_uart/README.md)。E6镜头不使用OV9281内参；未下载本机1024×1024标定时 `pose_valid=0`，仍发送有效解码内容及完整模式角点，物理位置字段置零。

```text
@CAL,GET
@CAL,SET,1024,1024,fx,fy,cx,cy,k1,k2,p1,p2,k3
@OUT,FULL
@OUT,COMPACT
```

命令以CR或LF结尾，内参命令通过GPIO UART发送。`fx/fy/cx/cy`及畸变必须来自本机E6实测标定，不能使用示例猜测值。

## 编译、烧录与产物

```powershell
./build_p4.ps1
./flash_p4.ps1 -Port COM63
```

脚本加载 `C:\Espressif\tools\Microsoft.v6.0.PowerShell_profile.ps1`，使用 `sdkconfig.e6`、`build_e6/`。原GPIO测试配置与原build目录保留。主固件 `build_e6/agv_scan_e6.bin`；烧录地址由 `build_e6/flash_args` 给出，bootloader=0x2000、分区表=0x8000、应用=0x10000。

## 验证记录

已完成：IDF6完整编译、COM63烧录及哈希校验；实机芯片ID `56 01 43 11`；1024×1024首帧1048576字节，四MMAP缓冲，HS USB枚举；COM67 CDC快照CRC及JPEG解码通过；UVC约5.27秒接收149帧1024×1024图像。证据见 `docs/e6_boot.log`、`docs/e6_cdc_validation.json`、`docs/e6_uvc_validation.json`。

主机定位回归：`py -3.12 tools/validate_e6_pose.py`，815组案例通过，覆盖未标定拒绝、错误传感器分辨率拒绝和显式测试内参下的姿态解算。测试中800px焦距只用于合成图，不是本机标定；结果见 `docs/e6_pose_validation.json`。

当前静止标签 `012345` 已通过实机识码与同帧角点验证。E6优先执行定位标签检测，通用码体搜索作为补救；候选ROI归一化对亮/暗像素均进行3×3均值去噪，避免高增益噪声破坏二值化。10秒记录398个处理帧、386个成功帧（97.0%），约39.72处理帧/秒、49.92传感器帧/秒；USB收到390条匹配内容。结果与时序队列起止不同，计数不应逐项相减。该10秒记录为此前版本的现场测试；当前固件最近复测见 `docs/e6_live_validation.json`（3秒处理116帧、113成功，USB匹配117条，快照CRC通过）。

随后15秒连续快照/几何复测：收到564条 `012345`，均为精修角点质量2；结果年龄29.89～67.584ms、中位41.148ms；10次图片请求均收到CRC通过的JPEG，协议错误0。见 `docs/e6_geometry_snapshot_validation.json`。这些是当前单个静止标签/现场光照下的结果，不代表运动、小码或全视场验收。

Windows HS CDC接收需及时读取，并增大接收队列；同时轮询日志的早期测试出现过不完整图像。调整主机接收队列至2MiB并避免阻塞日志读取后，上述10次快照全部通过；`tools/check_e6.py`已设置Windows接收队列。

按钮按住/松开已通过同步电平验证：确认按住时GPIO35=0、GPIO18/19/22=1；确认松开时GPIO35=1、GPIO18=0、GPIO19/22保持1。第二次按下切换补光关闭及灯具实际发光仍待确认。证据：`docs/e6_button_confirmed_held.log`、`docs/e6_button_confirmed_released.log`。软件命令能使GPIO18/19/22读回高/低，但不能证明实际灯亮或电压正确。AW36515曾返回0x0B=0x02（OVP，读后清零）；外补光供电/负载仍需核对。GPIO UART电气通信、本机镜头标定与物理定位精度、运动场景及长时间稳定性仍待现场验证。

固件烧录包与SHA256清单：`dist/e6_20261119/`，已更新为当前COM63实烧版本。CDC复测：`py -3.12 tools/check_e6.py --seconds 10 --expect 012345`；未收到匹配内容时返回非零退出码。

## 按钮/补光联调修正

按板级要求恢复GPIO22高有效外补光门控，AW36515 0x01=0x13（硬件Torch使能、LED1/2开启、Flash禁止）。内补光GPIO19不再因外驱动I2C错误被连带关闭。GPIO35原始电平、30ms消抖及输出状态可通过CDC的 `@BOARD` / `@LIGHT,0..4` 联调，日志从COM63读取。UART控制器按最新引脚定义使用UART0。

曝光调试范围改为自动曝光最大10ms、增益最大15倍，保持50fps传感器模式；这是现场成像联调配置，运动模糊与正式识码曝光上限仍需有码实测。外补光电流码继续为0x10（按规格书公式约32.34mA/通道），不能据此声明实测电流。

VSYS极性已按用户最新确认修正：EN_VSYS/GPIO33低有效，0开启3.3V、1关闭。启动安全锁存为1，上电序列第一步改为0，点灯前确保为0并等待5ms，下电序列最后改为1。此前GPIO33读回高仅证明控制引脚为高，实际对应VSYS关闭，旧诊断记录不可作为供电正常的证据。外补光未连接，先前OVP标志出现在开路负载条件。

低有效VSYS固件已通过COM63烧录，回读GPIO33=0、GPIO18/19=1，见`docs/e6_vsys_active_low_probe.log`。开启后首轮5秒收到78条012345、快照CRC通过；后续3秒无匹配，当前现场光照/标签状态下识码稳定性尚待复测。之前39.7帧/秒数据来自VSYS未开启的旧场景，不作为当前开启补光时的性能结论。

用户已目视确认：VSYS极性修正后，瞄准灯与内补光均正常点亮。不亮问题已定位为EN_VSYS极性错误。现已关闭强制点灯，恢复GPIO35自动控制，回读GPIO33=0、GPIO18/19/22=0、override=-1；记录见`docs/e6_lamps_restore_auto.log`。外补光尚未连接，实物验收待接入后进行。

## 内补光过曝修复与验证

根因已由补光关→开→关对比确认：修复前关灯230帧中229成功，开灯75帧仅1成功；开灯后1/3/5秒图片均值约246、饱和像素约87%，属于持续过曝。实时流水线曾依赖ISP自动曝光，但未调用现有RAW8灰度反馈函数。

现已在E6实时采集路径接入图像反馈曝光/增益调节，停用E6的IPA曝光/增益写入以保持单一控制源。实机日志显示补光开启后增益由约8.7倍降至1倍，约0.8秒进入目标亮度；关闭后也能回升。修复后6秒开灯289个处理帧中286成功，三次图片灰度约65、饱和比例0%；关灯前后仍可识别。前后证据分别为`docs/e6_light_compare.json`和`docs/e6_light_ae_fixed.json`。

最终开补光10秒验证：487个处理帧、483成功；USB收到487条012345且角点均为精修质量2，结果年龄中位15.982ms、最大83.353ms；4次快照CRC全部通过，协议错误0。时序与结果队列起止不同，计数不可逐项相减。详见`docs/e6_illuminated_final_validation.json`。这是当前单个静止标签、距离及光照下的验证，运动与不同材质反光场景仍需实测。结束后已关闭补光并恢复GPIO35自动控制。

双传感器最终验证：两种固件编译通过，各自仅链接所选sensor探测器，UVC尺寸匹配；两种分辨率各815组定位回归通过（合计1630），证据见`docs/dual_sensor_build_validation.json`及`docs/e6_pose_dual_sensor_validation.json`、`docs/ov9281_pose_dual_sensor_validation.json`。本轮固件包与SHA256位于`dist/dual_sensor_20261124/`。

## OV9281实机验证（2026-11-24）

COM62烧录通过esptool哈希校验，实际应用版本为`dual-sensor-20261124-1-g5426e31`。启动日志确认sensor PID=0x9281、1280×720 RAW8/50fps、四MMAP缓冲、首帧921600字节、VDDO_4=3300mV。记录见`docs/ov9281_boot.log`。

首次10秒CDC测试处理192帧、189成功，收到189条`012345`；传感器约49.82fps，处理约19.30fps；快照CRC通过。首次快照所在帧解码失败（102.536ms），后续补光对比的九张快照全部解码成功。记录见`docs/ov9281_live_validation.json`。

内补光关→开→关各6.5秒，处理/成功分别为135/135、136/136、137/136；共407条`012345`，均为精修角点质量2。三个阶段结果年龄中位约69～70ms，最大99.579ms，符合100ms限制。九次同帧JPEG快照CRC全部通过，协议错误0；开灯后三张图片均值约115、饱和像素约8.6%，没有出现持续过曝失码。外补光未接，命令同时控制的GPIO22电平不等于外灯实物验收。证据见`docs/ov9281_lighting_validation.json`和`docs/ov9281_lighting.log`。复测命令：

```powershell
py -3.12 tools/check_e6.py --sensor ov9281 --port COM68 --seconds 10 --expect 012345
py -3.12 tools/check_sensor_lighting.py --sensor ov9281 --port COM68 --log-port COM62 --expect 012345
```

UVC通过VID:PID选择扫码相机，6.281秒收到175帧1280×720图像；见`docs/ov9281_uvc_validation.json`。测试结束关闭强制灯控并恢复GPIO35自动模式，回读override=-1、GPIO33=0、GPIO18/19/22=0。未写入或替换镜头标定，当前OV9281仍提示需要本机内参，物理位姿无效。以上为单个静止标签及当前光照下的结果，未覆盖运动、全视场及长时间稳定性；GPIO35按键逻辑沿用已验证的共用代码，本次未要求用户重新按键。

双传感器固件包`dist/dual_sensor_20261124/`中的OV9281应用已更新为本次实际烧录版本，manifest包含SHA256与实测证据路径；E6固件包保持原已编译版本。
