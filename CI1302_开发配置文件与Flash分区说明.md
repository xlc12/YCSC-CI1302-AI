# CI1302 开发配置文件与 Flash 分区说明

> 以 `projects/offline_asr_llm_aiot_uart_sample`（CI1302 / 2 MB flash）为例，说明各部分配置文件的分工、flash 分区的排布规律，并复盘一次「固件超出 flash 大小」的烧录报错。
>
> 结论来源：SDK 内文件 + 打包工具（`tools/ci-tool-kit.exe`）实测。

---

## 一、结论速览

CI1302 的分区，本质是**把一片 2 MB flash 按地址从低到高切成若干连续区块**，每块放一类数据（用户代码、识别模型、DNN 模型、提示音、自定义文件、掉电保存区）。地址与大小都写在工程内的 `firmware/config.ini` 里，由打包工具负责摆放。

| 关键数字 | 含义 |
| --- | --- |
| **2 MB** | CI1302 片内 flash 容量（`flash_size=2`） |
| **1884 KB** | 修复后固件内容结束于 `0x1D7646`，距 NV 分区起点仍余 148 KB |
| **65%** | DNN 模型分区预留 1336 KB 占整片 flash 的比例，是最大消耗项 |
| **0x1FC000** | NV data 分区起始地址，也是内容不可越过的硬边界 |

本次遇到的烧录报错，根因不是地址写错，而是**各分区「预留大小」之和超过了 NV 分区起点**：打包出来的固件总长达到 2146 KB，比 2 MB 可用的 2032 KB 多出约 114 KB，于是 NV 分区被顶到 flash 之外。调整 3 个预留值后打包通过。

---

## 二、Flash 分区怎么理解

### 2.1 分区 = 地址 + 预留大小

在工程内 `firmware/config.ini` 的 `[PACKAGE]` 段中，每个分区由三个要素描述：`*_addr`（起始地址）、`*_size`（预留大小）、`*_path`（要烧进去的 bin 文件）。理解分区只需抓住三件事：

- **它是静态排布，不是文件系统**：上电后程序按固定地址直接读写，没有索引与分配。
- **真正决定成败的是「预留大小」，不是「起始地址」**：打包工具按预留大小从下往上顺序摆放，起始地址是排布的结果。实测把 `module_addr` 从 `0x37000` 改成 `0x90000` 后再打包，DNN 分区的落点完全不变。
- **NV data 分区固定在 flash 顶端**：实测把 `NV_addr` 从 `0x1F8000` 改成 `0x1F0000`，打包产物里 NV 依旧落在 `0x1FC000`，即 `flash 末尾 − NV_size`。

### 2.2 本工程的实际布局

下表中「预留」为 config.ini 里配置的大小，「实际占用」为对应 bin 文件的真实长度（实测打包产物中的分区表字段）。**只有当预留 ≥ 实际时，分区才不会溢出。**

| 分区 | 地址范围 | 预留 | 实际占用 | 用途 / config.ini 字段 |
| --- | --- | --- | --- | --- |
| 引导 + 分区表 | `0x000000–0x00A000` | 40 KB | 工具写入 | 引导程序（`binary/ci130x_bootloader.bin`）与分区表本身 |
| User 用户代码 | `0x00A000–0x049000` | 252 KB | 216 KB | 应用固件 + BNPU 算法库；`user_size` / `user_path` |
| ASR 模型 | `0x049000–0x04B000` | 8 KB | 5 KB | 识别模型与命令词；`command_size` / `asr_path` |
| **DNN 模型（最大项）** | `0x04B000–0x199000` | **1336 KB** | 1161 KB | 深度网络模型，决定整包能否塞进 2 MB；`module_size` / `dnn_path` |
| Voice 音源 | `0x199000–0x1D5000` | 240 KB | 229 KB | 提示音 / 播报音频；`voice_size` / `voice_path` |
| UserFile | `0x1D5000–0x1D7000` | 8 KB | 6 KB | 自定义文件（如 `cmd_info`）；`user_file_size` / `user_file_path` |
| （未使用余量） | `0x1D7000–0x1FC000` | 148 KB | — | 预留缓冲，调大 DNN / Voice 时可从这里借空间 |
| NV data | `0x1FC000–0x200000` | 16 KB | 运行期写入 | 掉电保存、标定参数；`NV_size`（工具固定放 flash 顶端） |

