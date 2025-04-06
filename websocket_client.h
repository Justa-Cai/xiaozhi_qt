#pragma once

#include <QObject>
#include <QWebSocket>
#include <QTimer>
#include <functional>
#include <vector>

struct AudioParams {
    QString format;
    int sampleRate;
    int channels;
    int frameDuration;
};

class WebSocketClient : public QObject {
    Q_OBJECT
public:
    explicit WebSocketClient(QObject *parent = nullptr);
    ~WebSocketClient();

    // 连接到服务器
    bool connectToServer(const QString& url);
    
    // 发送音频数据
    void sendAudio(const std::vector<uint8_t>& data);
    
    // 发送文本消息
    void sendText(const QString& text);
    
    // 关闭连接
    void closeConnection();
    
    // 是否已连接
    bool isConnected() const;

    // 设置回调函数
    void setOnAudioCallback(std::function<void(const std::vector<uint8_t>&)> callback) {
        onAudioCallback = callback;
    }
    
    void setOnJsonCallback(std::function<void(const QString&)> callback) {
        onJsonCallback = callback;
    }
    
    void setOnConnectedCallback(std::function<void()> callback) {
        onConnectedCallback = callback;
    }
    
    void setOnDisconnectedCallback(std::function<void()> callback) {
        onDisconnectedCallback = callback;
    }
    
    void setOnAudioParamsCallback(std::function<void(const AudioParams&)> callback) {
        onAudioParamsCallback = callback;
    }

private slots:
    void onConnected();
    void onDisconnected();
    void onBinaryMessageReceived(const QByteArray &message);
    void onTextMessageReceived(const QString &message);
    void onError(QAbstractSocket::SocketError error);

private:
    void sendHello();
    void checkTimeout();

private:
    QWebSocket webSocket;
    QTimer timeoutTimer;
    bool serverHelloReceived = false;
    
    std::function<void(const std::vector<uint8_t>&)> onAudioCallback;
    std::function<void(const QString&)> onJsonCallback;
    std::function<void()> onConnectedCallback;
    std::function<void()> onDisconnectedCallback;
    std::function<void(const AudioParams&)> onAudioParamsCallback;
    
    const int TIMEOUT_MS = 10000;  // 10秒超时
    const int OPUS_FRAME_DURATION_MS = 60;
}; 