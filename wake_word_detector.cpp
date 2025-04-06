#include "wake_word_detector.h"
#include <vosk_api.h>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QWaitCondition>
#include <QEventLoop>
#include <QDir>
#include <QCoreApplication>
#include <QThread>
#include <sstream>
#include <chrono>

WakeWordDetector::WakeWordDetector(QObject *parent)
    : QObject(parent)
    , model(nullptr, [](VoskModel* m) { if(m) vosk_model_free(m); })
    , recognizer(nullptr, [](VoskRecognizer* r) { if(r) vosk_recognizer_free(r); })
    , isInitialized(false)
    , isRunning(false)
    , mutex()
    , condition()
    , workerThread(nullptr)
    , lastResetTime(std::chrono::steady_clock::now())
    , accumulatedText("")
    , lastRecognitionTime(std::chrono::steady_clock::now())
{
}

WakeWordDetector::~WakeWordDetector()
{
    stop();
}

bool WakeWordDetector::initialize(const QString& modelPath)
{
    if (isInitialized) {
        return true;
    }

    // 检查模型路径是否存在
    QDir modelDir(modelPath);
    if (!modelDir.exists()) {
        qDebug() << "模型目录不存在:" << modelPath;
        return false;
    }

    // 加载模型
    VoskModel* rawModel = vosk_model_new(modelPath.toUtf8().constData());
    if (!rawModel) {
        qDebug() << "无法加载Vosk模型:" << modelPath;
        return false;
    }
    model.reset(rawModel);

    // 创建识别器
    VoskRecognizer* rawRecognizer = vosk_recognizer_new(model.get(), 16000.0);
    if (!rawRecognizer) {
        qDebug() << "无法创建Vosk识别器";
        return false;
    }
    recognizer.reset(rawRecognizer);

    // 设置为单词模式，这样可以实时获取识别结果
    vosk_recognizer_set_words(recognizer.get(), 1);

    isInitialized = true;
    qDebug() << "Vosk唤醒词检测器初始化成功";
    emit initializationFinished(true);

    return true;
}

void WakeWordDetector::processAudioData(const QByteArray& pcmData)
{
    if (!isInitialized || !recognizer) {
        qDebug() << "processAudioData: 检测器未初始化或识别器为空";
        return;
    }

    QMutexLocker locker(&mutex);
    // qDebug() << "processAudioData: 收到音频数据，大小:" << pcmData.size() << "字节";
    audioQueue.enqueue(pcmData);
    // qDebug() << "processAudioData: 当前队列中数据包数量:" << audioQueue.size();
    condition.wakeOne();
}

void WakeWordDetector::start()
{
    if (!isInitialized || isRunning) {
        qDebug() << "start: 启动失败 - 初始化状态:" << isInitialized << "运行状态:" << isRunning;
        return;
    }
    
    qDebug() << "start: 准备启动工作线程";
    isRunning = true;
    
    // 使用 std::thread 创建工作线程
    workerThread = std::make_unique<std::thread>(&WakeWordDetector::processingLoop, this);
    
    qDebug() << "start: 工作线程已启动";
}

void WakeWordDetector::stop()
{
    if (!isRunning) {
        return;
    }
    
    {
        QMutexLocker locker(&mutex);
        isRunning = false;
        condition.wakeAll();
    }
    
    // 等待工作线程结束
    if (workerThread && workerThread->joinable()) {
        workerThread->join();
    }
    workerThread.reset();
}

void WakeWordDetector::checkWakeWord(const QString& text) {
    static const QStringList wakePatterns = {
        "你好小智", "你好 小智",
        "小智小智", "小智 小智",
        "小子小子", "小子 小子",
        "你好小子", "你好 小子"
    };

    QString cleanText = text;
    cleanText.remove(' ');
    
    bool isWakeWordFound = false;
    for (const QString& pattern : wakePatterns) {
        QString cleanPattern = pattern;
        cleanPattern.remove(' ');
        
        if (cleanText.contains(cleanPattern)) {
            isWakeWordFound = true;
            break;
        }
    }

    if (isWakeWordFound) {
        qDebug() << "processingLoop: 检测到唤醒词！完整文本:" << text;
        
        QMetaObject::invokeMethod(this, [this, text]() {
            emit wakeWordDetected(text);
        }, Qt::QueuedConnection);

        // 重置识别器状态
        vosk_recognizer_reset(recognizer.get());
        
        // 清空累积的文本和重置时间
        accumulatedText.clear();
        lastRecognitionTime = std::chrono::steady_clock::now();
        lastResetTime = std::chrono::steady_clock::now();
        
        // 获取一次部分识别结果以清空缓存
        const char* partial = vosk_recognizer_partial_result(recognizer.get());
        if (partial) {
            qDebug() << "processingLoop: 识别器状态已重置";
        }
    }
}

void WakeWordDetector::processingLoop()
{
    std::stringstream ss;
    ss << std::this_thread::get_id();
    qDebug() << "processingLoop: 处理循环开始运行，线程ID:" << QString::fromStdString(ss.str());
    
    while (isRunning) {
        QByteArray audioData;
        
        {
            QMutexLocker locker(&mutex);
            while (audioQueue.isEmpty() && isRunning) {
                condition.wait(&mutex);
            }
            
            if (!isRunning) {
                break;
            }
            
            if (!audioQueue.isEmpty()) {
                audioData = audioQueue.dequeue();
            }
        }
        
        if (!audioData.isEmpty()) {
            int result = vosk_recognizer_accept_waveform(recognizer.get(), 
                                                   audioData.constData(),
                                                   audioData.size());
            
            if (result < 0) {
                qDebug() << "processingLoop: 音频数据处理失败";
                continue;
            }

            const char* partial = vosk_recognizer_partial_result(recognizer.get());
            if (partial) {
                QJsonDocument doc = QJsonDocument::fromJson(partial);
                if (!doc.isNull()) {
                    QJsonObject obj = doc.object();
                    QString text = obj["partial"].toString().toLower();
                    
                    if (!text.isEmpty()) {
                        // qDebug() << "processingLoop: 识别到的文本:" << text;

                        auto currentTime = std::chrono::steady_clock::now();
                        auto timeSinceLastRecognition = std::chrono::duration_cast<std::chrono::milliseconds>(
                            currentTime - lastRecognitionTime).count();

                        // 如果超过时间窗口或累积文本过长，重置累积
                        if (timeSinceLastRecognition > ACCUMULATION_WINDOW_MS || 
                            accumulatedText.length() > MAX_ACCUMULATED_LENGTH) {
                            accumulatedText.clear();
                        }

                        // 更新累积文本和时间
                        if (!accumulatedText.isEmpty()) {
                            accumulatedText += " ";
                        }
                        accumulatedText += text;
                        lastRecognitionTime = currentTime;

                        // 检查累积的文本是否包含唤醒词
                        checkWakeWord(accumulatedText);
                    }
                }
            }
        }
    }
    
    std::stringstream ss_end;
    ss_end << std::this_thread::get_id();
    qDebug() << "processingLoop: 处理循环结束，线程ID:" << QString::fromStdString(ss_end.str());
} 