分区大小对比如下（按地址顺序，单位 KB）：

```
DNN 模型   0x04B000 |████████████████████████████████████████████████████████████████████| 1336
User 代码  0x00A000 |███████████████|                                                      252
Voice 音源 0x199000 |██████████████|                                                       240
未使用余量 0x1D7000 |█████████|                                                            148
引导+分区表 0x000000 |██|                                                                    40
NV data    0x1FC000 |█|                                                                     16
ASR 模型   0x049000 |                                                                        8
UserFile   0x1D5000 |                                                                        8
```

### 2.3 分区表自己也在 flash 里

分区表是一段结构体（`#pragma pack(1)` 的 `ci_flash_data_info_t`），记录厂商号、产品号、芯片名以及每个分区的 `offset / size / crc / status`（见 `components/flash_control/flash_control_inc/ci_flash_data_info.h`）。固件通过宏决定去哪里读它：

| 宏 / 条件 | 分区表地址 | 说明 |
| --- | --- | --- |
| `CI_OTA_ENABLE = 1` | `0x6000` | 支持 OTA 时使用 |
| `CI_OTA_ENABLE = 0` | `0x2000` | 不支持 OTA 时使用 |

> ⚠️ **需要核对的一点**
> 本工程的 `user_config.h` 中 `CI_OTA_ENABLE = 0`（对应 `0x2000`），但实测打包产物中，**分区表实际写在 `0x6000`**（该处 `ChipName` 字段读出 `CI1302`、`ProductID` 为 100，与 config.ini 一致），`0x2000` 区域为空。做 OTA 相关改动时，务必确认「工具写入位置」与「固件读取位置」一致。

### 2.4 三条硬约束

1. **预留 ≥ 实际**：任一 `*_size` 小于对应 bin，打包立即报 `xxx partition overflow`。
2. **内容不越 NV 起点**：`头部 + User + ASR + DNN + Voice + UserFile 的预留之和 ≤ 0x1FC000`，否则报「NVdata 分区地址超出该设备 flash 大小」。
3. **改完要让工具重新排布**：直接改 `*_addr` 不会改变实际落点，改完 `*_size` 后重新打包（或使用工具中的地址刷新）即可。

---

## 三、各分区内容从哪来

分区里的 bin 不是手写的，而是编译与合成脚本产出，再交给打包工具拼成一整包固件。

```
应用源码 app/ ──make──► user_code.bin（[0]code.bin + BNPU 算法库 [1]code.bin）
asr/ 模型     ──merge asr-file──► asr.bin      ┐
dnn/ 模型     ──merge nn-file ──► dnn.bin      │
voice/ 提示音 ──lame + merge────► voice.bin    ├──► config.ini（分区地址与大小）
cmd_info.xls  ──cmd_info.bat────► user_file.bin┘
                                                    │
                                                    ▼
                                    ci-tool-kit make-firmware
                                                    │
                                                    ▼
                                        Firmware_2.0.0.bin ──► 烧录 / 串口升级
```

- **User 分区**：`make` 后由链接器输出 `[0]code.bin`，同时把 BNPU 核心算法库 `libbnpu_core_alg_pro.a` 复制成 `[1]code.bin`，再用 `ci-tool-kit merge user-file` 合成 `user_code.bin`（见 `project_file/makefile`）。
- **ASR / DNN / Voice / UserFile**：由 `firmware/合成分区bin文件.bat` 一键合成；其中提示音默认按 `fmt=4`（MP3，16 kHz 重采样）压制。
- **NV data 分区**：不打包任何内容，只登记地址与大小，供运行期掉电保存。

