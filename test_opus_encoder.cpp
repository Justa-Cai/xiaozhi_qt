#include <QCoreApplication>
#include <QFile>
#include <QDebug>
#include <QDataStream>
#include "opus_decoder.h"
#include "speaker_manager.h"

class OpusTest : public QObject {
    Q_OBJECT
public:
    explicit OpusTest(QObject *parent = nullptr) : QObject(parent) {
        decoder = new OpusDecoder(this);
        speaker = new SpeakerManager(this);
        
        // 配置解码器和扬声器参数
        // 使用音频参数：16kHz采样率，单声道，60ms帧长
        sampleRate = 24000;
        channels = 1;
        frameDuration = 60; // 60ms
        
        decoder->initialize(sampleRate, channels, frameDuration);
        speaker->configureAudioParams(sampleRate, channels);
        
        // 计算缓冲区大小
        calculateBufferSize();
    }
    
    void playOpusFile(const QString& filename) {
        QFile file(filename);
        if (!file.open(QIODevice::ReadOnly)) {
            qDebug() << "无法打开文件:" << filename;
            return;
        }
        
        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::LittleEndian);
        
        qDebug() << "Opus文件大小:" << file.size() << "字节";
        
        // 逐帧解码并播放
        QByteArray pcmBuffer;
        int frameCount = 0;
        
        while (!stream.atEnd()) {
            // 读取帧长度
            quint16 frameLength;
            stream >> frameLength;
            
            if (stream.status() != QDataStream::Ok) {
                qDebug() << "读取帧长度失败";
                break;
            }
            
            // 检查帧长度是否有效
            if (frameLength == 0 || frameLength > 2000) {  // Opus帧通常不会超过2000字节
                qDebug() << "无效的帧长度:" << frameLength;
                break;
            }
            
            // 读取Opus帧数据
            QByteArray opusFrame(frameLength, 0);
            if (stream.readRawData(opusFrame.data(), frameLength) != frameLength) {
                qDebug() << "读取帧数据失败";
                break;
            }
            
            // 解码当前帧
            QByteArray pcmFrame = decoder->decode(opusFrame);
            
            if (!pcmFrame.isEmpty()) {
                pcmBuffer.append(pcmFrame);
                frameCount++;
                
                // 当缓冲区达到一定大小时进行播放
                if (pcmBuffer.size() >= bufferSize) {
                    speaker->playPCM(pcmBuffer);
                    // qDebug() << "播放音频块，大小:" << pcmBuffer.size() << "字节";
                    pcmBuffer.clear();
                }
            } else {
                qDebug() << "解码失败，跳过当前帧";
            }
        }
        
        file.close();
        
        // 播放剩余的数据
        if (!pcmBuffer.isEmpty()) {
            speaker->playPCM(pcmBuffer);
            qDebug() << "播放最后的音频块，大小:" << pcmBuffer.size() << "字节";
        }
        
        qDebug() << "音频播放完成，总帧数:" << frameCount;
    }
    
private:
    void calculateBufferSize() {
        // 计算每个采样的字节数（16位采样 = 2字节）
        const int BYTES_PER_SAMPLE = 2;
        // 计算缓冲区时长（建议使用帧长的倍数，这里使用2倍帧长）
        const int BUFFER_DURATION_MS = frameDuration * 2;
        // 计算缓冲区大小：采样率 * 通道数 * 每个采样字节数 * 缓冲区时长（秒）
        bufferSize = sampleRate * channels * BYTES_PER_SAMPLE * (BUFFER_DURATION_MS / 1000.0);
    }

    OpusDecoder* decoder;
    SpeakerManager* speaker;
    int sampleRate;
    int channels;
    int frameDuration;
    int bufferSize;
};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    OpusTest test;
    test.playOpusFile("recv.opus");
    // test.playOpusFile("send.opus");
    
    return app.exec();
}

#include "test_opus_encoder.moc" 