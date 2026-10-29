# 移植对照矩阵与排查树

## 一、换芯片 / 换主控时逐项核对

| 维度 | 核对内容 | CI1302（当前） | CI1306 |
|---|---|---|---|
| 芯片定义 | `CI_CHIP_TYPE` | 1302 | 1306 |
| 板级宏 | `USE_CI_xxx_BOARD` | `USE_CI_D02GS01J_BOARD=1` | `USE_CI_D06GT01D_BOARD=1` |
| 协议串口 | 端口 / 引脚 | UART0，PB5=TX、PB6=RX | UART1，PB7、PC0 |
| 日志串口 | 是否与协议口冲突 | 冲突，需 `USE_1302_UART0_MODE=2` 关日志 | 不冲突 |
| IIS 引脚 | SDO/SDI/BCLK/LRCLK/MCLK | PA4/PA2/PA5/PA3/PA6 | 同左 |
| IIS 主从 | 芯片能否做从机 | 只能主机 | 只能主机 |
| 音频格式 | 采样率 / 位宽 / 声道 | 16k / 16bit / 立体声 | 同左 |
| 麦克风模式 | `MIC_DIFF_SINGLE` | 1（单端） | 按模块 |
| 参考通路 | `REF_DIFF_SINGLE`、`REF_IN_FROM_INNER_CODEC` | 1、0（外部模拟回采） | 按硬件 |
| 增益 | MIC AMP / PGA / 数字 | 12dB / 28.5dB / 6dB | 同架构，值按实测 |
| 偏置 | MICBIAS 来源 | 片内 LDO，1.7×(AVDD/2) | 同架构 |
| 功放 | 引脚 / 极性 / 是否自动控制 | PC4 / 固件低有效 / 不自动 | 查板级文件 |
| 分区 | flash 容量、NV 起始、预留 | 2MB，NV 0x1FC000 | 查 `config.ini` |
| 音频时机 | `IIS_UPLOAD_IS_WAKEUP`、`IIS_DOWNLOAD_BY_CMD` | 1、1（全由主控控制） | 同左 |

## 二、排查树

```
现象：烧不进去 / 打包失败
└─ 查 firmware/config.ini 各分区 size 与实际 bin 大小
   ├─ 大分区溢出 → 重分配 module / voice
   └─ 小分区溢出（user_file）→ 单独放大

现象：上电无声
├─ 是否收到 SYS_READY(0x0601)？否 → 串口配置 / 波特率 / 引脚
├─ 本地提示音是否被注释？（本工程全部注释，属设计）
├─ 是否发过 MASTER_PLAY_STATE=1？否 → 播放窗口未打开
├─ IIS 是否有 BCLK/LRCLK？否 → 引脚复用被覆盖或主从配反
└─ PA 是否打开？（PC4 电平 / 0x0119 取反语义）

现象：录音无数据或幅度异常
├─ IIS_UPLOAD_IS_WAKEUP=1 时是否发过 SET_AUDIO_START_RECORD？
├─ 麦克风单端/差分配置与实物是否一致？
├─ 单端时 MIC_N 是否真的接地？原麦是否已断开？
└─ 增益是否过小/削顶？查 MIC AMP 与 dig_gain

现象：回声消不干净
├─ 是否刚改过单端？→ 预期劣化 1~3dB，需重新标定
├─ PA 开关瞬态 / 电源纹波是否耦合进 MIC 走线？
├─ REF 通道是否正常回采（外部右声道）
└─ 是否擅自改过 pga_gain（满档时无余量）

现象：唤醒率下降
├─ 麦克风灵敏度 / 偏置是否变化
├─ 底噪抬升（单端引入共模噪声）
└─ AEC 标定参数是否沿用旧硬件
```

## 三、易踩的坑

1. **改 `sdk_default_config.h` 不生效**：项目层 `user_config.h` 已覆盖该宏。
2. **先调 `inner_codec_micbias_set()` 后被覆盖**：`inner_codec_adc_enable()` 内有 `reg23 |= 0x07`，必须在它之后调用。
3. **以为提高外部供电能提高 MICBIAS**：偏置基准是片内 LDO 的 AVDD，与外部输入电压无关。
4. **把数字增益当模拟增益用**：数字增益补幅度不补 SNR，单端场景下会连带放大底噪。
5. **PA 常开**：`PLAYER_CONTROL_PA=0` 时固件不管 PA，需主控同步开关。
6. **`PA6` 被 IIS 复用后想挪作他用**：需改 `mclk_out_en = IIS_MCLK_MODENULL` 并去掉 `pad_config_for_iis()` 中的 PA6 复用，否则会被覆盖回第二功能。
7. **分区表只改 size 不改 addr**：地址是累加结果，改一个 size 必须顺移后面所有 addr。
