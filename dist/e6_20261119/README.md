# E6固件 / 2026-11-19

已通过COM63烧录并验证E6芯片ID、CSI首帧、CDC图片和UVC预览；已知标签012345已实机识别，当前静止场景约39.7处理帧/秒；15秒收到564条精修角点结果，10次快照CRC通过。按钮按住/松开输入及灯控引脚电平已验证，第二次按下切换和灯具实际发光待确认，AW曾出现OVP故障标志。物理定位须完成本机E6标定，内参未标定时姿态无效。

在本机PowerShell加载ESP-IDF6环境，进入此目录：

```powershell
$env:PROCESSOR_ARCHITECTURE = 'AMD64'
. C:\Espressif\tools\Microsoft.v6.0.PowerShell_profile.ps1
python -m esptool --chip esp32p4 -p COM63 --before default-reset --after hard-reset write-flash "@flash_args"
```

这是16MB Flash、P4 v3.1及以上版本的完整三文件烧录包。使用manifest.json校验文件。

最新修正：GPIO33/EN_VSYS低有效，0开启3.3V、1关闭。启动/点灯/下电均已修正，COM63烧录和GPIO33=0回读通过。灯具实物待用户确认；此前39.7帧/秒为VSYS关闭时的旧现场结果，不作为新补光条件下性能结论。

用户已目视确认：VSYS极性修正后，瞄准灯与内补光均正常点亮。不亮问题已定位为EN_VSYS极性错误。现已关闭强制点灯，恢复GPIO35自动控制，回读GPIO33=0、GPIO18/19/22=0、override=-1；记录见`docs/e6_lamps_restore_auto.log`。外补光尚未连接，实物验收待接入后进行。

VDD_IO_5连接VDDO_4，GPIO39～48默认3.3V。BOARD_VDDO4_MV=3300；已实烧并读取LDO状态表确认通道4配置3300mV。

当前版本已修复内补光持续过曝：E6使用RAW8灰度反馈控制曝光/增益。开补光10秒487处理帧、483成功；收到487条012345及精修角点，4次快照CRC通过。灯光切换约1秒收敛。已恢复GPIO35自动控制。
