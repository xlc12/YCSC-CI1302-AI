# 09 · 关键 API 参考

本文按模块汇总二次开发最常用的接口。签名以对应头文件为准；此处给出用途与调用时机。

---

## 1. ASR（`components/asr/asr_api.h`）

| API | 用途 | 调用时机 |
| --- | --- | --- |
| `int asrtop_taskmanage_create(bool use_ulxx_type)` | 创建 ASR 任务/信号量/队列 | 系统初始化 |
| `int asrtop_asrpcmbuf_mem_cfg(unsigned base, int frm_nums, int frm_shift)` | 配置 PCM 缓存 | ASR 启动前 |
| `int asrtop_asr_system_start(lg0, lg0_size, ac, ac_size, pdata)` | 加载模型并启动 | 启动/切换模型 |
| `int asrtop_asr_system_release(void)` | 释放 ASR 缓存 | 需要长时间占用 Flash 前 |
| `int asrtop_asr_system_pause(void)` | 暂停识别 | 播报、配网、OTA |
| `int asrtop_asr_system_continue(void)` | 恢复识别 | 上述操作结束 |
| `int asrtop_asr_switch_fst(int fst_idx, void *pdata)` | 切换语言模型网络 | 唤醒/退出唤醒 |
| `int asrtop_sys_isbusy(void)` | 查询是否忙 | 调度判断 |
| `int get_asrtop_asrfrmshift(void)` | 获取帧移 | 音频分帧 |
| `int asrtop_dynamic_confidence_mode_cfg(int mode)` | 0 平均 / -1 最大值 | 调优 |
| `int set_asr_sigle_word_confidence_count_threshold(short cfd, unsigned char cnt)` | 单词阈值 | 调优 |
| `int get_asr_sys_verinfo(char *buf)` | 版本（≥80 字节） | 版本上报 |
| `void asr_system_startup_task(void *p)` | ASR 启动任务 | 由组件创建 |

模型加载参数 `pdata`：

```
pdata[0] DNN 输出位宽（1=8bit，其他=16bit）
pdata[1] 是否启用第二语言模型
pdata[2] 第二语言模型地址
pdata[3] 第二语言模型大小
pdata[4] 模型索引
```

相关初始化：

```c
void decoder_port_inner_rpmsg_init(void);
void dnn_nuclear_com_outside_port_init(void);
void asr_top_nuclear_com_outside_port_init(void);
void vad_fe_nuclear_com_outside_port_init(void);
void codec_manage_inner_port_init(void);
```

---

## 2. 共享状态 ciss（`components/status_share/status_share.h`）

```c
void     ciss_init(void);
void     ciss_set(status_id_t id, status_t value);
status_t ciss_get(status_id_t id);
```

常用状态 ID：

| ID | 含义 |
| --- | --- |
| `CI_SS_ASR_EN` | ASR 使能 |
| `CI_SS_ASR_SYS_STATE` | ASR 是否已启动 |
| `CI_SS_WAKING_UP_STATE` | 是否唤醒 |
| `CI_SS_CMD_STATE` | 当前是唤醒词还是命令词 |
| `CI_SS_CMD_SCORE` | 当前命令词分数 |
| `CI_SS_PLAY_STATE` | 播放状态 |
| `CI_SS_VAD_STATE` / `CI_SS_NNVAD_STATE` | VAD 状态 |
| `CI_SS_ASR_TASK_PAUSED` | 识别是否暂停 |
| `CI_SS_HOST_PARAM_SET_OK` | HOST 参数同步完成 |
| `CI_SS_DNN_HOST_CFG_READY` / `CI_SS_DNN_BNPU_CFG_READY` | DNN 配置就绪 |
| `CI_SS_DNN_MODEL_FLASH_ADDR1..5` | 模型地址 |
| `CI_SS_DECODER_MIN_ACTIVE` / `CI_SS_DECODER_BEAM` | 解码参数 |
| `CI_SS_AUDIO_IN_BUFFER_NUM` | 音频缓冲数量 |
| `CI_SS_NN_DENOISE_MODEL_ADDR` 等 | 各算法模型地址 |

