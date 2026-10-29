---
name: ci13xx-audio-codec-adapter
description: Tune CI13XX audio projects as a pure codec for a host MCU — mic input mode, gain limits, MICBIAS, PA pin, AEC impact, partitions. Use when adapting CI1302 or CI1306 to ESP32 or another chip.
---

# CI13XX Audio Codec Adapter

把 CI1302 / CI1306 当作纯音频 Codec 交给主控（ESP32 等），或把本工程移植到其它芯片时，按本文执行。所有结论均已在 `YCSC-CI13XX_SDK_LLM_AIOT_V3.0.19` 源码中核实。

## 一、先定位配置，不要直接改 SDK 默认值

按优先级从高到低：

1. 项目层 `projects/<proj>/app/app_main/user_config.h` — 板型、芯片型号、协议串口、麦克风模式、音频时机宏
2. 板级 `driver/boards/CI-D02GS01J.c`（CI1302）或 `CI-D06GT01D.c`（CI1306）— 引脚复用、IIS 参数、codec 增益、PA 引脚
3. SDK 默认 `system/sdk_default_config.h` — 兜底默认值

改任何宏前，先确认项目层是否已覆盖；只改项目层。

## 二、硬约束（不要试图突破）

| 项 | 边界 | 说明 |
|---|---|---|
| IIS 主从 | 只能做 MASTER | 主控必须配 SLAVE，时钟由 CI 输出 |
| 音频格式 | 16kHz / 16bit / 立体声 / BCLK:LRCK=64 / MCLK=256FS | 左右声道数据相同 |
| PGA | -18 ~ +28.5 dB，步进 1.5 dB | `>28` 被钳到 `0x1f`；28.5 已满档，无法再加 |
| 数字增益 | -98.5 ~ +28.5 dB，步进 0.5 dB | `reg = gain*2 + 197`，位于 ADC 之后 |
| MIC AMP | 12 / 16 / 20 dB | 唯一还有余量的模拟增益 |
| MICBIAS | 1.7 × (AVDD/2) ≈ 2.8V | 片内 LDO 决定，外部改 5V 供电无效 |
| PA 引脚 | PC4，固件低有效，实物高有效 | 主控侧语义取反 |
| Flash 分区 | 结束地址 ≤ NV `0x1FC000`（2MB） | `config.ini` 填的是预留大小，地址是结果 |

## 三、标准操作

### 1. 麦克风差分改单端

1. `user_config.h` 中 `MIC_DIFF_SINGLE` 由 0 改 1。
2. 板级文件不用改：`codec_adc_input_mode_l` 自动切 `SINGGLE_ENDED`，`dig_gain_l` 自动 1.0 → 6.0。
3. 硬件：MEMS `OUT` 串约 1uF 隔直电容接 MIC_L，MIC_N 就近单点接地到 AGND，断开板上原麦克风。
4. 预期代价：AEC 的 ERLE 下降 1~3 dB，静音段底噪抬升；差分共模抑制优势丧失是主因，数字增益补幅不补 SNR。
5. `REF_DIFF_SINGLE` 独立控制参考通道，通常保持 1，不要跟着改。

### 2. 调整录音增益

1. 优先动 `codec_adc_mic_amp_l`（模拟，12/16/20 dB），并把 `dig_gain_l` 相应下调。
2. 不要再尝试提高 `pga_gain_l`，已满档。
3. `AUTO_GET_AEC_GAIN` 启用时 PGA 被固定为 20.0f，改动前先确认该宏状态。
4. 改完重跑 AEC 标定。

### 3. 功放极性 / 开关控制

1. 首选主控侧取反，不改固件：`SET_VOLUME_MUTE_STATE(0x0119)` 发 1 = 开功放，发 0 = 关功放。
2. 必须改固件时才动 `GS0XJ_BOARD_PA_AUTO` / `PLAYER_CONTROL_PA`。
3. `PLAYER_CONTROL_PA=0` 时 PA 不随播放自动开关，常开耗电，需主控与音频流同步开关：开 PA → 20ms → `0x0220=1`；`0x0220=0` → 50ms → 关 PA。

### 4. 分区溢出

1. 打包日志出现"超出预留大小"即溢出。
2. 调整 `firmware/config.ini` 的 size，并按顺序顺移后续 addr，总长不超过 NV 起始地址。
3. 不要只查大分区，`user_file_size` 这类小分区同样可能溢出。

## 四、验证顺序

打包成功 → 收到 `CIAS_AUDIO_SYS_READY(0x0601)` → IIS 时钟正常 → 录音数据幅度合理 → 播放出声 → AEC 与唤醒指标 → PA 时序。

## 五、参考

- [references/porting-matrix.md](references/porting-matrix.md)：移植到其它芯片 / 主控的对照维度与排查树
- 项目内完整报告：`文档-YCSC/CI13XX纯Codec适配_麦克风与增益_技术报告.md`
- 主控侧对接提示词：`文档-YCSC/ESP32对接CI130X_IIS音频工程_适配提示词.md`
