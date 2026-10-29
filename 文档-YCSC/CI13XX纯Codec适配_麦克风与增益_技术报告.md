# CI13XX 纯 Codec 适配技术报告

> 对象工程：`projects/offline_asr_llm_aiot_iis_sample`（芯片 CI1302，板级 `CI-D02GS01J`）
> 目标形态：CI1302 作为一颗"由主控全权控制的音频 Codec"（类比 ES8311），主控为 ESP32，音频走 IIS，控制走 UART。
> 本文汇总本轮全部技术结论、改动记录、实测现象解释与遗留风险，供后续优化和移植到其它芯片时对照。

---

## 1. 结论摘要

| # | 结论 | 影响面 |
|---|---|---|
| 1 | CI1302 只能做 IIS **主机**，主控必须做从机 | 主控侧驱动设计 |
| 2 | 录音、播放、音量、功放全部可由 UART 指令控制，上电默认全关 | 整机状态机 |
| 3 | 麦克风由差分改单端后，AEC 会变差（ERLE 典型降 1~3dB），但可控 | 语音识别率、回声残留 |
| 4 | **模拟 PGA 已到硬件上限 28.5dB，无法再把数字增益挪到模拟侧** | 增益方案 |
| 5 | MICBIAS 恒为 1.7×(AVDD/2)≈2.8V，由片内 LDO 决定，外部改 5V 供电不会改变 | 麦克风供电设计 |
| 6 | 功放使能脚 PC4，固件按低有效实现，实物高有效 → 指令语义取反 | ESP32 侧控制 |

---

## 2. 系统架构

```
                ┌──────────────────────────── CI1302 ────────────────────────────┐
  MIC ──► 内部Codec ADC ──► AEC / NN降噪 / VAD / 唤醒 / ASR ──► IIS0 SDO ────────┼──► 主控 I2S RX
 主控 I2S TX ──► IIS0 SDI ──► 播放任务(16ms/帧) ──► 内部Codec DAC ──► PA(PC4) ──► 喇叭
                └── UART0 @921600 ──► 控制协议（双向）◄── 主控 ──────────────────┘
```

三条独立数据流：

1. **上传流**：MIC → ADC → 算法前处理 → IIS SDO → 主控（16kHz/16bit/立体声，左右声道数据相同）
2. **下行流**：主控 → IIS SDI → 播放任务每 16ms 搬 1024 字节 → DAC → PA → 喇叭
3. **控制流**：UART0，帧头 16 字节（magic `0x5A5AA5A5` + checksum + type + len + version + fill）