状态值约定（部分）：

| 枚举 | 值 |
| --- | --- |
| `ci_ss_vad_state_t` | IDLE / START / ON / END |
| `ci_ss_wakeup_state_t` | NO_WAKEUP / WAKEUPED |
| `ci_ss_asr_sys_state_t` | POWER_OFF / STARTED_UP |
| `ci_ss_play_state` | IDLE / PLAYING / PLAYING_TO_IDLE |
| `ci_ss_cmd_state_t` | NULL / WAKEUP / NORMAL |

---

## 3. 核间通信（`components/nuclear_com/ci130x_nuclear_com.h`）

```c
typedef int32_t (*nuclear_com_serve_cb_t)(void *payload, uint32_t len,
                                          uint32_t src, void *priv);
typedef struct {
    uint32_t trans_flag_addr;
    uint32_t src_ept_num;
    uint32_t dst_ept_num;
    void    *data_p;
    uint32_t data_len;
} nuclear_com_t;

void nuclear_com_init(void);
void nuclear_com_send(nuclear_com_t *msg, uint32_t timeout);
void nuclear_com_registe_serve(nuclear_com_serve_cb_t cb, mailbox_serve_ept_t ept);
```

远端调用宏：

```c
REMOTE_CALL(X)                 /* → rc_caller_X */
DECLEAR_REMOTE_FUNC_ID(X)      /* → RC_X_ID */
GET_REMOTE_FUNC_ID(X)
RC_MAX_PARAMETER_NUM           /* 8 */
```

按核选择：

```c
RC_HOST_CALLER(X) / RC_HOST_CALLEE(X)
RC_BNPU_CALLER(X) / RC_BNPU_CALLEE(X)
```

---

## 4. Codec（`components/codec_manager/codec_manager.h`）

| API | 用途 |
| --- | --- |
| `void cm_init(void)` | 初始化 |
| `int cm_reg_codec(int idx, cm_codec_hw_info_t *info)` | 注册（最多 3 个） |
| `int cm_register_codec_callback(int idx, void (*cb)(void))` | 事件回调 |
| `int cm_config_pcm_buffer(int idx, io_direction_t dir, cm_pcm_buffer_info_t *buf)` | 缓冲配置 |
| `int cm_config_codec(int idx, io_direction_t dir, cm_sound_info_t *info)` | 采样/位宽/声道 |
| `int cm_start_codec(int idx, io_direction_t dir)` | 启动 |
| `int cm_stop_codec(int idx, io_direction_t dir)` | 停止 |
| `int cm_read_codec(int idx, uint32_t *addr, uint32_t *size)` | 读取采集块 |
| `int cm_write_codec(int idx, void *pcm, uint32_t wait)` | 写播放块 |
| `int cm_get_pcm_buffer(int idx, uint32_t *buf, uint32_t wait)` | 取缓冲 |
| `int cm_release_pcm_buffer(int idx, io_direction_t dir, void *buf)` | 归还 |
| `int cm_set_codec_dac_gain(int idx, cm_cha_sel_t cha, int gain)` | DAC 增益 |
| `int cm_set_codec_adc_gain(...)` | ADC 增益 |
| `int cm_set_codec_alc(...)` / `cm_get_codec_alc_state(...)` | ALC |
| `int cm_set_codec_mute(...)` | 静音 |
| `int cm_get_codec_empty/busy_buffer_number(...)` | 缓冲水位 |
| `icodec_*` | 内部 Codec 实现 |

方向：`CODEC_INPUT`（ADC）、`CODEC_OUTPUT`（DAC）。
声道：`CM_CHA_LEFT`、`CM_CHA_RIGHT`、`CM_CHA_TWO_CHA`。

