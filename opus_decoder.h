#ifndef OPUS_DECODER_H
#define OPUS_DECODER_H

#include <QObject>
#include <opus/opus.h>

class OpusDecoder : public QObject {
    Q_OBJECT
public:
    explicit OpusDecoder(QObject *parent = nullptr);
    ~OpusDecoder();

    // 初始化解码器
    bool initialize(int sampleRate, int channels, int frameDuration);
    
    // 解码Opus数据
    QByteArray decode(const QByteArray& opusData);
    
    // 获取当前帧大小(采样数)
    int getFrameSize() const { return frameSize; }

private:
    OpusDecoder* decoder;
    int sampleRate;
    int channels;
    int frameDuration;  // 毫秒
    int frameSize;      // 每帧采样数
    
    // 清理解码器
    void cleanup();
};

#endif // OPUS_DECODER_H 