/**/
#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__


//**板级配置选择
/*板级配置更多细节请查看:https://document.chipintelli.com/硬件资料-->模块手册
chipintelli提供的部分开发板和模组，可以通过下面的宏选择，也可以参考开发板的板级配置文
件添加自定义板级配置文件*/
#define USE_CI_D02GS01J_BOARD       1   //CI-D0XGS01J，端子模块，芯片型号必须设置为1302
#define USE_CI_D02GS02S_BOARD       0   //CI-D0XGS02S，SMT模块，芯片型号必须设置为1302
#define USE_CI_D12GS01J_BOARD       0   //CI-D0XGS01J，端子模块，芯片型号必须设置为1312JE
#define USE_CI_D06GT01D_BOARD       0   //CI-D06GT01D，开发版，芯片型号必须设置为1306
#define USE_CI_E12GS02J_BOARD       0   //CI-E12GS02J，开发版，芯片型号必须设置为231x
#define USE_CI_D06GT01J_BOARD       0   //CI_D06GT01J, 开发板，型号必须为设置1306-仅配置支持双mic 算法+AEC，其他配置不支持
#define USE_CI_E0XGTD02S_BOARD      0   //CI-E06GT02S, 开发板2305/2306
#define USE_CUS_XXXXXXX_BOARD       0   //用户自定义

#if (USE_CI_D02GS01J_BOARD == 1)
#define CI_CHIP_TYPE                1302    //flash:2MB,SSOP24
#define BOARD_PORT_FILE             "CI-D02GS01J.c"
#elif (USE_CI_D02GS02S_BOARD == 1)
#define CI_CHIP_TYPE                1302    //flash:2MB,SSOP24
#define BOARD_PORT_FILE             "CI-D02GS02S.c"
#elif (USE_CI_D12GS01J_BOARD == 1)
#define CI_CHIP_TYPE                1312    //flash:2MB,SSOP16
#define BOARD_PORT_FILE             "CI-D12GS01J.c"
#elif (USE_CI_D06GT01D_BOARD == 1)
#define CI_CHIP_TYPE                1306    //flash:4MB,QFN40
#define BOARD_PORT_FILE             "CI-D06GT01D.c"
#elif (USE_CI_E12GS02J_BOARD == 1)
#define CI_CHIP_TYPE                2312    //flash:2MB,SSOP16
#define BOARD_PORT_FILE             "CI-E12GS02J.c"
#define USE_BLE                     1
#elif (USE_CI_D06GT01J_BOARD == 1)
#define CI_CHIP_TYPE                1306    //flash:4MB,QFN40 双mipc算法+外部codec 7243e用该板级
#define BOARD_PORT_FILE             "CI-D06GT01J.c"
#elif (USE_CI_E0XGTD02S_BOARD == 1)
#define CI_CHIP_TYPE                2306    
#define BOARD_PORT_FILE            "CI-E06GT02S.c"

#elif (USE_CUS_XXXXXXX_BOARD == 1)
#define CI_CHIP_TYPE                xxxx    //flash:4MB,QFN40
#define BOARD_PORT_FILE             "CI-XXXX.c"
#endif

#ifndef HOST_MIC_USE_NUMBER
#define HOST_MIC_USE_NUMBER            1   //定义mic数量
#endif

//**麦克风、回声参考电路模式配置
#define AUDIO_IN_FROM_DMIC              0       //是否使用数字MIC输入音频

/*注意!! 外部IIS0需要根据SDK当前需求输入正确的信号，例如采样率、通道数和左右通道的数据（mic or ref）*/
#define MIC_RECORD_IIS_SELECT           0 //MIC输入的IIS选择:0，内部codec IIS1   1,外部IIS0;

#define MIC_DIFF_SINGLE                 0   /*1,单端。0，差分（通用模块都是差分模式，省成本的模块为单端(MICN_L 接GND)时，需要配置为SINGLE）)*/
#define REF_DIFF_SINGLE                 1   /*1,单端。0，差分（D类功放是采用单mic aec 使用 差分 mic ; AB类功放是采用单mic aec 使用 单端 mic)*/

/*1、双mic + aec算法必须外部挂codec作为信号回采(推荐7243e)
  2、双dmic + 模拟右麦/外部挂codec(推荐7243e)作为信号回采
  3、外部IIS0双麦 + 模拟右麦作为信号回采
*/
#if USE_AEC_MODULE && (USE_BEAMFORMING_MODULE || (USE_NN_DOA || USE_GCC_DOA) || USE_DEREVERB_MODULE || USE_DUAL_MIC_ANY)
#if AUDIO_IN_FROM_DMIC 
#if REF_IN_FROM_INNER_CODEC
#define IF_USE_ANOTHER_CODEC_TO_GET_REF 0//采用2 DMIC + 外挂codec输入REF信号
#else
#define IF_USE_ANOTHER_CODEC_TO_GET_REF 1//采用2 DMIC + 外挂codec输入REF信号
#endif
#elif MIC_RECORD_IIS_SELECT
#define REF_IN_FROM_INNER_CODEC         1//采用外部IIS0双麦 + 模拟右麦作为信号回采
#else
#define IF_USE_ANOTHER_CODEC_TO_GET_REF 1
#if USE_CI_D06GT01J_BOARD != 1   //必须外挂codec使用USE_CI_D06GT01J_BOARD板级
#error "dual mic alg + aec , must use USE_CI_D06GT01J_BOARD board\n"
#endif
#endif
#endif