---

## 5. 音频输入（`components/audio_in_manage/audio_in_manage.h`）

```c
int32_t audio_in_buffer_init(void);
int32_t audio_in_hw_straight_mode_init(void);
void    audio_in_preprocess_mode_task(void *p);
void    audio_in_rpmsg_init(void);
bool    get_audio_queue_space_is_enough(void);
```

---

## 6. 精简播放器（`components/simple_audio_player/simple_audio_player.h`）

| API | 用途 |
| --- | --- |
| `int sap_init(void)` | 初始化 |
| `int sap_play(uint32_t addr, void *end_cb)` | 播放 Flash 音频 |
| `int sap_play_stream(const char *fmt, uint32_t buf_size, uint32_t block_size, void *end_cb, SAP_PLAY_DATA_REQUEST_CALLBACK req_cb)` | 播放流 |
| `sap_status_t sap_get_state(void)` | 状态 |
| `void sap_stop(void)` | 同步停止 |
| `int sap_register_decoder(audio_decoder_interface_t *d)` | 注册解码器 |
| `int32_t pdf_push_data(uint8_t *data, uint32_t size, TickType_t wait)` | 推送数据 |
| `void audio_play_hw_pa_da_ctl(FunctionalState cmd, bool control_pa)` | PA 控制 |
| `void audio_play_set_vol_gain(int32_t gain)` | 音量增益 |

回调类型：

```c
typedef void (*SAP_PLAY_END_CALLBACK)(int32_t arg);
typedef void (*SAP_PLAY_DATA_REQUEST_CALLBACK)(int32_t data_size);
```

---

## 7. Flash 分区与文件表（`components/flash_control/flash_control_inc/ci_flash_data_info.h`）

| API | 用途 |
| --- | --- |
| `ci_flash_data_info_init(default_group)` | 解析分区表 |
| `get_current_model_addr(&dnn, &dnn_size, &asr, &asr_size)` | 当前组模型地址 |
| `get_group_addr(partition_addr, group_id)` | 组地址 |
| `get_file_addr(group_addr, file_id, &addr, &size)` | 文件地址 |
| `get_dnn_addr_by_id(file_id, &addr, &size)` | DNN 模型地址 |
| `get_asr_addr_by_id(id, &addr, &size)` | ASR 模型地址 |
| `get_voice_addr_by_id(ids, addrs, num)` | 提示音地址 |
| `get_userfile_addr(file_id, &addr, &size)` | 用户文件地址 |
| `get_fw_version(product_version_t *v)` | 固件版本 |
| `get_partition_table(void)` | 分区表指针 |
| `cached_flash_reader_init/read/destroy` | 带缓存的 Flash 顺序读 |
| `get_partition_table_crc(pt)` | 分区表校验 |

文件 ID 约定：`60000` 命令词，`60001` 声纹，`60002` 哭声，`60003` NN 降噪，`60004` NN DOA，`60005` 鼾声，`60006` 通话降噪，`60008` 男女声。

---

## 8. NV 参数（`components/ci_nvdm/ci_nvdata_manage.h`）

```c
void cinv_init(uint32_t flash_addr, uint32_t size);
cinv_item_ret_t cinv_item_init(uint32_t id, uint16_t len, void *buf);
cinv_item_ret_t cinv_item_read(uint32_t id, uint16_t len, void *buf, uint16_t *real_len);
cinv_item_ret_t cinv_item_write(uint32_t id, uint16_t len, void *buf);
cinv_item_ret_t cinv_item_delete(uint32_t id);
cinv_item_ret_t cinv_register_hotid(uint32_t hot_id);
```

返回值：`CINV_ITEM_UNINIT`、`CINV_OPER_FAILED`、`CINV_OPER_SUCCESS`、`CINV_ITEM_LEN_ERR`。

预定义 ID：

