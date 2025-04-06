#include "vad_processor.h"
#include <QDebug>
#include <QEventLoop>

VadProcessor::VadProcessor(QObject *parent)
    : QObject(nullptr)
    , vad(new WebRTCVad(nullptr))
    , isRunning(false)
    , silenceFrameCount(0)
    , silenceFramesThreshold(50)  // 默认值
    , isProcessing(false)
    , frameBuffer()
{
    // 初始化 VAD
    vad->init(16000);  // 16kHz 采样率
    vad->setMode(2);   // 设置中等激进程度

    // 将vad对象也移动到工作线程
    vad->moveToThread(&workerThread);

    // 将对象移动到工作线程
    moveToThread(&workerThread);

    // 连接线程启动信号到处理槽
    connect(&workerThread, &QThread::started, this, &VadProcessor::run);
}

VadProcessor::~VadProcessor()
{
    stop();
    workerThread.wait();
    delete vad;
}

void VadProcessor::start()
{
    if (!isRunning) {
        isRunning = true;
        workerThread.start();
    }
}

void VadProcessor::stop()
{
    if (isRunning) {
        isRunning = false;
        workerThread.quit();
        workerThread.wait();
    }
}

void VadProcessor::reset()
{
    QMutexLocker locker(&mutex);
    silenceFrameCount = 0;
    dataQueue.clear();
    frameBuffer.clear();  // 清除帧缓冲
}

void VadProcessor::processAudioData(const QByteArray& pcmData)
{
    // qDebug() << "VAD处理器收到音频数据，大小:" << pcmData.size() << "字节，处理器状态:" << (isRunning ? "运行中" : "未运行");
    
    if (!isRunning) {
        qDebug() << "VAD处理器未运行，忽略数据";
        return;
    }

    QMutexLocker locker(&mutex);
    dataQueue.enqueue(pcmData);
    
    // 触发数据处理
    bool success = QMetaObject::invokeMethod(this, &VadProcessor::handleNewData, Qt::QueuedConnection);
    // qDebug() << "VAD处理器触发handleNewData:" << (success ? "成功" : "失败");
}

void VadProcessor::run()
{
    qDebug() << "VAD处理线程启动";
    
    // 创建事件循环
    QEventLoop eventLoop;
    
    while (isRunning) {
        // 处理所有待处理的事件
        eventLoop.processEvents();
        // QThread::msleep(1); // 短暂休眠，避免CPU占用过高
    }
    
    qDebug() << "VAD处理线程结束";
}

void VadProcessor::handleNewData()
{
    if (isProcessing) {
        qDebug() << "VAD处理器正忙，跳过本次处理";
        return;
    }

    isProcessing = true;
    QByteArray pcmData;
    
    {
        QMutexLocker locker(&mutex);
        if (!dataQueue.isEmpty()) {
            pcmData = dataQueue.dequeue();
        }
    }

    if (!pcmData.isEmpty()) {
        frameBuffer.append(pcmData);
        
        // 处理完整的帧
        while (frameBuffer.size() >= FRAME_SIZE * sizeof(int16_t)) {
            QByteArray frame = frameBuffer.left(FRAME_SIZE * sizeof(int16_t));
            frameBuffer.remove(0, FRAME_SIZE * sizeof(int16_t));
            
            const int16_t* samples = reinterpret_cast<const int16_t*>(frame.constData());
            bool hasVoice = vad->process(samples, FRAME_SIZE);
            
            if (hasVoice) {
                // qDebug() << "检测到语音帧，重置静音计数器";
                silenceFrameCount = 0;
                emit voiceDetected();
            } else {
                // qDebug() << "检测到静音帧，当前静音帧计数:" << silenceFrameCount;
                silenceFrameCount++;
                if (silenceFrameCount >= silenceFramesThreshold) {
                    qDebug() << "WebRTC VAD检测到持续静音";
                    emit silenceDetected();
                    reset();
                }
            }
        }
    }

    isProcessing = false;
    emit processingFinished();
} 