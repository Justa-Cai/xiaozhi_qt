#include "opus_encoder.h"
#include <QDebug>

OpusEncoder::OpusEncoder(QObject *parent)
    : QObject(parent)
    , encoder(nullptr)
    , sampleRate(16000)
    , channels(1)
    , frameDuration(20)
    , frameSize(0)
{
}

OpusEncoder::~OpusEncoder()
{
    cleanup();
}

bool OpusEncoder::initialize(int newSampleRate, int newChannels, int newFrameDuration)
{
    // 如果参数没有变化，不需要重新初始化
    if (encoder && sampleRate == newSampleRate && 
        channels == newChannels && frameDuration == newFrameDuration) {
        return true;
    }
    
    // 清理现有编码器
    cleanup();
    
    // 更新参数
    sampleRate = newSampleRate;
    channels = newChannels;
    frameDuration = newFrameDuration;
    frameSize = (sampleRate * frameDuration) / 1000;
    
    // 创建新的编码器
    int error;
    encoder = opus_encoder_create(sampleRate, channels, 
                                OPUS_APPLICATION_VOIP, &error);
    if (error != OPUS_OK) {
        qDebug() << "Failed to create Opus encoder:" << error;
        return false;
    }
    
    // 配置编码器参数
    // 根据采样率和通道数计算合适的比特率
    int bitrate = channels * sampleRate;  // 基础比特率
    if (sampleRate <= 8000) {
        bitrate = channels * 12000;  // 8kHz采样率使用12kbps/通道
    } else if (sampleRate <= 16000) {
        bitrate = channels * 20000;  // 16kHz采样率使用20kbps/通道
    } else if (sampleRate <= 24000) {
        bitrate = channels * 32000;  // 24kHz采样率使用32kbps/通道
    } else {
        bitrate = channels * 48000;  // 更高采样率使用48kbps/通道
    }
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(bitrate));
    opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(10));
    opus_encoder_ctl(encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    opus_encoder_ctl(encoder, OPUS_SET_VBR(1));
    opus_encoder_ctl(encoder, OPUS_SET_VBR_CONSTRAINT(1));
    opus_encoder_ctl(encoder, OPUS_SET_FORCE_CHANNELS(1));
    opus_encoder_ctl(encoder, OPUS_SET_APPLICATION(OPUS_APPLICATION_VOIP));
    opus_encoder_ctl(encoder, OPUS_SET_LSB_DEPTH(16));
    opus_encoder_ctl(encoder, OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_60_MS));
    
    return true;
}

QByteArray OpusEncoder::encode(const QByteArray& pcmData)
{
    if (!encoder || pcmData.isEmpty()) {
        return QByteArray();
    }
    
    // 检查输入数据大小是否正确
    int frameBytes = frameSize * channels * sizeof(opus_int16);
    if (pcmData.size() != frameBytes) {
        qDebug() << "Invalid PCM data size:" << pcmData.size() 
                 << "expected:" << frameBytes;
        return QByteArray();
    }
    
    // 分配输出缓冲区
    unsigned char output[4000];  // 足够大的缓冲区
    
    // 编码PCM数据
    opus_int16* pcm = (opus_int16*)pcmData.data();
    int len = opus_encode(encoder, pcm, frameSize, output, 4000);
    if (len < 0) {
        qDebug() << "Failed to encode:" << len;
        return QByteArray();
    }
    
    // 直接返回opus编码数据
    return QByteArray((char*)output, len);
}

void OpusEncoder::cleanup()
{
    if (encoder) {
        opus_encoder_destroy(encoder);
        encoder = nullptr;
    }
} 