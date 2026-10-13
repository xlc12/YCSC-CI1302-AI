/*该文件提取算法公共api*/
#ifndef __CI_ALG_COMMON_API__
#define __CI_ALG_COMMON_API__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <ci_log.h>
#include <malloc.h>
#include "ci_basic_alg.h"
#include "status_share.h"
#include "romlib_runtime.h"
#include "ci_log_config.h"
#include "alg_preprocess.h"
#include "status_share.h"
#include "user_config.h"
#include "remote_api_for_bnpu.h"
#include "sdk_default_config.h"
/**
 * @brief 算法通用接口标志
 */
typedef enum
{
    CI_COMMON_UPSAMPLE          =  (1<<1),    /*升采样接口 */
    CI_COMMON_DOWNSAMPLE        =  (1<<2),    /*降采样接口 */
    /* 保留 */
}common_api_type_t;

typedef struct
{ 
    /*api 接口选择*/
    uint32_t common_api_flag;
    /*降采样*/
    int16_t lowpass_filter_size;
    const float * lowpass_filter_addr;
    int16_t * left_delay;   //延时线队列
    int16_t * right_delay;
    int left_wr_idx;   // 延时线读指针，下一个要写入的位置
    int right_wr_idx;
    /*升采样*/
    int16_t prev_l;
    int16_t prev_r;
    bool first_frame;   // 标识是否是第一次调用
}ci_common_handle_t;

#ifdef __cplusplus
extern "C"
{
#endif
/**
 * @brief 创建3代 bnpu- cpu通用模块句柄
 * @param common_api_flag 3代 bnpu- cpu通用api标志，按照common_api_type_t枚举赋值
 * @param lowpass_filter_size 降采样低通滤波器长度
 * @param lowpass_filter_addr 降采样低通滤波器地址
 *  lowpass_filter_size为0，lowpass_filter_addr为NULL则使用默认使用4阶低通滤波器
 * @return ci_common_handle_t* 句柄，NULL 创建识别
 */
ci_common_handle_t* ci_common_create(uint32_t common_api_flag,int16_t lowpass_filter_size,const float * lowpass_filter_addr);
/**
 * @brief 线性插值升采样（无抗镜像滤波）
 * @param in     输入音频地址（较低采样率）
 * @param in_num_frames  输入帧数
 * @param out    输出音频地址（较高采样率）
 * @param out_num_frames 输出帧数
 * @param stereo_flag   是否为双声道
 * @return 0 成功，-1 参数错误
 * @note 升采样因子 = out_num_frames / in_num_frames，必须为整数
 *       不做低通滤波，直接线性插值，会产生镜像频谱
 */
int16_t upsample_linear_interp(ci_common_handle_t* handle,int16_t *in, int in_num_frames,
                               int16_t *out, int out_num_frames,
                               bool stereo_flag);
/**
 * @brief 将音频降采样到目标采样率
 * @param in     输入音频地址
 * @param in_num_frames  输入音频的每帧采样数（16k为256个short）
 * @param out    输出音频地址
 * @param out_num_frames    输出音频的每帧采样数
 * @param stereo_flag   是否为双声道 true or false
 * @return 成功为0 ， 失败为-1检查输入参数
 * @note in_num_frames和out_num_frames必须为整数倍，确保输入和输出地址已申请了足够的空间
 *  4阶低通滤波器（适用于16降采样到8k）示例:
 *  #define LOWPASS_FILTER_SIZE 4
 *  static const float LOWPASS_FILTER[LOWPASS_FILTER_SIZE] = {0.0167f,0.4833f,0.4833f,0.0167f};
 */
int16_t downsample_process(ci_common_handle_t* handle,int16_t *in,int in_num_frames, int16_t *out, int out_num_frames,bool stereo_flag);
#ifdef __cplusplus
}
#endif


#endif  ///__CI_ALG_COMMON_API__