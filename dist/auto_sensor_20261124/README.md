# AGV SCAN 自动传感器通用固件

一个固件支持E6与OV9281。启动先按AGV板时序上电，再探测I2C地址并核对芯片ID，选择原生尺寸、曝光、解码处理及独立标定命名空间。只在启动时检测，换相机需断电；不支持带电拔插。GPIO、灯控、UART/CDC协议保持共用。

新增GPIO36低有效换头按钮：第一次按下进入持久断电状态（红灯常亮、USB断开）；关闭全部扫码头电源及灯控、I2C高阻，松开后再次按下重启并重新识别扫码头，恢复后补光默认关闭。待换头状态在主控复位/整机重上电后仍保持。GPIO35补光改为AW36515软件Torch模式0x0B，内外补光按次切换，松开保持，GPIO22不再随瞄准松开产生低脉冲。首次物理换头需测量接口VSYS/DOVDD/AVDD/DVDD确认已无残压；本板没有电压反馈。

- 0x10 + 0x56014311：E6，1024×1024 RAW10单lane/50fps；RAW8灰度反馈曝光及E6降噪。
- 0x60 + 0x9281：OV9281，1280×720 RAW8双lane/50fps；ISP/IPA曝光。
- 无设备、同时两个地址应答、ID不符或I2C错误：报错并清理相机供电，禁止继续用错误模式启动。
- UVC在探测后枚举原生分辨率，USB序列号按传感器区分，避免Windows缓存另一分辨率。OV9281当前CDC为COM69，烧录为COM62。
- VDDO_4=3.3V；GPIO33低有效VSYS；补光/瞄准依赖VSYS；GPIO35按钮规则保留。
- E6标定scan_pose，OV9281标定pose_ov9281；未标定不输出有效物理位姿。

本包与COM62实烧二进制相同，SHA256见manifest.json。OV9281已实测；用户选择本轮暂不换回E6，E6仅完成自动选择与定位主机回归。

工程根目录默认编译、烧录：

```powershell
./build_p4.ps1
./flash_p4.ps1 -Port COM62
```

从本包目录烧录（已加载ESP-IDF6环境）：

```powershell
python -m esptool --chip esp32p4 -p COM62 --before default-reset --after hard-reset write-flash "@flash_args"
```

保留NVS；切换传感器不需要重新烧录。