//输出识别中间结果使能
#define ASR_MIDDLE_RESULT_OUT_EN       0   

  
//0：正常uart0作为日志串口 1：uart0作为调试采音串口 上传双声道数据;(注意：bnpu端的对应串口需要关闭，打开串口录音软件，再开机，避免数据错帧导致录音异常),多消耗13KB SYS内存,内存不够可先关闭重采样
#define DEBUG_AUDIO_UART_UPLOAD_EN                   0
#if DEBUG_AUDIO_UART_UPLOAD_EN
#define USE_UART_SEND_PRE_RSLT_AUDIO_NUMBER       (UART_TypeDef*)(HAL_UART0_BASE)   //用哪个UART口将算法处理后的音频数据送出
#define USE_UART_SEND_PRE_RSLT_AUDIO_BAUD         UART_BaudRate921600               //串口采音波特率
#define USE_UART_SEND_PRE_RSLT_AUDIO_BUF_LEN      1024*10                           //串口采音BUF大小
#define CONFIG_CI_LOG_UART                        0//HAL_UART1_BASE  //配置log输出使用的串口，请勿与protocol共用同一个串口
#else
//**通讯串口配置
#define CONFIG_CI_LOG_UART             HAL_UART0_BASE  //配置log输出使用的串口，请勿与protocol共用同一个串口
#endif

//离在线主要使用与WIFI通信的串口UART_NUM_SEND_PLAY_AUDIO_NUMBER，根据需求增加该串口
#define MSG_COM_USE_UART_EN            0   //0,关闭语音模块通讯协议。1,开启语音模块通讯协议。
#define UART_PROTOCOL_NUMBER           (HAL_UART2_BASE)    //语音模块协议使用的串口，请勿与log共用同一个串口。
#define UART_PROTOCOL_BAUDRATE         (UART_BaudRate921600) //语音模块协议使用的串口波特率。
#define UART_PROTOCOL_VER              2   //语音模块协议版本号:1,一代协议。2,二代协议，255,平台生成协议

#define CLOUD_UART_PROTOCOL_EN         0   //云端协议使能-只有在启英开发者平台做固件配协议能用
#if CLOUD_UART_PROTOCOL_EN
#define CLOUD_CFG_UART_SEND_EN         1   //使能串口发送数据
#define CLOUD_CFG_PLAY_EN              1   //播报音使能
#define CLOUD_CFG_UART_PORT	         ((UART_TypeDef*)(HAL_UART1_BASE))// HAL_UART0_BASE ~ HAL_UART2_BASE，请勿与log共用同一个串口
#define CLOUD_CFG_UART_BAUND_RATE    UART_BaudRate9600
#endif

//**通信串口引脚开漏模式使能配置
//注:推挽模式的IO只能对接3.3V电平的IO，开漏模式可以对接5V电平的IO(外部需要上拉到5V)
#define UART0_PAD_OPENDRAIN_MODE_EN     0   //0,UART0为推挽模式。1,UART0为开漏模式。
#define UART1_PAD_OPENDRAIN_MODE_EN     0   //0,UART1为推挽模式。1,UART1为开漏模式。
#define UART2_PAD_OPENDRAIN_MODE_EN     0   //0,UART2为推挽模式。1,UART2为开漏模式。

// ota使能
#define CI_OTA_ENABLE                   0   // 1使能 0 不使能

//**时钟源配置
#ifndef USE_EXTERNAL_CRYSTAL_OSC
#if ((CI_CHIP_TYPE == 1312) || (CI_CHIP_TYPE == 1311) || (CI_CHIP_TYPE == 2305) || (CI_CHIP_TYPE == 2306))
#define USE_EXTERNAL_CRYSTAL_OSC        0
#else
#define USE_EXTERNAL_CRYSTAL_OSC        1           //0:使用内部RC作为时钟源。1:使用外部晶振作为时钟源。
#endif
#endif
//**波特率自适应功能配置
#if (USE_EXTERNAL_CRYSTAL_OSC == 0)             //使用内部RC时,建议开启波特率自适应(需要电控增加对应支持)。
#define UART_BAUDRATE_CALIBRATE         1       //是否使能波特率自适应功能。
#define BAUDRATE_SYNC_PERIOD            300000  // 波特率同步周期，单位毫秒。
#define BAUDRATE_FAST_SYNC_PERIOD       5000    // 一次波特率同步失败后，下一次同步间隔，单位毫秒。
#define BAUD_CALIBRATE_MAX_WAIT_TIME    400     // 等待反馈包的超时时间，单位毫秒。
#endif

//*红外功能配置
#define USE_IR_ENABLE                   0       //红外功能，1:是 0:否。开启红外功能在使用打包工具升级固件时，请取消勾选“升级完成自动运行”，防止重复烧录\
                                                  红外功能涉及多模型切换和红外码库，firmware文件请参考external\firmware参考\ir(红外)\firmware
#if USE_IR_ENABLE
#define UART_CONTOR_SEND_IR             0       //用通信口进行串口协议控制发红外
#define IR_TEST	                        0       //用通信口进行串口协议的产检
#ifndef USE_NIGHT_LIGHT
#define USE_NIGHT_LIGHT                 1       
#endif
#endif
/********************************************离在线参数宏配置开始********************************************/
//*语音上传和播放配置(通过串口)
#ifndef AUDIO_DATA_UPLOAD_BY_UART
#define AUDIO_DATA_UPLOAD_BY_UART                       (1)            //通过串口上传语音功能配置，消耗75KB内存
#endif

#ifndef AUDIO_DATA_PLAY_BY_UART
#define AUDIO_DATA_PLAY_BY_UART                         (1)            //通过串口接收音频数据播放配置
#endif

#if !AUDIO_DATA_UPLOAD_BY_UART && !AUDIO_DATA_PLAY_BY_UART
#define WIFI_CMD_BY_UART                                (1)            //通过串口和wifi指令交互配置
#endif

