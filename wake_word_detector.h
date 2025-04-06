#ifndef WAKE_WORD_DETECTOR_H
#define WAKE_WORD_DETECTOR_H

#include <QObject>
#include <QString>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>

// 前向声明 VoskRecognizer 和 VoskModel
struct VoskRecognizer;
struct VoskModel;

class WakeWordDetector : public QObject
{
    Q_OBJECT

public:
    explicit WakeWordDetector(QObject *parent = nullptr);
    ~WakeWordDetector();

    // 初始化检测器
    bool initialize(const QString& modelPath);
    
    // 处理音频数据
    void processAudioData(const QByteArray& pcmData);
    
    // 启动和停止处理线程
    void start();
    void stop();

signals:
    // 当检测到唤醒词时发出信号
    void wakeWordDetected(const QString& text);
    
    // 当初始化完成时发出信号
    void initializationFinished(bool success);

private:
    void processingLoop();  // 处理循环函数
    void checkWakeWord(const QString& text); // 检查唤醒词的辅助函数

private:
    std::unique_ptr<VoskModel, void(*)(VoskModel*)> model;
    std::unique_ptr<VoskRecognizer, void(*)(VoskRecognizer*)> recognizer;
    std::atomic<bool> isInitialized;
    std::atomic<bool> isRunning;
    
    std::unique_ptr<std::thread> workerThread;
    QQueue<QByteArray> audioQueue;
    QMutex mutex;
    QWaitCondition condition;
    std::chrono::steady_clock::time_point lastResetTime;

    // 新增：用于累积识别结果
    QString accumulatedText;                                    // 累积的识别文本
    std::chrono::steady_clock::time_point lastRecognitionTime; // 上次识别时间
    static constexpr int ACCUMULATION_WINDOW_MS = 1500;        // 累积窗口时间（毫秒）
    static constexpr int MAX_ACCUMULATED_LENGTH = 50;          // 最大累积长度
};

#endif // WAKE_WORD_DETECTOR_H 