| ID | 用途 |
| --- | --- |
| `NVDATA_ID_VOLUME (0x50000001)` | 音量 |
| `NVDATA_ID_CWSL_START (0x50001000)` | 自学习数据 |
| `NVDATA_ID_VP_MOULD_INFO (0xA0000001)` | 声纹模板 |
| `NVDATA_ID_OTA_MCU_STATUS (0x80000001)` | OTA 状态 |
| `NVDATA_ID_USER_START (0x60000001)` | 用户自定义起点 |

擦除块 4 KB，magic `0xABCDEF03`。

---

## 9. 日志（`components/log/ci_log.h`）

```c
void ci_log_init(void);
ci_logverbose(level, fmt, ...);
ci_logdebug(level, fmt, ...);
ci_loginfo(level, fmt, ...);
ci_logwarn(level, fmt, ...);
ci_logerr(level, fmt, ...);
ci_logassert(level, fmt, ...);
```

等级位掩码：`CI_LOG_ASSERT/ERROR/WARN/INFO/DEBUG/VERBOSE`。

---

## 10. 按键（`components/ci_key/ci_key.h`）

```c
void ci_key_gpio_init(void);
void ci_key_io_init(void);
void ci_key_init(void);
void key_info_show(sys_msg_key_data_t *msg);
void HEMY_Timer_Create(void);
void AutoReloadCallback(TimerHandle_t xTimer);
void HEMY_Timer_Create_START(void);
```

消抖：`KEY_SHORTPRESS_TIMER=2`（2×10 ms），长按：`KEY_LONGPRESS_TIMER=100`（100×10 ms）。

---

## 11. VMUP 串口协议（`components/msg_com/voice_module_uart_protocol.h`）

```c
void vmup_receive_packet(uint8_t ch);      /* 中断中逐字节喂入 */
void vmup_communicate_init(void);          /* 初始化解析器 */
/* 发送族函数（vmup_send_*）与 ACK/通知封装见头文件后半部分 */
```

帧常量：

```c
VMUP_MSG_HEAD_LOW  0xA5
VMUP_MSG_HEAD_HIGH 0xFC
VMUP_MSG_TAIL      0xFB
VMUP_MSG_TYPE_CMD_UP    0xA0
VMUP_MSG_TYPE_CMD_DOWN  0xA1
VMUP_MSG_TYPE_ACK       0xA2
VMUP_MSG_TYPE_NOTIFY    0xA3
```

常用命令见 [06-离在线AIoT与协议](06-离在线AIoT与协议.md)。

---

## 12. 系统消息与唤醒（`app/app_main/system_msg_deal.h`）

| API | 用途 |
| --- | --- |
| `void userapp_initial(void)` | 用户业务初始化 |
| `void sys_msg_task_initial(void)` | 创建系统消息队列/任务 |
| `void UserTaskManageProcess(void *p)` | 系统主循环 |
| `void enter_wakeup_deal(void)` | 进入唤醒 |
| `void exit_wakeup_deal(void)` | 退出唤醒 |
| `sys_wakeup_state_t get_wakeup_state(void)` | 查询唤醒 |
| `void update_awake_time(void)` | 刷新计时 |
| `BaseType_t send_msg_to_sys_task(sys_msg_t *msg, TickType_t wait)` | 投递消息 |
| `bool check_current_playing(void)` | 是否播放中 |
| `void pause_asr(void)` / `resume_asr(void)` | 暂停/恢复识别 |
| `void pause_voice_in(void)` / `resume_voice_in(void)` | 暂停/恢复采集 |
| `void vol_set(uint8_t vol)` / `uint8_t vol_get(void)` | 音量 |

消息联合体 `sys_msg_t` 的成员类型：

```c
sys_msg_asr_status_t / sys_msg_asr_data_t
sys_msg_cmd_info_status_t / sys_msg_key_status_t
sys_msg_play_status_t / sys_msg_net_status_t
```

---

## 13. CIAS 应用层接口

### 13.1 上传（`cias_voice_upload.h/.c`）