// 若使用的组合内存不够，可关闭重采样，节省24KB内存，但是识别效果会下降5个点左右
//压缩算法选择-pcm和speex和opus只能三选一,不能同时支持; opus压缩+mp3播放不支持自学习，推荐推荐设置SYS_HEAP_SIZE = (1024*110))
#if (AUDIO_DATA_UPLOAD_BY_UART)
#define AUDIO_COMPRESS_RECORD_DISABLE                     0            //(务必在makefile中配置宏AIOT_AUDIO_COMPRESS_TYPE=0)PCM上传数据，不做数据压缩 推荐设置SYS_HEAP_SIZE = (1024*130))
#define AUDIO_COMPRESS_SPEEX_ENABLE                       0            //(务必在makefile中配置宏AIOT_AUDIO_COMPRESS_TYPE=1) 1-处理使能SPEEX压缩后的数据 2-使能opus压缩；推荐设置SYS_HEAP_SIZE = (1024*110)) 
#define AUDIO_COMPRESS_OPUS_ENABLE                        1            //(务必在makefile中配置宏AIOT_AUDIO_COMPRESS_TYPE=2) 1-处理使能SPEEX压缩后的数据 2-使能opus压缩 OPUS比speex多消耗31KB内存,目前仅USE_AEC_DENOISE_NN算法支持，其余内存不够
#define AUDIO_COMPRESS_G722_ENABLE                        0            //(务必在makefile中配置宏AIOT_AUDIO_COMPRESS_TYPE=0)20ms压缩一次，320个点，640字节压缩为160字节，压缩率为4:1 推荐设置SYS_HEAP_SIZE = (1024*130)) 
#endif
//网络数据播放类型选择-mp3和pcm和opus只能三选一，不能同时支持;
#if (AUDIO_DATA_PLAY_BY_UART)
#define NET_AUDIO_PLAY_BY_MP3                             1            //云端播放通过mp3格式(注意:推荐设置SYS_HEAP_SIZE = (1024*110))  
#define NET_AUDIO_PLAY_BY_PCM                             0            //云端播放通过pcm格式(注意:推荐设置SYS_HEAP_SIZE = (1024*130))
#define NET_AUDIO_PLAY_BY_G722                            0            //云端播放通过G722格式播放,必须使用精简播放器 推荐设置SYS_HEAP_SIZE = (1024*130)) 
#endif
//语音数据上传是否带协议头
#define AUDIO_SEND_WITH_PROTOCOL_HEADER                   1            //1-带协议头上传(默认) 0-不带协议直接传裸数据-调试使用

//云端交互相关配置          
#define CUR_INTERACTION_MULTI_ROUND_ENABLE                1              //1-多轮 0-单轮
#define UPLOAD_PLAY_FULL_DUPLEX_ENABLE                    1              //全双工处理,播放音频的同时支持vad检测音频上传
#define VAD_START_STOP_PLAY_ENABLE                        0              //全双工模式下，vad起来，立刻停止播放 1-停止播放 0-不停止播放
#define CLOUD_ANS_TIME_OUT_ENEABLE                        0              //云端响应超时功能使能
#define CLOUD_ANS_TIME_OUT_VALUE                          6              //云端响应超时时间6S，必须大于0
#define AUDIO_PLAY_MODE                                   1              //1-支持打断当前播放 0-不支持，顺序播放-暂时不用
#define UPLOAD_NNDENOISE_AUDIO_DATA_ENABLE                1              //1-上传降噪的音频 0-上传非降噪的音频
#define DENOISE_STRENGTH_ADAPT_ENABLE                     0              //降噪强度自适应调整模式，需在ci_ssp_config.h配置降噪模块中配置自适应模式具体参数
#define CHECK_NET_WORK_STATE_ENABLE                       0              //检测网络状态后再上传音频宏
#define PCM_MSG_STREAM_NUM                               30              //原始数据stream buf大小   //30
#define LOCAL_REC_VAD_END_ENABLE                          1              //本地识别后立即停止VAD,若不停止，在半双工并且本地词条有播报情况下,播报时会丢录音
//NN_VAD配置(10ms一帧)
#define NN_VAD_SENSITIVITY                                0              //灵敏度等级，设置0,1,2，对应低，中，高。灵敏度等级越高越灵敏;配合tvad_valid_num调试TVAD的灵敏度。改0（低）：环境嘈杂时降低噪声误触发（VAD_START 打断 TTS）
#define NN_VAD_FRONT_PORCH                                25             //后窗口大小， VAD_ON->VAD_END 最多持续的帧数 ；取值20~40（过小会导致说话停顿就VAD_END了)。改25（250ms）：说完话更快触发VAD_END上报
#define NN_VAD_VALID_CHECK_MODE                           1              //VAD语音有效性检查模型：1-前置检查，等待判断语音有效后，再回退语音上传 2-后置检查，优先实时上传语音，检查有效性结果后通知WIFI端,建议关闭云端响应超时功能使能
#define NN_VAD_VALID_NUM                                  25             //VAD_ON 状态的至少满足的有效帧数，大于该阈值则视为语音有效，低于该阈值识别结果无效。改25（250ms）：有效判定窗加长，滤掉短促环境噪声（关门声、碰撞声等），降低误打断

//超时强制VAD END检测
#define VAD_TIMEOUT_CHECK_ENABLE                          1              //0-不进行超时检测 1-进行超时检测-不可修改
#define VAD_TIMEOUT_VALUE                                 5              //强制结束录音的时间(单位S)，上传音频 

//回退帧数配置
#define NN_VAD_BACK_PORCH                                 25             //前端回退25帧400ms-
#define PCM_ALG_FRAME_LEN                                 512            //前端原始音频16K采样，每帧256个点共512字节

//最小音频帧过滤 暂不使用
// #define VAD_ON_MIN_NUM    (25)                                           //vad起来判断有效最短帧数为25帧(10ms一帧),最大40
// #if VAD_ON_MIN_NUM > 40
// #error "The VAD_ON_MIN_NUM max 40\n"
// #endif

//和WiFi通信串口及参数配置
#if AUDIO_DATA_PLAY_BY_UART || AUDIO_DATA_UPLOAD_BY_UART || WIFI_CMD_BY_UART
#define UART_NUM_SEND_PLAY_AUDIO_NUMBER                  HAL_UART1_BASE           //网络端交互的串口
#define UART_NUM_SEND_PLAY_AUDIO_BAUDRATE                UART_BaudRate921600      //网络端交互的串口波特率
#endif

