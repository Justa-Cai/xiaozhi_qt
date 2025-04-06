#include <QCoreApplication>
#include <QFile>
#include <QDebug>
#include <QtMath>
#include <QThread>
#include <QEventLoop>
#include <QTimer>
#include <QDataStream>
#include "opus_encoder.h"
#include "opus_decoder.h"
#include "speaker_manager.h"

class OpusSineTest : public QObject {
    Q_OBJECT
public:
    explicit OpusSineTest(QObject *parent = nullptr) : QObject(parent) {
        // 初始化参数
        sampleRate = 16000;  // 16kHz
        channels = 1;        // 单声道
        frameDuration = 60;  // 60ms
        frequency = 1000;    // 1kHz正弦波
        
        encoder = new OpusEncoder(this);
        decoder = new OpusDecoder(this);
        speaker = new SpeakerManager(this);
        
        // 初始化编解码器
        encoder->initialize(sampleRate, channels, frameDuration);
        decoder->initialize(sampleRate, channels, frameDuration);
        
        // 初始化扬声器
        speaker->configureAudioParams(sampleRate, channels);
    }
    
    // 生成一帧正弦波
    QByteArray generateSineWaveFrame(qint64 startSample) {
        // 计算一帧的采样点数（60ms @ 16kHz = 960个采样点）
        int samplesPerFrame = (sampleRate * frameDuration) / 1000;
        QByteArray pcmData;
        
        // 生成一帧的音频数据
        double amplitude = 32767.0; // 16位PCM的最大振幅
        
        for (int i = 0; i < samplesPerFrame; i++) {
            // 生成正弦波，考虑起始采样点以保持相位连续
            double t = static_cast<double>(startSample + i) / sampleRate;
            double value = amplitude * qSin(2.0 * M_PI * frequency * t);
            qint16 sample = static_cast<qint16>(value);
            
            // 添加采样点到PCM数据（小端序）
            pcmData.append(reinterpret_cast<char*>(&sample), 2);
        }
        
        return pcmData;
    }
    
    void testEncodeDecode() {
        // 计算30秒音频需要的帧数
        int samplesPerFrame = (sampleRate * frameDuration) / 1000;
        int totalFrames = (30 * sampleRate) / samplesPerFrame;
        
        // 打开文件准备写入
        QFile opusFile("send.opus");
        if (!opusFile.open(QIODevice::WriteOnly)) {
            qDebug() << "无法创建文件 send.opus";
            return;
        }
        QDataStream opusStream(&opusFile);
        opusStream.setByteOrder(QDataStream::LittleEndian);
        
        QByteArray allDecodedPcm;
        int totalOpusSize = 0;
        
        qDebug() << "开始生成" << totalFrames << "帧音频数据...";
        
        // 逐帧处理
        for (int frame = 0; frame < totalFrames; frame++) {
            // 生成当前帧的PCM数据
            QByteArray pcmFrame = generateSineWaveFrame(frame * samplesPerFrame);
            
            // 编码当前帧
            QByteArray opusFrame = encoder->encode(pcmFrame);
            
            // 写入帧长度和数据
            opusStream << (quint16)opusFrame.size();
            opusStream.writeRawData(opusFrame.constData(), opusFrame.size());
            totalOpusSize += 2 + opusFrame.size();
            
            // 解码当前帧
            QByteArray decodedFrame = decoder->decode(opusFrame);
            allDecodedPcm.append(decodedFrame);
            
            if (frame % 50 == 0) {  // 每50帧显示一次进度
                qDebug() << "已处理" << frame << "帧，完成" 
                         << (frame * 100.0 / totalFrames) << "%";
            }
        }
        
        opusFile.close();
        
        qDebug() << "音频生成完成:";
        qDebug() << "总Opus数据大小:" << totalOpusSize << "字节";
        qDebug() << "总PCM数据大小:" << allDecodedPcm.size() << "字节";
        
        // 保存解码后的PCM数据
        QFile pcmFile("decoded.pcm");
        if (pcmFile.open(QIODevice::WriteOnly)) {
            pcmFile.write(allDecodedPcm);
            pcmFile.close();
            qDebug() << "解码后的PCM数据已保存到decoded.pcm";
        }
        
        // 播放解码后的音频
        if (!allDecodedPcm.isEmpty()) {
            qDebug() << "开始播放音频...";
            speaker->playPCM(allDecodedPcm);
            
            // 使用事件循环等待音频播放完成（30秒）
            QEventLoop loop;
            QTimer::singleShot(31000, &loop, &QEventLoop::quit);
            loop.exec();
            
            qDebug() << "音频播放完成";
        } else {
            qDebug() << "没有可播放的音频数据";
        }
    }
    
private:
    OpusEncoder* encoder;
    OpusDecoder* decoder;
    SpeakerManager* speaker;
    int sampleRate;
    int channels;
    int frameDuration;
    int frequency;
};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    OpusSineTest test;
    test.testEncodeDecode();
    
    return app.exec();
}

#include "test_sine_wave.moc" 