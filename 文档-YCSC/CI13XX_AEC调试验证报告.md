# CI13XX AEC 调试验证报告

> 适用场景：ESP32 通过 IIS 发送音频给 CI1302，CI1302 经内部功放（PA）输出到喇叭，验证回声消除（AEC）是否有效及效果好坏。
> 适用工程：`projects/offline_asr_llm_aiot_iis_sample`（主）/ `projects/offline_asr_llm_aiot_uart_sample`（配置相同）
> 适用硬件：CI1302 + CI-D02GS01J 板型（自制板需确认带回采电路，见 §6）
> 报告日期：2026-11-04　SDK 版本：YCSC-CI13XX_SDK_LLM_AIOT_V3.0.19

---

## 1. AEC 工作原理与信号路径（调试前必读）

AEC 的参考信号**不是**从播放数据里截取的，而是从**功放输出端模拟回采**的。因此参考信号跟随喇叭实际输出，与音频来源（内部 TTS / UART 播放 / ESP32 IIS 推流）无关——ESP32 通过 IIS 推流播放时 AEC 同样有效。

```text
                       ┌─────────────────────── CI1302 ───────────────────────┐
 ESP32(IIS从机) ──IIS0──▶ IIS0接收 → 解码DAC → 功放PA ────▶ 喇叭
                       │                            │
                       │                            │ 分压回采电路(板上)
                       │                            ▼
   麦克风 ◀──声学回声── 喇叭               右通道ADC = REF(参考信号)
      │                                    │
      ▼                                    │
   左通道ADC = MIC ────────────────────────┴──▶ AEC 自适应对消 → 干净人声 → 识别
                       └──────────────────────────────────────────────────────┘
```

关键结论：

| 结论 | 说明 |
|---|---|
| 参考采自 PA 输出 | 板上必须有"PA 输出 → 电阻分压 → 内部 codec 右通道 ADC"回采电路，官方 CI-D02GS01J 模组自带 |
| 音量调节自动跟随 | 参考采在音量级之后，数字调音量（如 `audio_play_set_vol_gain`）时参考幅度同步变化，AEC 自适应可覆盖 |
| 只在播报状态工作 | `aec_control_mode = ENABLE_PLAYING_STATE_MODE`，不播报时 AEC 不做消回声处理（正常现象，测试必须在播放中进行） |

---

## 2. 在哪里调试（调试位置总览）

### 2.1 配置层次（改配置按此优先级，只改项目层）

| 优先级 | 文件 | 调什么 |
|---|---|---|
| 1 | `projects/<工程>/app/app_main/user_config.h` | 麦克风/参考模式、调试采音开关、串口配置 |
| 1 | `projects/<工程>/app/app_main/ci_ssp_config.c` | AEC 算法参数、采音输出通道选择 |
| 2 | `driver/boards/CI-D02GS01J.c` | REF 回采通道的 codec 增益、输入模式 |
| 3 | `system/user_alg_config.mk` | 算法编译开关（USE_AEC 等），一般不动 |

### 2.2 调试采音数据流（验证 AEC 的核心通道）

```text
SSP算法框架 ──▶ alg_preprocess.c: 选两个通道打包
                  ├── 左通道 = MICL  （AEC 前的原始麦克风数据）
                  └── 右通道 = DST1  （AEC+降噪+NLP 整链处理后的输出）
                        │
          ┌─────────────┴──────────────┐
          ▼                            ▼
  DEBUG_AUDIO_UART_UPLOAD_EN=1    AUDIO_DATA_UPLOAD_BY_IIS=1
  UART0 @921600bps 输出 PCM       经 IIS 回传给 ESP32 侧录音
```

两个 sample 的通道配置一致（左=MICL，右=DST1），因此**一次录音同时包含 AEC 前后两路数据**，无需烧录两版固件对比。

---

## 3. 怎么调试（分阶段操作步骤）

### 阶段 0：静态确认（约 10 分钟，不改代码）

1. 确认 AEC 已编译进固件：`system/user_alg_config.mk` 中 `USE_AEC := 1`（当前工程已配置）。
2. 确认 iis_sample 的 makefile 已强制 `REF_IN_FROM_INNER_CODEC=1`（makefile 第 50 行写死），即参考走内部 codec 回采。
3. 确认板级：CI-D02GS01J 官方模组自带回采电路；自制板用示波器/万用表确认 PA 输出到 codec 右通道输入脚之间有分压网络。