#if NET_AUDIO_PLAY_BY_PCM || NET_AUDIO_PLAY_BY_OPUS || NET_AUDIO_PLAY_BY_G722
#if SIMPLE_AUDIO_PLAYER_ENABLE
#define NET_PLAY_PCM_OR_G722_ENABLE_LOCAL_PLAY            1             //云端播放通过pcm、G722格式时,同时开启本地播报
#endif
#define USE_HP_OUT_NET_AUDIO                              1             //使用hpout直接输出云端音频
#define PLAY_PCM_FRAME_ENABLE                             1             //播放pcm数据
#define PLAY_PCM_FRAME_BUF_LEN                         1024*6           //pcm播放缓冲大小
#define PLAY_FLOW_CTRL_BY_IO_ENABLE                       1             //播放流控IO控制使能
#define PLAY_FLOW_CTRL_ACTIVE_LEVEL                       0             //流控有效电平   
#define PLAY_ONE_FRAME_MAXE_SIZE                         1024           //单包数据长度最大值
#define PLAY_MAX_BUF_THR_SIZE                            2*1024         //pcm播放buffer 超过该值则控制WiFi停止发送数据 
#endif

#if NET_AUDIO_PLAY_BY_OPUS
#define OPUS_PLAY_QUEUE_NUM                              10              //OPUS数据播放队列大小   
#define OPUS_PLAY_QUEUE_TIEM_SIZE                        80              //OPUS数据播放队列元素大小
#define OPUS_DECODE_DATA_SIZE                            640             //40ms数据640个点(int16_t)，1280字节
#endif

#if (AUDIO_DATA_PLAY_BY_UART)
#define AUDIO_PLAY_USE_OUTSIDE                          (1)              //启用自定义外部数据源播放
#define AIOT_AUDIO_PLAY_BUF_SIZE                        (6 * 1024)       //播放器缓存大小
#define PLAY_BUF_GET_DATA_MIN_SIZE                      (4 * 1024)       //可播放数据小于此大小开始从网络获取下一帧数据--最小4K
#endif

#if NET_AUDIO_PLAY_BY_PCM
#define REQUEST_ONE_FRAME_SEZIE                          1024*4          //pcm单帧请求播放数据大小最小为4K，不然会出现播放卡顿
#elif   NET_AUDIO_PLAY_BY_MP3  
#define REQUEST_ONE_FRAME_SEZIE                          1024*1          //请求单帧播放数据大小(mp3)，比特率大于128kbps，建议使用1024*2
#elif   NET_AUDIO_PLAY_BY_OPUS  
#define REQUEST_ONE_FRAME_SEZIE                          320             //请求单帧播放数据大小(opus) 4帧*1280
#elif   NET_AUDIO_PLAY_BY_G722  
#define REQUEST_ONE_FRAME_SEZIE                          1024*1             //请求单帧播放数据大小(opus) 4帧*1280
#else
#define REQUEST_ONE_FRAME_SEZIE                          128            //不播放音频文件，缓存可以减小
#endif

#if AUDIO_COMPRESS_RECORD_DISABLE
#define PCM_UPLOAD_QUEUE_NUM                              60             //上传数据队列大小   //60
#define COMPRESS_NEED_PCM_LEN                             0              //不需要压缩
#define COMPRESS_UPLOAD_QUEUE_TIEM_SIZE                   512             //上传队列元素大小,speex编码后每帧长度42字节
#elif AUDIO_COMPRESS_SPEEX_ENABLE
#define PCM_UPLOAD_QUEUE_NUM                              60             //上传数据队列大小   //60
#define COMPRESS_NEED_PCM_LEN                            320             //不可修改(320个点 short)
#define COMPRESS_UPLOAD_QUEUE_TIEM_SIZE                   43             //上传队列元素大小,speex编码后每帧长度42字节
#elif AUDIO_COMPRESS_OPUS_ENABLE         
#define USE_ALLOCA                                        1              //不可修改
#define VAR_ARRAYS                                        1              //不可修改
#define OPUS_BUILD                                        1              //不可修改
#define DISABLE_FLOAT_API                                 1              //不可修改
#define FIXED_POINT                                       1              //不可修改
#define REMOVE_FOR_MALLOC                                 1              //不可修改
#define HAVE_CONFIG_H                                     1              //不可修改          
//节约内存和代码体积新增的配置
#define OPUS_SILK_SUPPORT                                 0              //不可修改    
#define OPUS_MUILT_FRAME                                  1              //不可修改    
#define PCM_UPLOAD_QUEUE_NUM                              30             //上传数据队列大小   //60
#define COMPRESS_NEED_PCM_LEN                            640             //不可修改 (640个点 short)
#define COMPRESS_UPLOAD_QUEUE_TIEM_SIZE                   80             //上传队列元素大小,opus编码后每帧长度80字节
#elif AUDIO_COMPRESS_G722_ENABLE
#define PCM_UPLOAD_QUEUE_NUM                              60             //上传数据队列大小   //60
#define COMPRESS_NEED_PCM_LEN                            320             //不可修改(320个点 short)
#define COMPRESS_UPLOAD_QUEUE_TIEM_SIZE                  160             //上传队列元素大小,speex编码后每帧长度42字节
#else
#define COMPRESS_UPLOAD_QUEUE_TIEM_SIZE                  128             //上传队列元素大小,speex编码后每帧长度42字节
#endif

#if NET_AUDIO_PLAY_BY_MP3 && AUDIO_COMPRESS_RECORD_DISABLE && UPLOAD_PLAY_FULL_DUPLEX_ENABLE && !VAD_START_STOP_PLAY_ENABLE
#define NETWORK_RECV_BUFF_MAX_SIZE                       (REQUEST_ONE_FRAME_SEZIE*10 + 20) //接收网络端串口数据最大size-帧头+预留4字节
#define AIOT_AUDIO_PLAY_BUF_SIZE                         (12 * 1024)       //播放器缓存大小
#else
#define NETWORK_RECV_BUFF_MAX_SIZE                       (REQUEST_ONE_FRAME_SEZIE + 20) //接收网络端串口数据最大size-帧头+预留4字节
#endif
#if AUDIO_COMPRESS_RECORD_DISABLE
#define NETWORK_SEND_BUFF_MAX_SIZE                       (COMPRESS_UPLOAD_QUEUE_TIEM_SIZE/4 + 16)   //发送串口数据到网络端最大size,pcm数据分4包传输，避免丢数据
#elif AUDIO_COMPRESS_G722_ENABLE
#define NETWORK_SEND_BUFF_MAX_SIZE                       (200)         //发送串口数据到网络端最大size
#else 
#define NETWORK_SEND_BUFF_MAX_SIZE                       (COMPRESS_UPLOAD_QUEUE_TIEM_SIZE + 16)     //发送串口数据到网络端最大size
#endif
#if (NET_AUDIO_PLAY_BY_PCM) && AUDIO_COMPRESS_RECORD_DISABLE
#define NETWORK_SEND_BUFF_NUM                             8            //发送串口数据到网络端缓冲区个数  
#else
#define NETWORK_SEND_BUFF_NUM                             5            //发送串口数据到网络端缓冲区个数  
#endif

