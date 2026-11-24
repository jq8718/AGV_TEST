# AGV SCAN 双传感器固件 / 2026-11-24

E6原始完整基线：Git标签`e6-baseline-20261124`，提交`a4828d3`。

- `e6/`：OV01C1B，1024×1024 RAW10，单lane，50fps；保留E6补光曝光修复。
- `ov9281/`：OV9281，1280×720 RAW8，双lane，50fps；使用参考驱动及ISP/IPA自动曝光。
- 共用AGV板GPIO16/17 SCCB、GPIO20 sensor使能、GPIO21外部24MHz使能、GPIO33低有效VSYS、GPIO35按钮和原灯控；VDDO_4默认3.3V。
- UVC MJPEG30fps，分辨率随固件匹配；CDC与UART0协议相同，保留精修角点、100ms结果时限和标定命令。
- 标定按传感器分别保存。E6继续使用`scan_pose`，OV9281使用`pose_ov9281`。需本机实测内参，未标定时`pose_valid=0`。

两种固件编译通过；核对各自仅链接相应sensor探测器、UVC尺寸匹配；两种分辨率各815组定位主机回归通过。本轮没有开发板连接，未进行实机烧录或OV9281光学/USB/UART验证；接入前确认模块/转接板与AGV电源、控制及双lane接线相符。

进入所选固件子目录，加载ESP-IDF6环境后烧录（替换实际串口）：

```powershell
$env:PROCESSOR_ARCHITECTURE = 'AMD64'
. C:\Espressif\tools\Microsoft.v6.0.PowerShell_profile.ps1
python -m esptool --chip esp32p4 -p COM63 --before default-reset --after hard-reset write-flash "@flash_args"
```

校验文件见各子目录manifest.json；保留NVS，切换回E6时原标定仍可使用。
