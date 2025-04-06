#include "opus_decoder.h"
#include <QDebug>

OpusDecoder::OpusDecoder(QObject *parent)
    : QObject(parent)
    , decoder(nullptr)
    , sampleRate(16000)
    , channels(1)
    , frameDuration(60)
    , frameSize(0)
{
}

OpusDecoder::~OpusDecoder()
{
    cleanup();
}

bool OpusDecoder::initialize(int newSampleRate, int newChannels, int newFrameDuration)
{
    // 如果参数没有变化，不需要重新初始化
    if (decoder && sampleRate == newSampleRate && 
        channels == newChannels && frameDuration == newFrameDuration) {
        return true;
    }
    
    // 清理现有解码器
    cleanup();
    
    // 更新参数
    sampleRate = newSampleRate;
    channels = newChannels;
    frameDuration = newFrameDuration;
    frameSize = (sampleRate * frameDuration) / 1000;
    
    // 创建新的解码器
    int error;
    decoder = opus_decoder_create(sampleRate, channels, &error);
    if (error != OPUS_OK) {
        qDebug() << "Failed to create Opus decoder:" << error;
        return false;
    }
    
    return true;
}

QByteArray OpusDecoder::decode(const QByteArray& opusData)
{
    if (!decoder || opusData.isEmpty()) {
        return QByteArray();
    }
    
    // 分配PCM缓冲区
    QByteArray pcmData(frameSize * channels * sizeof(opus_int16), 0);
    
    // 解码Opus数据
    opus_int32 decodedSamples = opus_decode(
        decoder,
        reinterpret_cast<const unsigned char*>(opusData.constData()),
        opusData.size(),
        reinterpret_cast<opus_int16*>(pcmData.data()),
        frameSize,
        0  // 不使用FEC
    );
    
    if (decodedSamples < 0) {
        qDebug() << "解码失败:" << decodedSamples;
        return QByteArray();
    }
    
    // 调整缓冲区大小为实际解码的样本数
    pcmData.resize(decodedSamples * channels * sizeof(opus_int16));
    
    return pcmData;
}

void OpusDecoder::cleanup()
{
    if (decoder) {
        opus_decoder_destroy(decoder);
        decoder = nullptr;
    }
} 