#define CI230X_AUDIO_DATA_OUT_BY_UART                     0             //230X芯片调试使用，上传音频同时通过另外一个串口将音频数据发出
#define UPLOAD_PCM_DATA_ENABLE                            0             //上传音频裸数据不带协议-调试使用 1-使能 0-关闭(默认)
#if UPLOAD_PCM_DATA_ENABLE
#define UPLOAD_PCM_VAD_TAG_ENABLE                         1            //上传pcm数据时带vad 标签
#define NETWORK_SEND_BUFF_MAX_SIZE                      640
#define AUDIO_COMPRESS_SPEEX_ENABLE                       0 
#define AUDIO_SEND_WITH_PROTOCOL_HEADER                   0                    //1-带协议头上传 0-不带协议直接传裸数据
#endif 
#if ((USE_NN_DOA&&USE_AEC_MODULE) \
 || ((USE_CWSL)&& (AUDIO_COMPRESS_SPEEX_ENABLE || AUDIO_COMPRESS_OPUS_ENABLE))\
 || (AUDIO_COMPRESS_OPUS_ENABLE && NET_AUDIO_PLAY_BY_MP3))
//注意开doa+aec算法或者开自学习，由于内存原因，需要关闭重采样，节省24KB内存，但是识别效果会下降5个点左右，如果要使用doa+aec算法又要保证识别效果，请使用hpout输出方案
#define INNER_CODEC_AUDIO_IN_USE_RESAMPLE   0                   
#endif
/********************************************离在线参数宏配置结束********************************************/
#define USER_CODE_SWITCH_ENABLE     (0)                                 //两份code动态切换功能
#if USER_CODE_SWITCH_ENABLE
#define UART_PROTOCOL_NUMBER           (HAL_UART1_BASE)      
#define UART_PROTOCOL_BAUDRATE         (UART_BaudRate115200)    //TTS默认波特率115200
#define USER_CODE2_SAVE_ADDR           0x4000-8                 //code固件地址存放位置
#endif 

//**语音识别配置
#define USE_SEPARATE_WAKEUP_EN          1       //是否使用独立的唤醒词模型。1:是 0:否。
#define DEFAULT_MODEL_GROUP_ID          1       //模型ID,用于指定上电启动时，默认进入的语言模型。通常0为命令词模型,1为唤醒词模型
    
#if (!USE_SEPARATE_WAKEUP_EN)    
#undef DEFAULT_MODEL_GROUP_ID    
#define DEFAULT_MODEL_GROUP_ID          0
#endif

#define PLAY_WELCOME_EN                 1      //是否在启动时播放开机提示音。1:是 0:否。
#define PLAY_ENTER_WAKEUP_EN            1      //是否在唤醒时播放提示音。1:是 0:否。
#define PLAY_EXIT_WAKEUP_EN             1      //是否在切换到只监听唤词状态时播放提示音。1:是 0:否。
#define PLAY_OTHER_CMD_EN               1      //是否在识别到命令词时播放提示音。1:是 0:否。
#define ADAPTIVE_THRESHOLD              0
#define ASR_SKIP_FRAME_CONFIG           0
#define EXIT_WAKEUP_TIME                30*1000   //退出唤醒超时时间,单位毫秒。超过此配置指定的时间长度内没有识别到任何命令词，就会切换到只监听唤词状态。
    

/*自动获取AEC处理时较好的MIC和REF增益以及AEC使能阈值，增益值和使能阈值会在输出日志并且发送指令到主端,该流程仅测试使用，
SDK默认开启ALC；故将ci_ssp_config.c中的alc_off_codec_adc_gain和aec_enable_threshold设置为测试得到的增益值与使能阈值
*/
#define AUTO_GET_AEC_GAIN               0 
#if AUTO_GET_AEC_GAIN
#define GET_AEC_GAIN_AUDIO_ID           3000  //AEC增益测试音频ID
#define GET_AEC_REF_THR_AUDIO_ID        3001  //AEC使能阈值（参考信号能量）测试音频ID
/*输入幅值上限，确保mic输入不截幅,根据应用时需播报的音频最大dBFS给对应阈值（dBFS是数字音频系统中描述信号幅度的单位）：
  0~-2dBFS，ENERGE_UP_THR阈值为4500
  -2~-6dBFS，ENERGE_UP_THR阈值为6000
  -6dBFS以下，ENERGE_UP_THR阈值为7000
  */
#define ENERGE_UP_THR                   7000
#define PLAY_WELCOME_EN                 0      //启动时不播放开机提示音（不可修改）
#define GET_AEC_GAIN_POEWR_ON_START     1      //1：上电后直接开启AEC增益测试，使用默认参数 0：通过指令开启AEC增益测试
#define DEFAULT_MIN_MIC_GAIN            12     //默认最小MIC增益，最终输出的MIC增益不会小于该值，范围为（-18~28）
#define DEFAULT_EXPECT_VOLUM            5      //默认期望的播放音量等级，最终输出的音量等级可能会减小，范围（1~7）
#endif