### 阶段 1：打开调试采音

1. 修改 `projects/offline_asr_llm_aiot_iis_sample/app/app_main/user_config.h`：
   ```c
   #define DEBUG_AUDIO_UART_UPLOAD_EN  1   // 原值 0（第 88 行）
   ```
2. 重新编译烧录。
3. 数据格式：UART0、波特率 921600、16kHz / 16bit / 双声道交织 PCM、无文件头。
4. **注意**：必须先打开串口录音工具再上电，避免数据错帧（源码注释明确要求）。
5. 备选路径：不开串口，用 `AUDIO_DATA_UPLOAD_BY_IIS=1`（iis_sample 默认已开）把同样的双声道数据经 IIS 回传，由 ESP32 侧录音保存，通道含义相同。

### 阶段 2：Audacity 分析设置

文件 → 导入 → 原始数据：编码选 **16bit PCM**，采样率 **16000Hz**，声道数 **2**，字节序默认。导入后左声道=MIC 原始信号，右声道=AEC 后信号。

### 阶段 3：录制三段数据

| 段 | 场景 | 目的 |
|---|---|---|
| A | 不播放、不说话 | 环境底噪基准 |
| B | ESP32 推流播放音乐，**人不说话**（10 秒以上） | **AEC 有效性的核心判据** |
| C | 播放音乐 + 正常喊唤醒词/指令 | 业务表现 |

### 阶段 4：按段 B 判定（判定标准表）

| 段 B 现象（左=MIC 原始，右=AEC 后） | 结论 | 处理 |
|---|---|---|
| 左通道有明显回声波形，右通道显著更小 | **AEC 有效**，按下面公式量化 | — |
| 左通道几乎无波形 | 回声根本没进麦：播放没出声或 mic 链路断 | 先查硬件 |
| 左右波形一样大 | AEC 没在工作 | 见 §6 排查树第 3 步 |
| 右通道仍能听出音乐旋律 | AEC 在跑但抑制量不足 | 见 §6 排查树第 2 步 |

量化指标（在段 B 上测，选中播放区间读 RMS）：

```text
ERLE = 20 × log10( RMS左声道 / RMS右声道 )

ERLE ≥ 10 dB  → AEC 有效
ERLE ≥ 15 dB  → 良好
ERLE < 6 dB   → 效果不足，需要调增益/查参考
```

说明：右通道 DST1 是整条前处理链（AEC+降噪+NLP）之后的输出；但段 B 中不存在人声，残余基本就是 AEC 的抑制效果。

### 阶段 5：直接观察参考信号（诊断利器）

修改 `projects/offline_asr_llm_aiot_iis_sample/app/app_main/ci_ssp_config.c` 第 37 行：

```c
.iis_right_channel = REFR,   // 原值 DST1，临时改为 REFR 直接看参考通道
```

重新采音，播放时右通道应出现**与播放内容一致**的波形：

- **有波形** → 参考正常，问题在增益匹配（`alc_off_codec_adc_gain_ref=4` 与板上分压比不匹配）
- **没波形** → 回采电路没信号，AEC 必然无效，查硬件（分压电阻、焊接、通道使能）

调试完改回 `DST1`。

### 阶段 6：业务指标验收

1. ESP32 以最大音量播 TTS/音乐（最恶劣场景）。
2. 距离 0.5m / 1m 各喊唤醒词 20 次。
3. 统计播报中唤醒成功率，与安静环境对比。

| 结果 | 结论 |
|---|---|
| 降幅 < 10% | 验收通过 |
| 几乎唤不醒（安静时正常） | AEC 无效或未工作 |
| 能唤醒但识别错字多 | ERLE 不足，残余回声干扰识别 |

---

## 4. AEC 无效排查树

