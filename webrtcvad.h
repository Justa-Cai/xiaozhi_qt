#ifndef WEBRTCVAD_H
#define WEBRTCVAD_H

#include <QObject>
#include <fvad.h>

// 前向声明
struct VadInst;

class WebRTCVad : public QObject
{
    Q_OBJECT
public:
    explicit WebRTCVad(QObject *parent = nullptr);
    ~WebRTCVad();
    
    bool init(int sampleRate = 16000);
    bool setMode(int mode); // 0-3, 0最不激进，3最激进
    bool process(const int16_t* audio_frame, size_t frame_length);

private:
    Fvad* handle;
    int sample_rate;
    bool initialized;
};

#endif // WEBRTCVAD_H 