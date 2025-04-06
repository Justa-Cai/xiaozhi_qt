#ifndef SPEAKER_MANAGER_H
#define SPEAKER_MANAGER_H

#include <QObject>
#include <QAudioSink>
#include <QBuffer>
#include <QQueue>
#include <QMutex>
#include <QMediaDevices>

class SpeakerManager : public QObject {
    Q_OBJECT
public:
    explicit SpeakerManager(QObject *parent = nullptr);
    ~SpeakerManager();

    // 播放PCM音频数据
    void playPCM(const QByteArray& pcmData);
    
    // 停止播放
    void stopPlaying();
    
    // 是否正在播放
    bool isPlaying() const { return playing; }
    
    // 配置音频参数
    void configureAudioParams(int sampleRate, int channels);

private slots:
    void handleStateChanged(QAudio::State state);

private:
    // 初始化音频设备
    bool initializeAudioDevice();
    
    // 处理音频数据
    void processAudioData(const QByteArray& data);

    // 计算缓冲区大小
    void calculateBufferSize();

    QAudioSink* audioSink;
    QIODevice* audioOutput;
    QAudioFormat format;
    QByteArray currentAudioData;
    
    int sampleRate;
    int channels;
    bool playing;
    int bufferSize;  // 缓冲区大小（字节）
};

#endif // SPEAKER_MANAGER_H 