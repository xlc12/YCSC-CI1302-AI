#include "ci_alg_common_api.h"

#define LOWPASS_FILTER_SIZE 4
static const float lowpass[LOWPASS_FILTER_SIZE] = {0.0167f,0.4833f,0.4833f,0.0167f};

/**
 * @brief 创建3代 bnpu- cpu通用模块句柄
 * @param common_api_flag 3代 bnpu- cpu通用api标志，按照common_api_type_t枚举赋值
 * @param lowpass_filter_size 降采样低通滤波器长度
 * @param lowpass_filter_addr 降采样低通滤波器地址
 *  lowpass_filter_size为0，lowpass_filter_addr为NULL则使用默认使用4阶低通滤波器
 * @return ci_common_handle_t* 句柄，NULL 创建识别
 */
ci_common_handle_t* ci_common_create(uint32_t common_api_flag,int16_t lowpass_filter_size,const float * lowpass_filter_addr)
{
    if(0 == (common_api_flag & 0xFFFFFF))
    {
        return NULL;
    }
    ci_common_handle_t * handle = (ci_common_handle_t*)pvPortMalloc(sizeof(ci_common_handle_t));
    if(!handle) 
    {
        return NULL;
    }
    memset(handle,0,sizeof(ci_common_handle_t));
    if (0 != (common_api_flag & CI_COMMON_DOWNSAMPLE))
    {   
        if(lowpass_filter_size != 0 && lowpass_filter_addr != NULL)
        {
            handle->lowpass_filter_size = lowpass_filter_size;
            handle->lowpass_filter_addr = lowpass_filter_addr;
        }
        else
        {
            handle->lowpass_filter_size = LOWPASS_FILTER_SIZE;
            handle->lowpass_filter_addr = lowpass;
        }
        handle->left_wr_idx = 0;
        handle->right_wr_idx = 0;
        handle->left_delay = pvPortMalloc(handle->lowpass_filter_size*sizeof(int16_t));
        handle->right_delay = pvPortMalloc(handle->lowpass_filter_size*sizeof(int16_t));
        if(handle->left_delay == NULL || handle->right_delay == NULL || handle->lowpass_filter_addr == NULL )
        {
            return NULL;
        }
    }
    if (0 != (common_api_flag & CI_COMMON_UPSAMPLE))
    {
        handle->prev_l = 0;
        handle->prev_r = 0;
        handle->first_frame = true;   // 标识是否是第一次调用
    }
    return handle;
}
// #pragma GCC optimize("O3")
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
                               bool stereo_flag)
{
    if (in == NULL || out == NULL || out_num_frames % in_num_frames != 0)
        return -1;

    int upsamp_factor = out_num_frames / in_num_frames;   // 升采样因子，例如 3

    // 静态变量，保存上一帧样本（跨调用保持）


    int out_idx = 0;   // 输出缓冲区索引

    for (int i = 0; i < in_num_frames; i++)
    {
        // 读取当前输入样本
        int16_t l_cur = stereo_flag ? in[2 * i] : in[i];
        int16_t r_cur = stereo_flag ? in[2 * i + 1] : 0;

        // 第一帧特殊处理：没有历史样本，所有输出复制当前样本
        if (handle->first_frame )
        {
            for (int k = 0; k < upsamp_factor; k++)
            {
                if (stereo_flag)
                {
                    out[2 * out_idx]     = l_cur;
                    out[2 * out_idx + 1] = r_cur;
                }
                else
                {
                    out[out_idx] = l_cur;
                }
                out_idx++;
            }
        }
        else
        {
            // 使用上一帧和当前帧进行线性插值
            for (int k = 0; k < upsamp_factor; k++)
            {
                float alpha = (float)k / upsamp_factor;   // 插值权重，0 <= alpha < 1
                // 左声道
                int32_t l_tmp = (int32_t)((1.0f - alpha) * handle->prev_l + alpha * l_cur);
                int16_t l_out = (l_tmp >= 32767) ? 32767 : ((l_tmp <= -32767) ? -32767 : (int16_t)l_tmp);
                int16_t r_out = 0;
                if (stereo_flag)
                {
                    int32_t r_tmp = (int32_t)((1.0f - alpha) * handle->prev_r + alpha * r_cur);
                    r_out = (r_tmp >= 32767) ? 32767 : ((r_tmp <= -32767) ? -32767 : (int16_t)r_tmp);
                }
                // 写入输出
                if (stereo_flag)
                {
                    out[2 * out_idx]     = l_out;
                    out[2 * out_idx + 1] = r_out;
                }
                else
                {
                    out[out_idx] = l_out;
                }
                out_idx++;
            }
        }

        // 更新上一帧样本（用于下一次调用）
        handle->prev_l = l_cur;
        handle->prev_r = r_cur;
    }

    // 下一次调用不再视为第一帧（除非外部重置）
    if (handle->first_frame )
        handle->first_frame  = false;

    return 0;
}
/**
 * @brief 将音频降采样到目标采样率
 * @param in     输入音频地址
 * @param in_num_frames  输入音频的每帧采样数（16k为256个short）
 * @param out    输出音频地址
 * @param out_num_frames    输出音频的每帧采样数
 * @param stereo_flag   是否为双声道 true or false
 * @return 成功为0 ， 失败为-1检查输入参数
 * @note in_num_frames和out_num_frames必须为整数倍，确保输入和输出地址已申请了足够的空间
 */