```text
播放中 AEC 无效
├── 1. REF 通道有信号吗？（阶段5 改 REFR 观察）
│     ├── 没有 → 回采电路问题：分压网络/焊接/REF_DIFF_SINGLE 与硬件不符
│     └── 有 → 下一步
├── 2. ERLE 低（>6dB 但不达标）
│     ├── alc_off_codec_adc_gain_ref=4 与实际分压比不匹配 → 核对/调整
│     ├── alc_off_codec_adc_gain_mic=20 与 mic 灵敏度不匹配 → 调整
│     ├── 音量过大导致功放/喇叭失真 → 非线性回声 AEC 处理不了，降音量验证
│     └── 改过麦克风接线（差分↔单端）→ REF_DIFF_SINGLE/MIC_DIFF_SINGLE 需与硬件一致
└── 3. AEC 完全没工作
      ├── USE_AEC 是否=1（user_alg_config.mk）→ 重编译
      ├── aec_config.alg_enable 是否=true
      └── 当前是否处于播报状态（ENABLE_PLAYING_STATE_MODE 下不播报不消回声）
```

---

## 5. 关键代码索引（具体的代码在哪里）

以下路径均相对 SDK 根目录 `YCSC-CI13XX_SDK_LLM_AIOT_V3.0.19/`。

### 5.1 AEC 算法配置

| 位置 | 内容 |
|---|---|
| `system/user_alg_config.mk:7` | `USE_AEC := 1`，AEC 算法编译总开关 |
| `projects/offline_asr_llm_aiot_iis_sample/project_file/makefile:24-26` | `USE_AEC=1` 时追加 `-DUSE_AEC_MODULE=1` 等编译宏 |
| `projects/offline_asr_llm_aiot_iis_sample/project_file/makefile:50` | 强制 `-DREF_IN_FROM_INNER_CODEC=1`（参考走内部 codec） |
| `projects/offline_asr_llm_aiot_iis_sample/app/app_main/ci_ssp_config.c:221-241` | `aec_config`：AEC 全部算法参数（见 §7 参数表） |
| `projects/offline_asr_llm_aiot_iis_sample/app/app_main/ci_ssp_config.c:292-296` | `ci_ssp` 结构体中 `.aec` 模块注册（module_config 为 NULL 即关闭） |

### 5.2 REF 参考回采通道（板级）

| 位置 | 内容 |
|---|---|
| `driver/boards/CI-D02GS01J.c:644-704` | `ref_speak_info`：REF 回采 codec 实例定义 |
| `driver/boards/CI-D02GS01J.c:692-695` | REF 通道输入模式与增益：单端输入、MIC AMP 12dB、PGA 28.5（满档）、数字增益 6.0 |
| `driver/boards/CI-D02GS01J.c:706-714` | `ref_codec_sound_info`：REF 采样格式 16kHz 双通道 |
| `driver/boards/CI-D02GS01J.c:716-747` | `ref_inner_codec_registe()`：注册 `REF_RECORD_INNER_CODEC_ID` 并分配 PCM buffer |

### 5.3 采音输出通道与上传链路

| 位置 | 内容 |
|---|---|
| `projects/offline_asr_llm_aiot_iis_sample/app/app_main/ci_ssp_config.c:31-40` | `iis_out_audio_config`：**采音通道选择**（左=MICL、右=DST1），验证 REF 时把右改 `REFR` |
| `components/audio_in_manage/alg_preprocess.c:700-714` | 按 `iis_left_channel/iis_right_channel` 配置选通道并调用输出接口 |
| `components/audio_pre_rslt_iis_out/ci130x_audio_pre_rslt_out.c:833-861` | `audio_pre_rslt_write_data()`：算法帧输出总入口 |
| `components/audio_pre_rslt_iis_out/ci130x_audio_pre_rslt_out.c:531-570` | `audio_debug_uart0_upload()`：左右通道交织打包送串口采音 buffer |
| `components/audio_pre_rslt_iis_out/ci130x_audio_pre_rslt_out.c:338-398` | 串口采音任务与 UART DMA 发送（UART0 默认） |

### 5.4 调试采音使能宏（user_config.h）

| 位置 | 内容 |
|---|---|
| `projects/offline_asr_llm_aiot_iis_sample/app/app_main/user_config.h:88` | `DEBUG_AUDIO_UART_UPLOAD_EN`（0→1 开串口采音） |
| `projects/offline_asr_llm_aiot_iis_sample/app/app_main/user_config.h:89-95` | 采音串口号（UART0）/ 波特率（921600）/ buffer 大小；开日志串口需错开 |
| `projects/offline_asr_llm_aiot_iis_sample/app/app_main/user_config.h:158` | `AUDIO_DATA_UPLOAD_BY_IIS=1`（经 IIS 回传采音，串口方案的替代） |
| `projects/offline_asr_llm_aiot_iis_sample/app/app_main/user_config.h:376-393` | **宏互斥检查**：`AUDIO_DATA_PLAY_BY_IIS` 与外挂 codec 采参考（384 行）、与 `MIC_RECORD_IIS_SELECT`（388 行）互斥，同开会 `#error` |