完整的 UART 命令表、帧格式、交互时序见 [ESP32对接CI130X_IIS音频工程_适配提示词.md](file:///d:/Projects/CI1302/YCSC-CI13XX_SDK_LLM_AIOT_V3.0.19/%E6%96%87%E6%A1%A3-YCSC/ESP32%E5%AF%B9%E6%8E%A5CI130X_IIS%E9%9F%B3%E9%A2%91%E5%B7%A5%E7%A8%8B_%E9%80%82%E9%85%8D%E6%8F%90%E7%A4%BA%E8%AF%8D.md)。

---

## 3. 配置基线

### 3.1 芯片与板型

| 项 | 值 | 位置 |
|---|---|---|
| 芯片 | CI1302 | `user_config.h` `CI_CHIP_TYPE` |
| 板级 | `USE_CI_D02GS01J_BOARD = 1`，`USE_CI_D06GT01D_BOARD = 0` | [user_config.h](file:///d:/Projects/CI1302/YCSC-CI13XX_SDK_LLM_AIOT_V3.0.19/projects/offline_asr_llm_aiot_iis_sample/app/app_main/user_config.h) |
| 协议串口 | UART0（PB5=TX / PB6=RX），921600 8N1 | `USE_1302_UART0_MODE = 2` 关日志 |
| 音频来源 | 内部 Codec（`AUDIO_IN_FROM_DMIC=0`、`MIC_RECORD_IIS_SELECT=0`） | 同上 |

### 3.2 IIS 规范

| 参数 | 值 |
|---|---|
| 外设 / 主从 | IIS0 / **MASTER**（CI 输出 BCLK、LRCLK、MCLK） |
| 采样率 / 位宽 / 声道 | 16000 Hz / 16 bit / 立体声 |
| BCLK:LRCK | 64（BCLK = 1.024 MHz） |
| MCLK | 256×FS = 4.096 MHz |
| 帧长 | 256 采样点 = 16 ms = 1024 字节（立体声） |

### 3.3 时机模式

| 宏 | 值 | 含义 |
|---|---|---|
| `IIS_UPLOAD_IS_WAKEUP` | 1 | 录音默认关，靠 `SET_AUDIO_START_RECORD(0x011A)` 或唤醒事件开启 |
| `IIS_DOWNLOAD_BY_CMD` | 1 | 播放默认关，靠 `MASTER_PLAY_STATE(0x0220)=1` 开启 |

---

## 4. 麦克风输入路径

### 4.1 代码中的两条分支

配置宏 `MIC_DIFF_SINGLE`（[user_config.h L57](file:///d:/Projects/CI1302/YCSC-CI13XX_SDK_LLM_AIOT_V3.0.19/projects/offline_asr_llm_aiot_iis_sample/app/app_main/user_config.h#L57)）同时控制三件事，均在 [CI-D02GS01J.c](file:///d:/Projects/CI1302/YCSC-CI13XX_SDK_LLM_AIOT_V3.0.19/driver/boards/CI-D02GS01J.c#L420-L465)：

| 项 | 差分（0） | 单端（1） |
|---|---|---|
| `codec_adc_input_mode_l` | `INNER_CODEC_INPUT_MODE_DIFF` | `INNER_CODEC_INPUT_MODE_SINGGLE_ENDED` |
| `dig_gain_l` | 1.0 | **6.0** |
| 左声道 ADC 参考 | MIC_L / MIC_N 差分 | MIC_L 对 GND |

右声道（REF 回采）独立由 `REF_DIFF_SINGLE` 控制，本工程为 **1（单端）**，`dig_gain_r = 6.0f`，不受 `MIC_DIFF_SINGLE` 影响（`USE_AEC_MODULE=1` 时走 `REF_DIFF_SINGLE` 分支）。

### 4.2 本轮改动

```c
// user_config.h L57
#define MIC_DIFF_SINGLE   1   // 0 → 1
```

改动后自动生效：ADC 输入切单端、`dig_gain_l` 由 1.0 提升到 6.0（补回单端损失的 6dB）。板级文件无需改动。

### 4.3 单端接线（ZST6216 类模拟 MEMS）

- ZST6216 `VDD` → 3.3V，`GND` → 板级地
- `OUT` → 隔直电容（约 1uF）→ MIC_L
- MIC_N → codec AGND，**就近单点接地**
- 板上原差分麦克风必须断开，不可并联
- 走线短、包地，远离 PA 输出与电源电感

### 4.4 参考通路

`REF_IN_FROM_INNER_CODEC = 0`（[sdk_default_config.h](file:///d:/Projects/CI1302/YCSC-CI13XX_SDK_LLM_AIOT_V3.0.19/system/sdk_default_config.h#L728)），即 AEC 的参考（REF）不是内部数字回环，而是**外部模拟右声道回采**，本身已是单端。AEC 的两路输入都受模拟前端质量影响。

---

## 5. 增益链路与硬上限

完整链路：**MIC AMP（模拟）→ PGA（模拟）→ ADC → 数字增益**。

| 级 | 当前值 | 可调范围 | 代码位置 |
|---|---|---|---|
| MIC AMP | 12 dB | 12 / 16 / 20 dB | `codec_adc_mic_amp_l`，[CI-D02GS01J.c L439](file:///d:/Projects/CI1302/YCSC-CI13XX_SDK_LLM_AIOT_V3.0.19/driver/boards/CI-D02GS01J.c#L439) |
| PGA | 28.5 dB | -18 ~ +28.5 dB，步进 1.5 dB | `pga_gain_l`，[CI-D02GS01J.c L445](file:///d:/Projects/CI1302/YCSC-CI13XX_SDK_LLM_AIOT_V3.0.19/driver/boards/CI-D02GS01J.c#L445) |
| 数字增益 | 6 dB（单端） | -98.5 ~ +28.5 dB，步进 0.5 dB | `dig_gain_l`，[CI-D02GS01J.c L451](file:///d:/Projects/CI1302/YCSC-CI13XX_SDK_LLM_AIOT_V3.0.19/driver/boards/CI-D02GS01J.c#L451) |

**关键结论：PGA 已满档，无法再加 6dB。**

[ci130x_codec.c L34-L48](file:///d:/Projects/CI1302/YCSC-CI13XX_SDK_LLM_AIOT_V3.0.19/driver/ci130x_chip_driver/src/ci130x_codec.c#L34-L48) 中 `get_pga_gain_reg_data_from_db()`：

```c
if (28 < gain_db) return 0x1f;          // 超过 28dB 直接钳到最大档
uint8_t reg = (uint8_t)((gain_db + 18) / 1.5f) & 0x1f;
```

`pga_gain_l = 28.5f` 传入后被截断为 28，寄存器值 `(28+18)/1.5 = 30`（0x1E），即 27dB，已是可用的最高档。这解释了为什么板级代码为单端模块固定使用 `dig_gain_l = 6.0f`——模拟侧没有余量。

数字增益换算（[ci130x_codec.c L252-L268](file:///d:/Projects/CI1302/YCSC-CI13XX_SDK_LLM_AIOT_V3.0.19/driver/ci130x_chip_driver/src/ci130x_codec.c#L252-L268)）：`reg = gain × 2 + 197`，每 LSB 0.5 dB，197 为 0 dB。

**若实测灵敏度不足，唯一可用的模拟手段**：`codec_adc_mic_amp_l` 提到 `INNER_CODEC_MIC_AMP_16dB` 或 `20dB`，并把 `dig_gain_l` 相应下调，避免总增益过高削顶。需实测验证。

> 注：启用 `AUTO_GET_AEC_GAIN` 时 PGA 被固定为 `20.0f`（L442），与上表不同；本工程未启用该宏。

---

## 6. AEC 影响评估

改单端对 AEC 的影响分三层：

**① 信号幅度与 SNR（可恢复但打折）**
差分比单端多 6dB 摆幅。数字增益补回了幅度，但数字增益位于 ADC 之后，会一并放大量化噪声与前级底噪，SNR 净损失约 3~6dB。

**② 共模抑制能力丧失（主因）**
AEC 假设麦克风信号是参考信号经线性声学路径的产物。差分输入靠 CMRR（通常 40dB 以上）抑制 PA 开关瞬态、电源纹波、地弹；单端后 CMRR≈0，这些干扰直接进入 ADC。它们与 REF 非线性相关，自适应滤波器消不掉，表现为 ERLE 下降、静音段底噪抬升。

**③ 参考通路本就单端（唯一正向因素）**
REF 已是单端（`REF_DIFF_SINGLE=1`）。MIC 也改单端后两路前端结构一致，AEC 增益标定的一致性更好，但抵不过 ①②。

**量化预期**：单麦 + 单喇叭 + AB 类功放 + 中等音量 + 短走线，ERLE 典型下降 1~3dB，主观为"回声尾巴略长、静音段底噪略大"。D 类功放 / 大音量 / 长走线场景劣化更明显。

**缓解措施（按性价比）**
1. MIC_N 就近单点接地到 codec AGND；MIC 走线短、包地、远离开关功放与电源
2. 如需提增益，加到 MIC AMP（模拟）而非数字增益
3. 改动后重跑 AEC 标定，不沿用差分参数
4. 验收：80% 音量循环播放，观察 AEC 后残留回声与双讲表现

---

## 7. MICBIAS

**结论：MICBIAS = 1.7 × (AVDD/2) ≈ 2.8V，与外部供电电压无关。**

- 由 `CODEC->reg23` bit[2:0] 选择，8 档（1.0 ~ 1.7 × AVDD/2），见 [ci130x_codec.h L582-L589](file:///d:/Projects/CI1302/YCSC-CI13XX_SDK_LLM_AIOT_V3.0.19/driver/ci130x_chip_driver/inc/ci130x_codec.h#L582-L589)
- 固件在 `inner_codec_adc_enable()` 中写死最高档：[ci130x_codec.c L209](file:///d:/Projects/CI1302/YCSC-CI13XX_SDK_LLM_AIOT_V3.0.19/driver/ci130x_chip_driver/src/ci130x_codec.c#L209) `CODEC->reg23 |= ((0x07<<0) | (0x07<<3));`
- 全 SDK 无任何 `inner_codec_micbias_set()` 调用，故恒为 1.7 档

| 档位 | 系数 | AVDD=3.3V 时 |
|---|---|---|
| 0 | 1.0 | 1.65 V |
| 3 | 1.3 | 2.15 V |
| 5 | 1.5 | 2.48 V |
| 7（当前） | 1.7 | **2.80 V** |

**实测 2.8V 反推 AVDD ≈ 3.3V**，说明 codec 模拟域由片内 LDO 供电。外部从 3.3V 改 5V 供电只是改变 LDO 输入，模拟域电压不变，因此 MICBIAS 不变。片内 LDO 档位由出厂 efuse trim 固化（`load_ldo_trim_value()`，[ci130x_init.c L88-L145](file:///d:/Projects/CI1302/YCSC-CI13XX_SDK_LLM_AIOT_V3.0.19/startup/ci130x_init.c#L88-L145)），软件无法提高。

**用途提醒**：MICBIAS 主要供驻极体电容麦（ECM）偏置，典型驱动能力约 1mA 量级。模拟 MEMS 麦（ZST6216）应接 3.3V VDD 供电，MICBIAS 可悬空。

**若要调档**：必须在 `inner_codec_adc_enable()` 之后调用 `inner_codec_micbias_set()`，否则会被 `|= 0x07` 覆盖。

---

## 8. 功放（PA）控制

| 项 | 值 |
|---|---|
| 引脚 | **PC4**（`gpio_set_output_*_level(PC, pin_4)`） |
| 固件实现 | 低电平有效：`power_amplifier_on()` 输出低 |
| 实物硬件 | 高电平有效 |
| 上电默认 | `PLAYER_CONTROL_PA=0` → 执行 `power_amplifier_on()` → PC4 低 → **功放关** |
| 控制指令 | `SET_VOLUME_MUTE_STATE(0x0119)`，payload 1=开 / 0=关（语义相对"静音"取反） |

ESP32 侧对照表：

| 目标动作 | 发送 | 固件执行 | PC4 | 实际功放 |
|---|---|---|---|---|
| 开 | `0x0119 = 1` | `power_amplifier_off()` | 高 | 开 |
| 关 | `0x0119 = 0` | `power_amplifier_on()` | 低 | 关 |

推荐时序：开功放 → 等应答 → 20ms → `0x0220=1` → 灌 PCM；停止时 `0x0220=0` → 等应答 → 50ms → 关功放。

PA 不随本地播放自动开关（`PLAYER_CONTROL_PA=0`，播放只控 DAC），常开会持续耗电，建议由主控与音频流同步开关。

---

## 9. 分区表修复记录

**症状**：编译打包报 `分区User: 文件过大，超出预留大小！实际大小 0x29F58 预留大小 0x24000`，固件无法烧录。

**原因**：工程开启的算法模块体积增长，原 `config.ini` 预留不足；另有第二处隐藏溢出 `user_file_size 0x1000 < 实际 0x1647`。

**处理**：重新分配 DNN / Voice 冗余空间，最终布局（CI1302，2MB flash，NV 固定 `0x1FC000`）：

| 分区 | addr | size |
|---|---|---|
| user | 0x000000 | 0x30000 |
| code1 | 0x030000 | 0x30000 |
| command | 0x034000 | 0xF000 |
| module | 0x043000 | 0x128000 |
| voice | 0x16B000 | 0x40000 |
| user_file | 0x1AB000 | 0x2000 |
| NV | 0x1FC000 | — |

关键认知：`config.ini` 中真正决定排布的是**各分区预留大小**，起始地址是计算结果；所有分区结束地址不得超过 NV 起始地址。

---

## 10. 验证清单

| # | 项目 | 方法 | 状态 |
|---|---|---|---|
| 1 | 单端 MIC 出音正常 | 录一段语音看 IIS 上传数据幅度 | 待验证 |
| 2 | 唤醒词识别率无明显下降 | 静音环境 20 次唤醒统计 | 待验证 |
| 3 | AEC 残留回声可接受 | 80% 音量循环播放 TTS，观察录音 | 待验证 |
| 4 | 静音段底噪 | 关闭播放，读 IIS 上传本底 | 待验证 |
| 5 | PA 开关与音频同步 | 示波器看 PC4 与 IIS SDI 时序 | 待验证 |
| 6 | UART 指令应答 | 每条指令收到 `0x0801` | 待验证 |
| 7 | 分区无溢出 | 打包日志无"超出预留大小" | 已解决 |

---

## 11. 移植到其它芯片 / 主控的对照方法

换芯片（CI1306、CI1313 等）或换主控时，按以下维度逐项对照：

| 维度 | 需要确认的内容 | 常见差异 |
|---|---|---|
| 引脚 | IIS 五线（SDO/SDI/BCLK/LRCLK/MCLK）+ 协议串口 + PA 脚 | CI1306 用 UART1（PB7/PC0），CI1302 只能 UART0 |
| IIS 主从 | 芯片是否支持从机 | CI13XX 仅主机 |
| 音频格式 | 采样率、位宽、声道、BCLK:LRCK、MCLK 倍频 | 固定 16k/16bit/立体声 |
| 麦克风 | 单端 / 差分、REF 通路来源（内部回环 or 外部回采） | `MIC_DIFF_SINGLE`、`REF_DIFF_SINGLE`、`REF_IN_FROM_INNER_CODEC` |
| 增益 | MIC AMP 档位、PGA 范围、数字增益范围 | 决定信噪比上限 |
| 偏置 | MICBIAS 是否由内部 LDO 决定 | 外部改供电无效 |
| 功放 | 控制引脚、有效电平、是否随播放自动开关 | `PLAYER_CONTROL_PA`、`GS0XJ_BOARD_PA_AUTO` |
| 分区 | flash 容量、NV 固定地址、各模块预留 | 决定能否烧录 |
| 协议 | 帧头格式、命令字、应答机制 | 版本号与 fill 字段 |

**通用排查顺序**：① 能否打包烧录（分区）→ ② 上电是否收到 `SYS_READY` → ③ IIS 是否有正确时钟 → ④ 录音通道数据幅度是否合理 → ⑤ 播放通道是否出声 → ⑥ AEC/唤醒指标。

---

## 12. 关键文件索引

| 文件 | 作用 |
|---|---|
| [user_config.h](file:///d:/Projects/CI1302/YCSC-CI13XX_SDK_LLM_AIOT_V3.0.19/projects/offline_asr_llm_aiot_iis_sample/app/app_main/user_config.h) | 芯片/板型/串口/麦克风/时机宏 |
| [CI-D02GS01J.c](file:///d:/Projects/CI1302/YCSC-CI13XX_SDK_LLM_AIOT_V3.0.19/driver/boards/CI-D02GS01J.c) | 引脚复用、IIS 配置、codec 增益、PA 引脚 |
| [cias_audio_data_handle.c](file:///d:/Projects/CI1302/YCSC-CI13XX_SDK_LLM_AIOT_V3.0.19/projects/offline_asr_llm_aiot_iis_sample/app/app_audio_handle/cias_audio_data_handle.c) | 播放任务、指令处理 |
| [ci130x_codec.c](file:///d:/Projects/CI1302/YCSC-CI13XX_SDK_LLM_AIOT_V3.0.19/driver/ci130x_chip_driver/src/ci130x_codec.c) | PGA / 数字增益 / MICBIAS / ADC 使能 |
| [ci130x_codec.h](file:///d:/Projects/CI1302/YCSC-CI13XX_SDK_LLM_AIOT_V3.0.19/driver/ci130x_chip_driver/inc/ci130x_codec.h) | 增益与偏置枚举定义 |
| [ci130x_init.c](file:///d:/Projects/CI1302/YCSC-CI13XX_SDK_LLM_AIOT_V3.0.19/startup/ci130x_init.c) | LDO trim 加载 |
| [sdk_default_config.h](file:///d:/Projects/CI1302/YCSC-CI13XX_SDK_LLM_AIOT_V3.0.19/system/sdk_default_config.h) | SDK 默认宏 |
| [config.ini](file:///d:/Projects/CI1302/YCSC-CI13XX_SDK_LLM_AIOT_V3.0.19/projects/offline_asr_llm_aiot_iis_sample/firmware/config.ini) | 分区与打包参数 |
| [ESP32对接CI130X_IIS音频工程_适配提示词.md](file:///d:/Projects/CI1302/YCSC-CI13XX_SDK_LLM_AIOT_V3.0.19/%E6%96%87%E6%A1%A3-YCSC/ESP32%E5%AF%B9%E6%8E%A5CI130X_IIS%E9%9F%B3%E9%A2%91%E5%B7%A5%E7%A8%8B_%E9%80%82%E9%85%8D%E6%8F%90%E7%A4%BA%E8%AF%8D.md) | 给主控侧 AI 的完整对接提示词 |