> **别混淆：flash 分区 ≠ SRAM 布局**
> 第二类地址在链接脚本 `lds/ci130x_asr_alg_mcu_temp.lds` 里：`SRAM_START_ADDR = 0x1ff51000`、`STACK_SIZE = 0xC00`、`SYS_HEAP_SIZE = 1024*120`，以及 `BOOT_PARAMETER` / `SHARE_SRAM` 段。这些描述的是**片内 RAM 的分配**，和 flash 分区是两套完全不同的地址体系；内存不够时调 `user_alg_config.mk` 与 lds，flash 不够时才动 config.ini。

---

## 四、开发时需要的配置文件

按下表对号入座即可：**分区不够用看 config.ini，内存不够用看 user_alg_config.mk 与 lds，功能开关看 user_config.h 与 makefile**。

| 文件 | 管什么 | 关键项 / 何时改 |
| --- | --- | --- |
| `firmware/config.ini` | 分区与打包总配置 | `chipid`、`flash_size`、各分区 `*_addr / *_size / *_path / *_version`、`NV_size`、`[UPDATE]` 串口升级参数。**改分区必改这里** |
| `app/app_main/user_config.h` | 应用功能宏 | 板级选择、mic/回采配置、串口与协议、`CI_OTA_ENABLE`（影响分区表位置）、VAD/压缩/播放等离在线参数 |
| `system/user_alg_config.mk` | 算法开关 + 内存预算 | `USE_AEC / USE_NN_DENOISE / USE_NN_VAD / USE_CWSL / USE_GCC_DOA`、各算法 BNPU SRAM 消耗、`AIOT_AUDIO_COMPRESS_TYPE`；**决定 BNPU 库与 SRAM 结束地址** |
| `project_file/makefile` | 工程编译 / 链接 | `AUDIO_UPLOAD_TYPE`、`USE_AUDIO_PLAYER_TYPE`、`MULT_INTENT`、链接脚本与库，以及 User 分区 bin 的生成规则 |
| `lds/ci130x_asr_alg_mcu_temp.lds` → `ci130x_asr_alg_mcu.lds` | SRAM / RAM 布局 | `SRAM_START/END_ADDR`、`STACK_SIZE`、`SYS_HEAP_SIZE`、栈与 heap 段。模板经 makefile 用 `sed` 替换 `@@…@@` 占位符生成 |
| `utils/common.lds`、`utils/common_head.mk`、`utils/common_tail_mcu.mk` | SDK 公共链接与编译规则 | `SDK_ALG_PRO_SRAM_HOST_END_ADDR` 等由 SDK 提供，**一般不改** |
| `system/sdk_default_config.h`、`system/platform_config.h` | SDK 默认配置 / 平台配置 | 内存池、codec、`REF_IN_FROM_INNER_CODEC` 等默认值；被 `ci_flash_data_info.h` 等共同包含 |
| `firmware/合成分区bin文件.bat`、`firmware/打包升级.bat`、`firmware/烧录user_code.bat` | 合成 / 打包 / 烧录脚本 | 调用 `tools/ci-tool-kit.exe`、打包升级工具与烧录工具；生成各分区 bin 并输出整包固件 |
| `tools/config.ini` | 烧录/打包工具自带默认芯片配置 | 工具侧默认值。工程实际使用的是 `firmware/config.ini`，**不要改错文件** |

---

## 五、实战复盘：固件超出 flash 大小

### 5.1 现象

编译通过，但烧录时弹窗报错：

```
错误
COMM2:固件大小超出该设备flash大小!(NWVdata分区地址超出该设备flash大小! )
```

### 5.2 定位过程

用 SDK 自带打包工具直接复现，避免猜测。在 `projects/offline_asr_llm_aiot_uart_sample/firmware` 目录下执行：

```powershell
..\..\..\tools\ci-tool-kit.exe make-firmware --ini-file config.ini --chip-name CI1302 --output-path out
```

