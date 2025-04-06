#include "speaker_manager.h"
#include <QDebug>

SpeakerManager::SpeakerManager(QObject *parent)
    : QObject(parent)
    , audioSink(nullptr)
    , audioOutput(nullptr)
    , sampleRate(24000)  // 使用24kHz采样率
    , channels(1)
    , playing(false)
    , bufferSize(0)
{
    // 配置默认音频格式
    format.setSampleRate(sampleRate);
    format.setChannelCount(channels);
    format.setSampleFormat(QAudioFormat::Int16);
    
    initializeAudioDevice();
}

SpeakerManager::~SpeakerManager()
{
    stopPlaying();
    delete audioSink;
}

void SpeakerManager::calculateBufferSize()
{
    const int BYTES_PER_SAMPLE = 2;  // 16位采样 = 2字节
    const int BUFFER_DURATION_MS = 60;  // 使用60ms的缓冲区
    bufferSize = sampleRate * channels * BYTES_PER_SAMPLE * (BUFFER_DURATION_MS / 1000.0);
}

bool SpeakerManager::initializeAudioDevice()
{
    // 检查默认音频输出设备
    QAudioDevice outputDevice = QMediaDevices::defaultAudioOutput();
    if (!outputDevice.isFormatSupported(format)) {
        qDebug() << "默认音频输出设备不支持当前格式";
        return false;
    }
    
    // 创建音频输出接收器
    if (audioSink) {
        delete audioSink;
    }
    audioSink = new QAudioSink(format, this);
    
    // 计算并设置缓冲区大小
    calculateBufferSize();
    audioSink->setBufferSize(bufferSize);
    
    audioSink->setVolume(1.0);
    
    // 连接状态变化信号
    connect(audioSink, &QAudioSink::stateChanged, this, &SpeakerManager::handleStateChanged);
    
    qDebug() << "音频播放参数:"
             << "\n  采样率:" << sampleRate
             << "\n  通道数:" << channels
             << "\n  采样格式:" << format.sampleFormat()
             << "\n  缓冲区大小:" << bufferSize << "字节"
             << "\n  设备名称:" << QMediaDevices::defaultAudioOutput().description();
    
    return true;
}

void SpeakerManager::configureAudioParams(int newSampleRate, int newChannels)
{
    if (sampleRate == newSampleRate && channels == newChannels) {
        return;
    }
    
    sampleRate = newSampleRate;
    channels = newChannels;
    
    // 更新音频格式
    format.setSampleRate(sampleRate);
    format.setChannelCount(channels);
    
    // 重新初始化设备
    if (playing) {
        stopPlaying();
    }
    initializeAudioDevice();
}

void SpeakerManager::playPCM(const QByteArray& pcmData)
{
    if (pcmData.isEmpty()) {
        return;
    }
    
    processAudioData(pcmData);
}

void SpeakerManager::processAudioData(const QByteArray& data)
{
    // 确保音频输出设备就绪
    if (!audioOutput || !audioOutput->isWritable()) {
        if (audioSink->state() != QAudio::StoppedState) {
            audioSink->stop();
        }
        audioOutput = audioSink->start();
        if (!audioOutput) {
            qDebug() << "无法启动音频输出设备";
            return;
        }
    }
    
    // 写入音频数据
    qint64 written = audioOutput->write(data);
    if (written != data.size()) {
        qDebug() << "音频数据写入不完整:" << written << "/" << data.size();
    }
    
    playing = true;
}

void SpeakerManager::handleStateChanged(QAudio::State state)
{
    switch (state) {
        case QAudio::IdleState:
            // 播放完成
            playing = false;
            break;
        case QAudio::StoppedState:
            // 播放停止
            playing = false;
            break;
        default:
            break;
    }
}

void SpeakerManager::stopPlaying()
{
    if (audioOutput) {
        audioOutput = nullptr;
    }
    if (audioSink && audioSink->state() != QAudio::StoppedState) {
        audioSink->stop();
    }
    
    playing = false;
} 