int16_t downsample_process(ci_common_handle_t* handle,int16_t *in,int in_num_frames, int16_t *out, int out_num_frames,bool stereo_flag)
{
    if (in == NULL || out == NULL || in_num_frames % out_num_frames != 0)
        return -1;

    int downsamp_factor = in_num_frames / out_num_frames;
    int delay_len = handle->lowpass_filter_size - 1;   // 历史样本数量

    // 环形延迟线：每个声道存储最近 delay_len 个历史样本（不包含当前输入）
    int out_idx = 0;

    for (int i = 0; i < in_num_frames; i++)
    {
        // 读取当前输入样本
        int16_t l_in = stereo_flag ? in[2 * i] : in[i];
        int16_t r_in = stereo_flag ? in[2 * i + 1] : 0;

        // ---------- 左声道 FIR 滤波 ----------
        // 卷积：y[n] = b[0]*x[n] + b[1]*x[n-1] + ... + b[L-1]*x[n-L+1]
        int32_t l_tmp = (int32_t)(handle->lowpass_filter_addr[0] * l_in);   // 当前样本乘 b0
        
        // 从延迟线中取出历史样本（按时间从旧到新）
        int idx = handle->left_wr_idx;   // 最旧的历史样本位置
        for (int j = 1; j < handle->lowpass_filter_size; j++)
        {
            l_tmp += (int32_t)(handle->lowpass_filter_addr[j] * handle->left_delay[idx]);
            idx = (idx + 1) % delay_len;
        }
        
        // 防溢出
        int16_t l_out;
        if (l_tmp >= 32767) l_out = 32767;
        else if (l_tmp <= -32767) l_out = -32767;
        else l_out = (int16_t)l_tmp;

        // 更新左声道延迟线：将当前样本存入，覆盖最旧的历史样本
        handle->left_delay[handle->left_wr_idx] = l_in;
        handle->left_wr_idx = (handle->left_wr_idx + 1) % delay_len;

        // ---------- 右声道（双声道时） ----------
        int16_t r_out = 0;
        if (stereo_flag)
        {
            int32_t r_tmp = (int32_t)(handle->lowpass_filter_addr[0] * r_in);
            idx = handle->right_wr_idx;
            for (int j = 1; j < handle->lowpass_filter_size; j++)
            {
                r_tmp += (int32_t)(handle->lowpass_filter_addr[j] * handle->right_delay[idx]);
                idx = (idx + 1) % delay_len;
            }
            if (r_tmp >= 32767) r_out = 32767;
            else if (r_tmp <= -32767) r_out = -32767;
            else r_out = (int16_t)r_tmp;

            handle->right_delay[handle->right_wr_idx] = r_in;
            handle->right_wr_idx = (handle->right_wr_idx + 1) % delay_len;
        }

        // 降采样抽取
        if (i % downsamp_factor == 0)
        {
            if (stereo_flag)
            {
                out[2 * out_idx]     = l_out;
                out[2 * out_idx + 1] = r_out;
            }
            else
            {
                out[out_idx] = l_out;
            }
            out_idx++;
        }
    }
    return 0;
}
