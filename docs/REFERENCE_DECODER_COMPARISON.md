# 与 ESP32P4_XCF_OV9281 的解码流程对齐

参考目录：D:/XC/2026/P4SCAN/ESP32P4_XCF_OV9281。配置基准采用其当前 sdkconfig/build（1280×720、50fps、双核分阶段流水线），不采用历史 sdkconfig.idf60 的640×400模式。

## 对比结论与本次修改

修改前不能声称解码源码完全一致：xcr_dmtx.c包含E6专用分支（跳过码体恢复优先路径、ROI亮暗模块均做3×3滤波）；OV9281运行时原本进入参考分支，但通用源码存在差异。本次已用参考原文件恢复xcr_dmtx.c及组件CMakeLists.txt；移除E6解码核心特判，两种传感器共用参考算法。同时删除scan_pose.c中重复的标定有效性检查，不改变姿态算法。

| 检查范围 | 结果 |
| --- | --- |
| 解码组件101个C/C++、头文件、汇编、构建文件 | 与参考一致，逐项SHA256记录在JSON中；比较时允许换行符差异 |
| 取首帧、begin_capture、acquire、release及latest_frame缓冲池 | 函数源码一致 |
| Core0前段搜索/准备 | 除E6成像曝光反馈适配外，capture_task一致 |
| Core1后段解码、同帧角点精修、时限判断及结果发布 | finish_decoded_slot与decode_task源码一致 |
| XCR、OV9281模式及实时流水线配置 | 检查项全部匹配参考当前sdkconfig |
| 标定后的物理位姿计算 | 算法沿用参考；本机内参与NVS独立管理 |

实际链路：四块CSI MMAP缓冲 → 最新帧选择/100ms时限 → Core0调用参考前段码体恢复与标记搜索 → Core1后段解码/纠错 → 同帧角点精修 → 新鲜度/几何有效性过滤 → UART/CDC结果及同帧JPEG/UVC预览。不增加上一帧ROI。

## 保留的硬件与平台适配

此结论针对解码算法与流程，整个工程不是参考目录的逐文件镜像：AGV GPIO16/17 I2C、GPIO33低有效VSYS、GPIO35补光、GPIO36持久断电换头、传感器启动自动识别、动态原生UVC尺寸及UART0 GPIO43/42仍按本板运行。E6保留其传感器曝光反馈与ISP降噪，输入图像分辨率随型号变化。后续按用户要求，OV9281默认使用参考工程工厂内参，本机保存的标定优先；E6仍需自己的内参，两型号独立保存。角度、距离及串口验证见[位姿与串口对比](REFERENCE_POSE_UART_COMPARISON.md)。

平台仍为IDF6、P4v3.2、CPU400MHz，参考当前sdkconfig CPU360MHz；相同算法不表示两块硬件、输入图像、曝光或处理速度完全相同。参考历史性能不作为本机性能结论。

E6解码核心本次改为严格参考算法，因此此前E6专用ROI去噪与搜索顺序不再启用；当前仅有OV9281接入，E6光学识码仍需换机复验。

## 验证

- `py -3.12 tools/check_reference_decoder.py`：101文件、八项阶段/缓冲检查及配置比对通过，见reference_decoder_alignment.json。
- 参考原版C/C++解码链路主机回放：九张OV9281快照转Gray8全部识别012345、角点质量2；黑/灰/白三张负样本均不误识。见reference_decoder_replay_validation.json。JPEG重建输入不是原始传感器RAW，主机时间不是P4时间。
- IDF6通用固件编译及COM62烧录哈希校验通过；启动自动识别0x60、PID0x9281、1280×720、四缓冲及921600字节首帧。见reference_aligned_ov9281_boot.log。
- 当前静止标签10秒实测：214处理帧、214成功、214条012345，传感器约50.12fps、处理约21.40fps；快照CRC通过、协议错误0。见reference_aligned_ov9281_live_validation.json。这是当前标签与光照下结果，不代表全视场/运动场景。

通用固件包dist/auto_sensor_20261124已更新为本次COM62实际烧录文件，清单记录SHA256。GPIO35/36实现本次未修改。