### 5.5 其他关联代码

| 位置 | 内容 |
|---|---|
| `projects/offline_asr_llm_aiot_uart_sample/app/app_main/user_config.h:57-58` | `MIC_DIFF_SINGLE` / `REF_DIFF_SINGLE`：麦克风与参考通道差分/单端选择 |
| `projects/offline_asr_llm_aiot_uart_sample/app/app_main/user_config.h:344` | `AUTO_GET_AEC_GAIN=0`：AEC 增益自动标定关闭，增益为手写死值 |
| `projects/offline_asr_llm_aiot_uart_sample/app/app_main/system_msg_deal.c:765` | `audio_play_set_vol_gain(93*vol/VOLUME_MAX + 7)`：数字音量设置（影响回声强度；模拟回采自动跟随） |
| `components/alg/aec/ci_aec_type.h` | `aec_config_t` 参数类型定义 |
| `components/alg/aec/ci_adapt_aec.h` | AEC 适配层接口 |

---

## 6. ESP32 对接时的 IIS 硬约束

| 项 | 约束 |
|---|---|
| 主从 | CI1302 只能做 IIS MASTER，**ESP32 必须配 SLAVE**，BCLK/LRCK 由 1302 输出 |
| 格式 | 16kHz / 16bit / 立体声 / BCLK:LRCK=64 / MCLK=256FS |
| 宏冲突 | `AUDIO_DATA_PLAY_BY_IIS=1` 时参考只能走内部 codec；IIS 播放与外挂 codec 采参考、与 `MIC_RECORD_IIS_SELECT` 均互斥（同开 `#error`） |

---

## 7. 附录：关键参数速查表

### 7.1 `aec_config`（ci_ssp_config.c:221-241）

| 参数 | 当前值 | 说明 |
|---|---|---|
| `alg_enable` | true | AEC 算法使能 |
| `mic_channel_num` | 1 | 麦克风通道数（1302 单麦场景） |
| `ref_channel_num` | 1 | 参考通道数 |
| `aec_control_mode` | ENABLE_PLAYING_STATE_MODE | 按播报状态启停 AEC（另一模式 COMPUTE_REF_AMPL_MODE 按参考幅值判断） |
| `aec_gain` | 1.0f | AEC 输出增益 |
| `aec_enable_threshold` | 2000.0f | 参考信号判断门限 |
| `nlp_flag` / `aggr_mode` | 2 / 1 | NLP（残余回声抑制）开关与激进程度 |
| `fft_size` | 256 | 频域处理频点数 |
| `alc_off_codec_adc_gain_mic` | 20 | MIC 侧 ADC 增益（dB），与 mic 灵敏度匹配 |
| `alc_off_codec_adc_gain_ref` | 4 | 内部 codec 回采参考增益（dB），**需与板上分压比匹配** |
| `dtd_ratio` | 1.0f | 双讲检测比例 |

### 7.2 REF 回采通道 codec 增益（CI-D02GS01J.c:692-695）

| 参数 | 当前值 |
|---|---|
| `codec_adc_input_mode_r` | INNER_CODEC_INPUT_MODE_SINGGLE_ENDED（单端） |
| `codec_adc_mic_amp_r` | INNER_CODEC_MIC_AMP_12dB |
| `pga_gain_r` | 28.5f（满档，无余量） |
| `dig_gain_r` | 6.0f |

### 7.3 注意事项

1. 右通道 DST1 是整链输出（AEC+降噪+NLP），量化 ERLE 时用"只播放不说话"的数据段。
2. 串口采音先开工具再上电；`DEBUG_AUDIO_UART_UPLOAD_EN=1` 时 log 串口配置会被替换（`CONFIG_CI_LOG_UART` 改变），勿与协议串口冲突；多消耗约 13KB SYS 内存。
3. `AUTO_GET_AEC_GAIN=0`，增益为手写死值；改过硬件（换 mic、改分压、换功放）后必须重新核对 §7.1/§7.2 增益。
4. 测试务必在**播放状态**下进行（`ENABLE_PLAYING_STATE_MODE`），静音环境测不出回声抑制效果。
