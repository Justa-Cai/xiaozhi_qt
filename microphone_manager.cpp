#include "microphone_manager.h"
#include <QDebug>
#include <QTimer>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>

MicrophoneManager::MicrophoneManager(QObject *parent)
    : QObject(parent)
    , audioSource(nullptr)
    , audioDevice(nullptr)
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
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    // 检查格式是否支持
    QAudioDevice inputDevice = QMediaDevices::defaultAudioInput();
    if (!inputDevice.isFormatSupported(format)) {
        qDebug() << "默认音频输入设备不支持请求的格式";
        return false;
    }

    // 检查设备是否可用
    if (!inputDevice.isNull()) {
        audioSource = new QAudioSource(format, this);
        if (audioSource) {
            // 设置缓冲区大小（可选）
            audioSource->setBufferSize(4096);

            // 打开设备进行录音
            audioDevice = audioSource->start();
            if (audioDevice) {
                connect(audioDevice, &QIODevice::readyRead, this, &MicrophoneManager::handleReadyRead);
                recording = true;
                qDebug() << "录音开始成功";
                return true;
            } else {
                qDebug() << "打开音频设备失败";
                delete audioSource;
                audioSource = nullptr;
            }
        } else {
            qDebug() << "创建音频源失败";
        }
    } else {
        qDebug() << "找不到默认音频输入设备";
    }

    return false;
}

void MicrophoneManager::stopRecording()
{
    if (!recording) {
        return;
    }
    
    // 防止重入
    QMutexLocker locker(&audioMutex);
    
    if (!recording) {  // 双重检查
        return;
    }
    
    recording = false;  // 先设置标志，防止新的音频数据进入
    
    // 保存需要清理的指针
    QPointer<QAudioSource> sourceToStop = audioSource;
    QPointer<QIODevice> ioToClose = audioDevice;
    
    // 清空类成员指针
    audioSource = nullptr;
    audioDevice = nullptr;
    
    // 使用 QPointer 安全地访问对象
    if (!sourceToStop.isNull()) {
        // 先断开状态变化信号，避免触发 handleStateChanged
        disconnect(sourceToStop, &QAudioSource::stateChanged,
                  this, &MicrophoneManager::handleStateChanged);
        
        // 停止音频源并阻止其他信号
        sourceToStop->blockSignals(true);
        sourceToStop->stop();
        
        // 延迟删除音频源
        sourceToStop->deleteLater();
    }
    
    if (!ioToClose.isNull()) {
        // 断开信号并关闭设备
        disconnect(ioToClose, &QIODevice::readyRead,
                  this, &MicrophoneManager::handleReadyRead);
        
        ioToClose->blockSignals(true);
        if (ioToClose->isOpen()) {
            ioToClose->close();
        }
    }
    
    qDebug() << "录音已停止，资源已清理";
}

void MicrophoneManager::handleStateChanged(QAudio::State state)
{
    // 使用 QPointer 安全地访问 audioSource
    QPointer<QAudioSource> source = audioSource;
    if (source.isNull()) {
        return;
    }
    
    qDebug() << "音频设备状态变化:" << state;
    
    if (state == QAudio::StoppedState) {
        if (source->error() != QAudio::NoError) {
            qDebug() << "音频设备错误:" << source->error();
            QMetaObject::invokeMethod(this, "stopRecording", Qt::QueuedConnection);
        }
    } else if (state == QAudio::ActiveState) {
        qDebug() << "音频设备已进入活动状态";
    }
}

void MicrophoneManager::handleReadyRead()
{
    QMutexLocker locker(&audioMutex);
    
    // 先检查录音状态
    if (!recording) {
        return;
    }
    
    // 再检查设备状态
    if (!audioDevice || !audioDevice->isOpen() || !audioDevice->isReadable()) {
        return;
    }
    
    // 计算当前可用的数据大小
    qint64 bytesAvailable = audioDevice->bytesAvailable();
    int frameBytes = frameSize * format.bytesPerFrame();
    
    // 如果数据不足一帧，等待下次读取
    if (bytesAvailable < frameBytes) {
        return;
    }
    
    // 读取一帧数据
    QByteArray frameData;
    try {
        frameData = audioDevice->read(frameBytes);
    } catch (...) {
        qDebug() << "警告：读取音频数据时发生异常";
        return;
    }
    
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
        
        // 在互斥锁保护之外发送信号
        locker.unlock();
        // qDebug() << "MicrophoneManager: 准备发送PCM数据，大小:" << frameData.size() << "字节";
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