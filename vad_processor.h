#ifndef VAD_PROCESSOR_H
#define VAD_PROCESSOR_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QQueue>
#include <QByteArray>
#include "webrtcvad.h"

class VadProcessor : public QObject
{
    Q_OBJECT
public:
    explicit VadProcessor(QObject *parent = nullptr);
    ~VadProcessor();

    void start();
    void stop();
    void reset();
    void processAudioData(const QByteArray& pcmData);
    void setVadFrames(int frames) { silenceFramesThreshold = frames; }

signals:
    void silenceDetected();
    void voiceDetected();
    void processingFinished();

private slots:
    void run();
    void handleNewData();

private:
    static const int FRAME_SIZE = 160;  // 10ms at 16kHz
    QThread workerThread;
    WebRTCVad* vad;
    QMutex mutex;
    QQueue<QByteArray> dataQueue;
    QByteArray frameBuffer;  // 用于缓存未处理完的音频数据
    bool isRunning;
    int silenceFrameCount;
    int silenceFramesThreshold;
    bool isProcessing;
};

#endif // VAD_PROCESSOR_H 