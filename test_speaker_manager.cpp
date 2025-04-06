#include "speaker_manager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QtMath>
#include <QTimer>

const int SAMPLE_RATE = 16000;  // 采样率改为16kHz
const int CHANNELS = 1;         // 单声道
const double FREQUENCY = 1000;  // 1kHz正弦波
const double AMPLITUDE = 32767; // 16位音频的最大振幅
const int DURATION_MS = 60;     // 生成60ms的音频数据

QByteArray generateSineWave() {
    int numSamples = (SAMPLE_RATE * DURATION_MS) / 1000;
    QByteArray sineWaveData;
    sineWaveData.resize(numSamples * 2); // 16位采样，每个采样2字节
    
    int16_t* samples = reinterpret_cast<int16_t*>(sineWaveData.data());
    for (int i = 0; i < numSamples; ++i) {
        double time = static_cast<double>(i) / SAMPLE_RATE;
        double angle = 2.0 * M_PI * FREQUENCY * time;
        samples[i] = static_cast<int16_t>(AMPLITUDE * qSin(angle));
    }
    
    return sineWaveData;
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    SpeakerManager speaker;
    speaker.configureAudioParams(SAMPLE_RATE, CHANNELS);
    
    QByteArray sineWave = generateSineWave();
    qDebug() << "开始持续播放1kHz正弦波...";
    qDebug() << "样本大小:" << sineWave.size() << "字节";
    qDebug() << "样本时长:" << DURATION_MS << "毫秒";
    
    // 创建定时器用于循环播放
    QTimer *playTimer = new QTimer(&app);
    QObject::connect(playTimer, &QTimer::timeout, [&]() {
        speaker.playPCM(sineWave);
    });
    
    // 每60ms触发一次
    playTimer->start(DURATION_MS);
    
    // 立即开始第一次播放
    speaker.playPCM(sineWave);
    
    return app.exec();
} 