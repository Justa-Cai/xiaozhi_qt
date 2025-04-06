#ifndef MICROPHONE_MANAGER_H
#define MICROPHONE_MANAGER_H

#include <QObject>
#include <QAudioSource>
#include <QMediaDevices>

class MicrophoneManager : public QObject {
    Q_OBJECT
public:
    explicit MicrophoneManager(QObject *parent = nullptr);
    ~MicrophoneManager();

    // 开始录音
    bool startRecording();
    
    // 停止录音
    void stopRecording();
    
    // 是否正在录音
    bool isRecording() const { return recording; }
    
    // 配置音频参数
    void configureAudioParams(int sampleRate, int channels);

signals:
    // 当有新的PCM音频数据时发出信号
    void pcmDataReady(const QByteArray& pcmData);

private slots:
    void handleStateChanged(QAudio::State state);
    void handleReadyRead();

private:
    // 初始化音频设备
    bool initializeAudioDevice();
    
    // 检查设备状态
    bool checkDeviceHealth();

    QAudioSource* audioSource;
    QIODevice* audioIO;
    QAudioFormat format;
    
    int sampleRate;
    int channels;
    int frameSize;  // 每帧采样数
    bool recording;
};

#endif // MICROPHONE_MANAGER_H 