//生产测试使用
#define IIS_CHANNEL_ENG_CALC_EANBLE                       0              //iis通道能量计算
#define ENG_CALC_INTERVAL_FRAME                           10             //iis通道能量10帧计算一次(可根据需求修改)
#define CIAS_HAVE_AUDIO_ENG_MICL                          50             //左MIC有音频能量阈值设置，默认50db    
#define CIAS_HAVE_AUDIO_ENG_MICR                          50             //右MIC有音频能量阈值设置，默认50db    
#define CIAS_HAVE_AUDIO_ENG_REFL                          50             //REFL有音频能量阈值设置，默认50db    
#define CIAS_HAVE_AUDIO_ENG_REFR                          50             //REFR有音频能量阈值设置，默认50db    
#define CIAS_UPLOD_FACTORY_TEST_REAL_VAL                  0              //上传音频上传过程中的实时值 0-不上传 1-上传  

//**播放器配置  
#if  USE_HP_OUT_NET_AUDIO
#define AUDIO_PLAYER_ENABLE             0   //是否启用音频播放器。0:不启用,1:启用。不使用播放功能时，启用本地mp3播放器多占用内存：默认播放器52K、精简播放器36K
#else
#define AUDIO_PLAYER_ENABLE             1   //是否启用音频播放器。0:不启用,1:启用。不使用播放功能时，启用本地mp3播放器多占用内存：默认播放器52K、精简播放器36K
#endif
                                            //关闭此功能可以节省内存空间。
#if  NET_PLAY_PCM_OR_G722_ENABLE_LOCAL_PLAY
#define AUDIO_PLAYER_ENABLE             1   //是否启用音频播放器。0:不启用,1:启用。不使用播放功能时，启用本地mp3播放器多占用内存：默认播放器52K、精简播放器36K
#endif
#define PLAYER_CONTROL_PA               0   //是否有播放器控音频功放开关。0:功放常开,1:播放器在需要播放时才打开,但可能增加一点每一次播放的延迟时间
#define VOLUME_MAX                      7   //设置音量调节的上限值，对应硬件支持的最大音量。
#define VOLUME_MIN                      1   //设置音量调节的下限值，对应最小音量。
#define VOLUME_DEFAULT                  5   //设置音量调节的默认值。

#if AUDIO_PLAYER_ENABLE
#if SIMPLE_AUDIO_PLAYER_ENABLE && NET_AUDIO_PLAY_BY_MP3
#define MP3_NO_CONSTANT_BITRATE         0   //开启非恒定比特率的模式VBR、ABR适配
#else
#if NET_AUDIO_PLAY_BY_OPUS
#define USE_OPUS_DECODER                1   //为1时加入opus解码器，1:是 0:否。
#else
#define USE_MP3_DECODER                 1   //为1时加入mp3解码器，1:是 0:否。
#define AUDIO_PLAY_SUPPT_MP3_PROMPT     1   //播放器是否开启mp3提示音，1:是 0:否。
#define USE_PROMPT_DECODER              1   //播放器是否支持prompt解码器，1:是 0:否。
#endif
#endif
#endif

#define BF_DEEPSE_MODE                  1   //1:全深度分离更耗内存(单双网络都可以用) 0：半深度分离(唤醒词做深度分离，命令词不做，只能用双网络)
#define BF_ASR_VALID_MODE               0   //1:开启ASR打分是否有效判断功能 0：关闭ASR打分是否有效判断功能 该功能只针对全深度分离和半深度分离

#if USE_DEREVERB_MODULE
#define DEREVERB_FREQ_RANGE_INDEX       0  //默认0:算法起效频率160HZ-4800HZ 消耗28KB内存  1: 算法起效频率0-8000HZ 消耗49KB内存     
#endif
#if USE_AEC_MODULE
#define AEC_INTERRUPT_TYPE              2  //默认2: 命令词和唤醒词都可打断  1: 只有命令词能打断   0:只有唤醒词能打断
#endif
//**自学习功能-请在安静环境下，用清晰洪亮的声音进行指令学习，避免环境噪音过大和学习者声音过小导致学习不成功 
//**注意：在线SDK只支持唤醒词学习，为了避免指令词被学习成模版，请确保cmd_info.xls中词条语义ID、命令词ID与需学习的词条语义ID、命令词ID不重复                                    
#if USE_CWSL
#define CWSL_WAKEUP_NUMBER          2           // 可学习的唤醒词数量-最大支持2个
#define WAKE_UP_ID                  1           // 学习的唤醒词对应的命令词ID
#define CWSL_REG_TIMES              1           // 学习时 每个词需说几遍，默认 1 遍即可,支持1、2遍,FOR_REG_2TIMES_FLOW_V2 配置 1时,最大支持 3 遍;
#define CWSL_WAKEUP_THRESHOLD       37          // 学习的唤醒词阈值门限，越小越灵敏，默认 37, 最小可配置到 32;
#define CWSL_CMD_THRESHOLD          35          // 学习的命令词阈值门限，越小越灵敏，默认 35，最小可配置到 30；
#define FOR_REG_2TIMES_FLOW_V2      0           // 学习时，说两遍/三遍逻辑，版本二流程，后续均和第一次的比较，一致学习成功，不一致，最多支持说 3 次\
                                                     FOR_REG_2TIMES_FLOW_V2 配置 1时, CWSL_REG_TIMES 必须是 2或3	
#define CWSL_REG_VAD_LEVEL          0           // 学习过程，灵敏度选项配置： 0 低灵敏度，可减少噪声对学习的干扰，需学习过程大声说话；1 高灵敏度，但也可以导致干扰噪声干扰学习
#define CICWSL_TOTAL_TEMPLATE       CWSL_WAKEUP_NUMBER*3           //可存储模板数量

#if (CWSL_REG_TIMES == 3)
#define FOR_REG_2TIMES_FLOW_V2      1
#elif (CWSL_REG_TIMES > 3)
#error "The CWSL_REG_TIMES max 3\n"
#endif
#endif