| API | 用途 |
| --- | --- |
| `void voice_upload_task_init(void)` | 创建上传任务 |
| `void asr_wakeup_on_handle(void)` | 唤醒后启动 |
| `void asr_wakeup_exit_handle(void)` | 退出唤醒收尾 |
| `void cias_aiot_vad_start_handle(void)` | VAD 起点 |
| `void cias_aiot_vad_end_handle(void)` | VAD 终点 |
| `void cias_aiot_param_refresh(void)` | 刷新参数 |
| `void wifi_net_state_check(void)` | 网络检查 |

### 13.2 播放（`cias_audio_data_handle.h`）

| API | 用途 |
| --- | --- |
| `void cias_online_func_init(void)` | 初始化在线功能 |
| `void request_play_data_func(void)` | 请求播放数据 |
| `void audio_player_param_init(void)` | 播放参数初始化 |
| `void write_ota_mcu_status(ota_nv_status_t status)` | 写 OTA 状态 |

### 13.3 协议与发送（`cias_network_msg_protocol.h` / `cias_network_msg_send_task.h`）

| API | 用途 |
| --- | --- |
| `void network_recv_data_task(void *p)` | 收包任务 |
| `int32_t wifi_msg_deal(uint8_t *buf, int32_t len)` | 单帧分发 |
| `bool cmd_need_ack(uint16_t cmd)` | 是否需要 ACK |
| `int32_t wifi_current_state_get/set(...)` | 网络状态 |
| `void cias_send_cmd(uint16_t cmd, uint32_t data)` | 发送命令 |
| `void network_send_data_task(void *p)` | 发送任务 |
| `void voice_data_packet_and_send(...)` | 音频打包发送 |

### 13.4 运行期参数结构（`cias_aiot_protocol.h`）

`CiasAiotRunParamTypedef` 保存队列/缓冲/状态标志；`CiasAiotFuncParamTypedef` 保存功能参数：

| 字段 | 含义 |
| --- | --- |
| `vad_filter_frame` | VAD 过滤帧数 |
| `vad_end_silence_time` | 结束静音时长 |
| `vad_start_max_timeout` | 最长录音 |
| `wakeup_timeout` | 唤醒保持 |
| `interaction_multi_round` | 多轮 |
| `upload_play_full_duplex` | 全双工 |
| `vad_start_stop_paly` | VAD 停播 |
| `audio_play_mode` | 播放模式 |
| `upload_audio_by_denoise` | 上传降噪音频 |
| `aec_gain_state` | AEC 增益标定状态 |

---

## 14. 钩子（`app/app_main/system_hook.h`）

```c
void sys_power_on_hook(void);
void sys_weakup_hook(void);
void sys_sleep_hook(void);
void sys_asr_result_hook(...);
```

均为 `__WEAK`，应用可覆盖以实现自定义上报。

---

## 15. NLP（`components/nlp/ci_nlp.h`）

```c
int  nlp_callback(void);
int  set_intent_output(char *cmd_detail, short flag, char *entity);
int  asr_result_callback_nlp(char *cmd, int cfd, int frames,
                             short sli_cfd, short node_cfd,
                             short start_frame, short frame_len);
void nlp_param_set(NlpParamTypedef *p);
void clean_nlp_keywords(void);
```

`NlpParamTypedef` 需要调用方提供输出缓冲、命令词数组、分数数组与两个回调（结果输出、意图排序）。

---

## 16. OTA（`components/ota/`）

| API | 头文件 | 用途 |
| --- | --- | --- |
| `int firmware_update_main(uint32_t baudrate)` | `firmware_updater.h` | 进入串口升级主循环 |
| `ota_partition_verify.h` | — | 分区校验 |
| `ota_aes.h` | — | 固件解密 |

云端 OTA 命令见 [06-离在线AIoT与协议](06-离在线AIoT与协议.md) 第 7 节。
