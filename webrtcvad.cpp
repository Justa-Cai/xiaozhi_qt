#include "webrtcvad.h"
#include <QDebug>

WebRTCVad::WebRTCVad(QObject *parent)
    : QObject(parent)
    , handle(nullptr)
    , sample_rate(16000)
    , initialized(false)
{
}

WebRTCVad::~WebRTCVad()
{
    if (handle) {
        fvad_free(handle);
        handle = nullptr;
    }
}

bool WebRTCVad::init(int sampleRate)
{
    if (handle) {
        fvad_free(handle);
        handle = nullptr;
    }

    // 检查采样率是否合法
    if (sampleRate != 8000 && sampleRate != 16000 && 
        sampleRate != 32000 && sampleRate != 48000) {
        qDebug() << "不支持的采样率:" << sampleRate;
        return false;
    }

    this->sample_rate = sampleRate;
    
    // 创建VAD实例
    handle = fvad_new();
    if (!handle) {
        qDebug() << "创建WebRTC VAD实例失败";
        return false;
    }

    // 设置采样率
    if (fvad_set_sample_rate(handle, sampleRate) < 0) {
        qDebug() << "设置采样率失败";
        fvad_free(handle);
        handle = nullptr;
        return false;
    }

    initialized = true;
    
    // 默认使用中等激进程度
    setMode(2);
    
    qDebug() << "WebRTC VAD初始化成功，采样率:" << sampleRate;
    return true;
}

bool WebRTCVad::setMode(int mode)
{
    if (!initialized || !handle) {
        qDebug() << "VAD未初始化";
        return false;
    }

    // 检查模式是否合法
    if (mode < 0 || mode > 3) {
        qDebug() << "不合法的VAD模式:" << mode;
        return false;
    }

    if (fvad_set_mode(handle, mode) < 0) {
        qDebug() << "设置VAD模式失败";
        return false;
    }

    qDebug() << "设置VAD模式成功:" << mode;
    return true;
}

bool WebRTCVad::process(const int16_t* audio_frame, size_t frame_length)
{
    if (!initialized || !handle) {
        qDebug() << "VAD未初始化";
        return false;
    }

    // 计算帧时长（毫秒）
    int frame_duration_ms = (frame_length * 1000) / sample_rate;
    
    // WebRTC VAD要求帧长度为10ms、20ms或30ms的数据
    if (frame_duration_ms != 10 && frame_duration_ms != 20 && frame_duration_ms != 30) {
        // 如果是60ms的帧，我们将其分成3个20ms的子帧处理
        if (frame_duration_ms == 60) {
            size_t sub_frame_length = frame_length / 3;
            bool has_voice = false;
            
            for (size_t i = 0; i < 3; i++) {
                int ret = fvad_process(handle,
                                     audio_frame + i * sub_frame_length,
                                     sub_frame_length);
                if (ret < 0) {
                    qDebug() << "VAD处理子帧" << i << "失败";
                    continue;
                }
                if (ret == 1) {
                    has_voice = true;
                }
            }
            return has_voice;
        }
        
        qDebug() << "不支持的帧时长:" << frame_duration_ms << "ms";
        return false;
    }

    int ret = fvad_process(handle, 
                          audio_frame,
                          frame_length);

    if (ret < 0) {
        qDebug() << "VAD处理失败";
        return false;
    }

    return (ret == 1);  // 1表示检测到语音
} 