#if USE_WMAN_VPR
#define VP_USE_FRM_LEN                  1200                            //声纹计算的窗长，单位为ms，建议范围1200-1500，值越大消耗内存越多（每增加100，内存增加8KB）
#define VP_CMPT_SKIP_NUM                0
#define VPT_SIZE                        (192*sizeof(float))             //模板大小-不可修改
#define NVDATA_ID_VP_NUMBER             NVDATA_ID_VP_MOULD_INFO         //存储已添加了的模板数量-不可修改
#define VP_SLIDE_TIME_PER_CMPT          1                               //声纹每次计算，滑窗的次数-不可修改
#define WMAN_PLAY_EN                    1                               //男女声纹识别播报
#endif
#if     USE_VPR
#define VP_USE_FRM_LEN                  1200      //声纹计算的窗长，单位为ms, 建议范围1200-1500，值越大消耗内存越多（每增加100，内存增加8KB）
#define VP_CMPT_SKIP_NUM                0         //-不可修改
#define VP_THR_FOR_MATCH                (0.52f)   //声纹阈值-建议范围(0.48-0.68)，值越大，灵敏度越低，误识越低，识别率下降，需要更严格的匹配注册的模版
#define VP_THR_FOR_SAME_MATCH           (0.50f)   //同一用户，判断是否重复所用声纹阈值-不可修改
#define VP_SLIDE_TIME_PER_CMPT          3         //声纹每次计算，滑窗-不可修改
#define VP_REC_TIMES                    3         //声纹注册时重复录入次数 -注册时的次数
#define MAX_VP_TEMPLATE_NUM             3         //声纹识别功能允许的最大模版(用户)数,最大4个 重要说明：每个模版单次约占0.8KB NV空间，三次2.4KB

#define MAX_VP_REG_TIME                 10        //注册声纹时最大超时等待时间（秒)
#define VPT_SIZE                        (192*sizeof(float))   //模板大小  -不可修改
#define NVDATA_ID_VP_NUMBER             0xA0000001      //存储模板数量NV基地址 -不可修改
#define NVDATA_ID_VP_INFO               0xA0000002      //存储模板ID NV基地址，每个用户模版数是重复录入次数-不可修改
                                                        //输出给用户的id就是（地址-0xA0000002/VP_REC_TIMES 
#define NVDATA_ID_VP_MODE               0xA0000003      //存储模板NV基地址 -不可修改
#if (MAX_VP_TEMPLATE_NUM > 4)
#error "The vpr template num max 4\n"
#endif
#endif

#if USE_SED_CRY || USE_SED_SNORE
#define NO_ASR_FLOW                     1         //不可修改
#if     USE_SED_CRY
#define THRESHOLD_CRY                   0.53f     //可根据具体需求修改,范围为(0~1)float类型-建议范围(0.5-0.6f),值越大，灵敏度越低
#define TIMES_CRY                       3         //可根据具体需求修改,最大5次(算法计算几次给结果)
#elif   USE_SED_SNORE
#define THRESHOLD_SNORE                 0.50f     //可根据具体需求修改,范围为(0~1)float类型-建议范围(0.5-0.6f),值越大，灵敏度越低
#define TIMES_SNORE                     3         //可根据具体需求修改,最大5次(算法计算几次给结果)
#endif

#if TIMES_CRY > 5
#error "The times should be less than or equal to 5\n"
#endif        
#endif

#if USE_NN_DOA
#if !USE_AEC_MODULE
#define NN_DOA_OUT_TYPE                 1         //doa输出类型：1-唤醒词输出角度  2-命令词输出角度 3-唤醒次和命令词都输出角度
#endif
#endif

#if USE_GCC_DOA
#define GCC_DOA_OUT_TYPE                 1        //doa输出类型：0-周期输出角度  1-唤醒词和命令词都输出角度
#endif

#if USE_BEAMFORMING_MODULE  || (USE_NN_DOA || USE_GCC_DOA) || USE_DEREVERB_MODULE || USE_DUAL_MIC_ANY
#if USE_CI_D12GS01J_BOARD
 #error "USE_CI_D12GS01J_BOARD not support dual mic alg !\n"    //131x不支持双mic算法
#endif
#define HOST_CODEC_CHA_NUM  2
#define OFFLINE_DUAL_MIC_ALG_SUPPORT    1
#else
#define HOST_CODEC_CHA_NUM              1
#define OFFLINE_DUAL_MIC_ALG_SUPPORT    0
#endif

#if USE_AEC_MODULE
    #define IF_JUST_CLOSE_HPOUT_WHILE_NO_PLAY   1
    #define HOST_CODEC_CHA_NUM  2
    #define OFFLINE_DUAL_MIC_ALG_SUPPORT    0

    #if USE_SED_SNORE || USE_SED_CRY
    #error "aec + sed detection alg not support!\n"    //事件检测不支持AEC
    #endif
#endif


#if USE_CI_D12GS01J_BOARD
#if USE_BEAMFORMING_MODULE  || (USE_NN_DOA || USE_GCC_DOA)|| USE_DEREVERB_MODULE || USE_DUAL_MIC_ANY
 #error "USE_CI_D12GS01J_BOARD not support aec and dual mic alg!\n"    //131x不支持aec和双 mic算法
#endif
#endif

#if (MIC_RECORD_IIS_SELECT&&IF_USE_ANOTHER_CODEC_TO_GET_REF)
#error "no support MIC_RECORD_IIS_SELECT && IF_USE_ANOTHER_CODEC_TO_GET_REF toghter\n"
#endif
#if REF_IN_FROM_INNER_CODEC && IF_USE_ANOTHER_CODEC_TO_GET_REF
#error "no support REF_IN_FROM_INNER_CODEC && IF_USE_ANOTHER_CODEC_TO_GET_REF toghter\n"
#endif

#if ((CI_CHIP_TYPE == 1302) && (MIC_RECORD_IIS_SELECT || USE_IIS1_OUT_PRE_RSLT_AUDIO\
    || IF_USE_ANOTHER_CODEC_TO_GET_REF) && AUDIO_IN_FROM_DMIC)
#error "1302, no support dmic && iis\n"
#endif

#if USE_BEAMFORMING_MODULE && USE_PWK
 #error "bf + pwk algorithm, not support\n"