结果：`flash_size=2` 时打包失败（`rom size overflow`）；把 `flash_size` 临时改成 8 后可打包，产物长度为 `0x218646`（2146 KB）> 2 MB —— 说明确实**装不下**，而不是 NV 地址填错。

差值来自三个被放大的预留值，以及一处「预留比实际还小」：

| config.ini 字段 | 修复前 | 实际需要 | 问题 |
| --- | --- | --- | --- |
| `command_size` | `0xF000`（60 KB） | `0x1369`（5 KB） | 多预留 55 KB |
| `module_size` | `0x14E000`（1336 KB） | `0x122428`（1161 KB） | 多预留 175 KB（可保留） |
| `voice_size` | `0x70000`（448 KB） | `0x395C3`（229 KB） | 多预留 219 KB |
| `user_file_size` | `0x1000`（4 KB） | `0x1647`（6 KB） | **预留小于实际，会额外报 `user_file partition overflow`** |

多预留合计约 450 KB，而 2 MB flash 在 NV 分区之前只有约 330 KB 余量，于是固件长度越界：**越界约 `0x218646 − 0x1FC000` = 114 KB**。

```
固件内容结束地址（KB）        上限：NV 起始 0x1FC000 = 2032 KB
───────────────────────────────────────────────┼──────────────
修复前 0x218646 = 2146 KB  ████████████████████████████████████████████▌  ← 超界 114 KB
修复后 0x1D7646 = 1884 KB  ████████████████████████████████████████       ← 余量 148 KB
```

### 5.3 修复

只改 3 个预留值，`module_size` 保持不变：

| 字段 | 修复前 | 修复后 |
| --- | --- | --- |
| `command_size` | `0xF000` | `0x2000` |
| `voice_size` | `0x70000` | `0x3C000` |
| `user_file_size` | `0x1000` | `0x2000` |

修复后重排布局为 `ASR 0x049000 / DNN 0x04B000 / Voice 0x199000 / UserFile 0x1D5000`，内容结束于 `0x1D7646`（1884 KB），NV 分区稳定落在 `0x1FC000`，打包成功，余量 148 KB。

---

## 六、检查清单

1. **预留不小于实际**：任一 `*_size` 必须 ≥ 对应分区 bin 的实际大小。
2. **总量守边界**：所有分区预留之和须 ≤ NV 起点（本工程 `0x1FC000`）。
3. **地址交给工具**：只改 `*_size`，地址由打包工具重新排布；改完用上面的命令自查一遍。
4. **空间不够先砍大头**：DNN 模型与 Voice 音源占了 flash 的大头，优先换更小模型、降低提示音码率或调整音频格式。
5. **两套地址别串台**：flash 分区在 config.ini，SRAM 在 lds；内存不足调后者，flash 不足调前者。
6. **OTA 相关改动要双向确认**：分区表位置由 `CI_OTA_ENABLE` 决定，改完必须确认工具写入位置与固件读取位置一致。

---

## 参考文件

1. `projects/offline_asr_llm_aiot_uart_sample/firmware/config.ini` —— 工程分区与打包配置
2. `components/flash_control/flash_control_inc/ci_flash_data_info.h` —— 分区表结构体与分区表地址宏
3. `projects/offline_asr_llm_aiot_uart_sample/project_file/makefile` —— 编译/链接配置，User 分区 bin 生成与 BNPU 库联动
4. `projects/offline_asr_llm_aiot_uart_sample/app/app_main/user_config.h` —— 应用功能宏（板级、串口、OTA、离在线参数）
5. `projects/offline_asr_llm_aiot_uart_sample/lds/ci130x_asr_alg_mcu_temp.lds` —— SRAM 链接脚本模板（栈 / heap / RAM 起止地址）
6. `projects/offline_asr_llm_aiot_uart_sample/firmware/合成分区bin文件.bat` —— 各分区 bin 合成脚本
7. `tools/ci-tool-kit.exe` —— SDK 打包工具（本文分区表读取与 NV 落点为其实测记录）
8. `system/user_alg_config.mk` —— 算法开关与各算法内存预算
