#include "microphone_manager.h"
#include <QDebug>
#include <QTimer>
#include <QFile>

MicrophoneManager::MicrophoneManager(QObject *parent)
    : QObject(parent)
    , audioSource(nullptr)
    , audioIO(nullptr)
    , sampleRate(16000)
    , channels(1)
    , frameSize(320*3)  // 20ms @ 16kHz
    , recording(false)
{
    // 配置默认音频格式
    format.setSampleRate(sampleRate);
    format.setChannelCount(channels);
    format.setSampleFormat(QAudioFormat::Int16);
    
    initializeAudioDevice();
}

MicrophoneManager::~MicrophoneManager()
{
    stopRecording();
    delete audioSource;
}

bool MicrophoneManager::initializeAudioDevice()
{
    // 检查默认音频输入设备
    QAudioDevice inputDevice = QMediaDevices::defaultAudioInput();
    qDebug() << "默认音频输入设备信息:"
             << "\n  设备名称:" << inputDevice.description()
             << "\n  最小采样率:" << inputDevice.minimumSampleRate()
             << "\n  最大采样率:" << inputDevice.maximumSampleRate()
             << "\n  最小通道数:" << inputDevice.minimumChannelCount()
             << "\n  最大通道数:" << inputDevice.maximumChannelCount()
             << "\n  支持的采样格式:" << inputDevice.supportedSampleFormats();
    
    if (!inputDevice.isFormatSupported(format)) {
        qDebug() << "默认音频输入设备不支持当前格式:"
                 << "\n  采样率:" << format.sampleRate()
                 << "\n  通道数:" << format.channelCount()
                 << "\n  采样格式:" << format.sampleFormat();
                 
        // 尝试调整格式
        QAudioFormat newFormat = inputDevice.preferredFormat();
        qDebug() << "设备推荐的音频格式:"
                 << "\n  采样率:" << newFormat.sampleRate()
                 << "\n  通道数:" << newFormat.channelCount()
                 << "\n  采样格式:" << newFormat.sampleFormat();
                 
        format = newFormat;
        sampleRate = format.sampleRate();
        channels = format.channelCount();
        frameSize = (sampleRate * 60) / 1000;  // 60ms帧
    }
    
    // 创建音频输入源
    if (audioSource) {
        delete audioSource;
    }
    audioSource = new QAudioSource(inputDevice, format, this);
    
    return true;
}

void MicrophoneManager::configureAudioParams(int newSampleRate, int newChannels)
{
    if (sampleRate == newSampleRate && channels == newChannels) {
        return;
    }
    
    sampleRate = newSampleRate;
    channels = newChannels;
    frameSize = (sampleRate * 60) / 1000;  // 60ms帧
    
    // 更新音频格式
    format.setSampleRate(sampleRate);
    format.setChannelCount(channels);
    
    // 重新初始化设备
    if (recording) {
        stopRecording();
    }
    initializeAudioDevice();
}

bool MicrophoneManager::startRecording()
{
    qDebug() << "MicrophoneManager::startRecording: 开始录音";
    
    if (recording || !audioSource) {
        qDebug() << "录音失败: recording=" << recording 
                 << ", audioSource=" << (audioSource != nullptr);
        return false;
    }
    
    if (!checkDeviceHealth()) {
        qDebug() << "录音失败: 设备状态检查未通过";
        return false;
    }
    
    // 设置音频输入参数
    int periodSize = frameSize * 2;  // 一个周期的采样数
    int bufferSize = periodSize * 16; // 缓冲区大小为4个周期
    audioSource->setBufferSize(bufferSize * format.bytesPerFrame());
    audioSource->setVolume(1.0);
    
    qDebug() << "音频参数已设置:"
             << "\n  帧大小:" << frameSize
             << "\n  周期大小:" << periodSize
             << "\n  缓冲区大小:" << bufferSize
             << "\n  字节/帧:" << format.bytesPerFrame()
             << "\n  采样率:" << format.sampleRate()
             << "\n  通道数:" << format.channelCount()
             << "\n  采样格式:" << format.sampleFormat();
    
    // 连接信号
    connect(audioSource, &QAudioSource::stateChanged,
            this, &MicrophoneManager::handleStateChanged,
            Qt::UniqueConnection);
    
    // 启动录音
    audioIO = audioSource->start();
    if (!audioIO) {
        qDebug() << "录音失败: 无法获取音频IO设备";
        return false;
    }
    
    qDebug() << "音频IO设备已打开，状态:" << audioIO->isOpen()
             << ", 可读:" << audioIO->isReadable();
    
    connect(audioIO, &QIODevice::readyRead,
            this, &MicrophoneManager::handleReadyRead,
            Qt::UniqueConnection);
    
    recording = true;
    qDebug() << "录音已成功启动";
    return true;
}

void MicrophoneManager::stopRecording()
{
    if (!recording) {
        return;
    }
    
    if (audioSource) {
        audioSource->stop();
        disconnect(audioSource, &QAudioSource::stateChanged,
                  this, &MicrophoneManager::handleStateChanged);
    }
    
    if (audioIO) {
        disconnect(audioIO, &QIODevice::readyRead,
                  this, &MicrophoneManager::handleReadyRead);
        audioIO = nullptr;
    }
    
    recording = false;
    qDebug() << "录音已停止";
}

void MicrophoneManager::handleStateChanged(QAudio::State state)
{
    qDebug() << "音频设备状态变化:" << state;
    
    if (state == QAudio::StoppedState) {
        if (audioSource->error() != QAudio::NoError) {
            qDebug() << "音频设备错误:" << audioSource->error();
            stopRecording();
        }
    } else if (state == QAudio::ActiveState) {
        qDebug() << "音频设备已进入活动状态";
    }
}

void MicrophoneManager::handleReadyRead()
{
    if (!recording || !audioIO) {
        return;
    }

    // 计算当前可用的数据大小
    qint64 bytesAvailable = audioIO->bytesAvailable();
    int frameBytes = frameSize * format.bytesPerFrame();
    
    // 如果数据不足一帧，等待下次读取
    if (bytesAvailable < frameBytes) {
        return;
    }

    // 读取一帧数据
    QByteArray frameData = audioIO->read(frameBytes);
    if (!frameData.isEmpty()) {
        // 将PCM数据保存到文件用于调试
        static QFile debugFile("send.pcm");
        static bool fileOpened = false;
        
        if (!fileOpened) {
            if (debugFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
                fileOpened = true;
                qDebug() << "调试文件已打开";
            } else {
                qDebug() << "无法打开调试文件";
            }
        }
        
        if (fileOpened) {
            debugFile.write(frameData);
        }
        // qDebug() << "handleReadyRead: 发送PCM数据，大小" << frameData.size() << "字节";
        emit pcmDataReady(frameData);
    }
}

bool MicrophoneManager::checkDeviceHealth()
{
    QAudioDevice inputDevice = QMediaDevices::defaultAudioInput();
    if (!inputDevice.isFormatSupported(format)) {
        qDebug() << "默认音频输入设备不支持当前格式";
        return false;
    }
    return true;
} 