#endif
//双麦算法和AEC在不接外部codec做AEC信号回采，不能同时使用
#if ((USE_NN_DOA || USE_GCC_DOA) || USE_DUAL_MIC_ANY)  && USE_AEC_MODULE
  #if AUDIO_IN_FROM_DMIC
    #if !IF_USE_ANOTHER_CODEC_TO_GET_REF && !REF_IN_FROM_INNER_CODEC
    #error "doa + aec algorithm, requires external codec1\n"
    #endif
  #else
    #if !IF_USE_ANOTHER_CODEC_TO_GET_REF
    #error "doa + aec algorithm, requires external codec\n"
    #endif
  #endif
#endif

#if USE_DEREVERB_MODULE || (USE_NN_DOA || USE_GCC_DOA)
    #if HOST_MIC_USE_NUMBER == 1
    #error "algorithm requires tow mic, please set HOST_MIC_USE_NUMBER = 2\n"
    #endif
#endif

#if (AUDIO_DATA_UPLOAD_BY_UART&&(USE_PWK))                       //语音上传功能不能开USE_PWK
#error "ONLY USE_NULL WITH NO PWK SUPPORT AUDIO_DATA_UPLOAD_BY_UART!"
#endif


#if USE_ALC_AUTO_SWITCH_MODULE && ((USE_NN_DOA || USE_GCC_DOA) || USE_DEREVERB_MODULE || USE_BEAMFORMING_MODULE || USE_AEC_MODULE)
#error "two mic alg, no alc auto\n"
#endif


//单麦就近唤醒 和 动态ALC和深度降噪,自学习，事件检测算法不能同时使用
#if USE_PWK && (USE_ALC_AUTO_SWITCH_MODULE || USE_NN_DENOISE || USE_CWSL || USE_SED_CRY || USE_SED_SNORE)
#error "can't be toghter pwk\n"
#endif

//语音模块协议 和 云端协议使能不能同时使用
#if MSG_COM_USE_UART_EN && CLOUD_UART_PROTOCOL_EN
#error "MSG_COM_USE_UART_EN and CLOUD_UART_PROTOCOL_EN, can't be toghter\n"
#endif
//开启红外功能必须用二代协议
#if USE_IR_ENABLE && (UART_PROTOCOL_VER != 2)
#error "use ir function, UART_PROTOCOL_VER must set 2!\n"
#endif
#if NET_AUDIO_PLAY_BY_OPUS || NET_AUDIO_PLAY_BY_G722
#if !SIMPLE_AUDIO_PLAYER_ENABLE
#error  "play opus or G722 audio data, must set makefile USE_AUDIO_PLAYER_TYPE = 0"
#endif
#endif
#if AUDIO_COMPRESS_SPEEX_ENABLE && AUDIO_COMPRESS_OPUS_ENABLE
#error  "audio compress not support opus and speex at the same time"
#elif AUDIO_COMPRESS_G722_ENABLE && AUDIO_COMPRESS_SPEEX_ENABLE
#error  "audio compress not support g722 and speex at the same time"
#elif AUDIO_COMPRESS_G722_ENABLE && AUDIO_COMPRESS_OPUS_ENABLE
#error  "audio compress not support g722 and opus at the same time"
#endif
#if NET_AUDIO_PLAY_BY_MP3 && NET_AUDIO_PLAY_BY_PCM
#error  "audio play not support mp3 and pcm at the same time"
#elif NET_AUDIO_PLAY_BY_MP3 && NET_AUDIO_PLAY_BY_OPUS
#error  "audio play not support mp3 and opus at the same time"
#elif NET_AUDIO_PLAY_BY_OPUS && NET_AUDIO_PLAY_BY_PCM
#error  "audio play not support opus and pcm at the same time"
#elif NET_AUDIO_PLAY_BY_G722 && NET_AUDIO_PLAY_BY_PCM
#error  "audio play not support g722 and pcm at the same time"
#elif NET_AUDIO_PLAY_BY_G722 && NET_AUDIO_PLAY_BY_OPUS
#error  "audio play not support g722 and opus at the same time"
#elif NET_AUDIO_PLAY_BY_G722 && NET_AUDIO_PLAY_BY_MP3
#error  "audio play not support g722 and mp3 at the same time"
#endif
//opus+mp3算法组合不支持自学习和双麦算法，内存不足
#if AUDIO_COMPRESS_OPUS_ENABLE && NET_AUDIO_PLAY_BY_MP3 && USE_CWSL
#error  "opus compress and mp3 play not support cwsl at the same time"
#endif
#if NET_AUDIO_PLAY_BY_OPUS && AUDIO_COMPRESS_OPUS_ENABLE
#error  "opus play and opus record not support at the same time"
#endif
#if (NET_AUDIO_PLAY_BY_MP3)&&(USE_NN_DOA || USE_GCC_DOA) && AUDIO_COMPRESS_OPUS_ENABLE
#error  "mp3 play and dual mic alg and opus compress not support at the same time"
#endif
#if (NET_PLAY_PCM_OR_G722_ENABLE_LOCAL_PLAY) && AUDIO_COMPRESS_OPUS_ENABLE
#error  "g722/mp3 play and local play and opus compress not support at the same time"
#endif
#if AUDIO_COMPRESS_SPEEX_ENABLE && NET_AUDIO_PLAY_BY_OPUS
#error  "speex compress and opus play not support at the same time"
#endif
#if (AIOT_AUDIO_COMPRESS_BY_G722 && !AUDIO_COMPRESS_G722_ENABLE) || (AUDIO_COMPRESS_G722_ENABLE && !AIOT_AUDIO_COMPRESS_BY_G722)
#error  "AUDIO_COMPRESS_G722_ENABLE must be AIOT_AUDIO_COMPRESS_TYPE = 3"
#endif
#if USE_EXTERNAL_CRYSTAL_OSC != 1
#error  "aiot application must be USE_EXTERNAL_CRYSTAL_OSC  = 1"
#endif




#endif /* _USER_CONFIG_H_ */
