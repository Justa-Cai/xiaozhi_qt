#ifndef OPUS_ENCODER_H
#define OPUS_ENCODER_H

#include <QObject>
#include <opus/opus.h>

class OpusEncoder : public QObject {
    Q_OBJECT
public:
    explicit OpusEncoder(QObject *parent = nullptr);
    ~OpusEncoder();

    // 初始化编码器
    bool initialize(int sampleRate, int channels, int frameDuration);
    
    // 编码PCM数据
    QByteArray encode(const QByteArray& pcmData);
    
    // 获取当前帧大小(采样数)
    int getFrameSize() const { return frameSize; }

private:
    OpusEncoder* encoder;
    int sampleRate;
    int channels;
    int frameDuration;  // 毫秒
    int frameSize;      // 每帧采样数
    
    // 清理编码器
    void cleanup();
};

#endif // OPUS_ENCODER_H 