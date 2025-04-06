#include "websocket_client.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

// 配置参数
#define CONFIG_WEBSOCKET_ACCESS_TOKEN "test-token"  // 替换为实际的访问令牌

WebSocketClient::WebSocketClient(QObject *parent)
    : QObject(parent)
{
    connect(&webSocket, &QWebSocket::connected, this, &WebSocketClient::onConnected);
    connect(&webSocket, &QWebSocket::disconnected, this, &WebSocketClient::onDisconnected);
    connect(&webSocket, &QWebSocket::binaryMessageReceived, this, &WebSocketClient::onBinaryMessageReceived);
    connect(&webSocket, &QWebSocket::textMessageReceived, this, &WebSocketClient::onTextMessageReceived);
    connect(&webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &WebSocketClient::onError);
            
    connect(&timeoutTimer, &QTimer::timeout, this, &WebSocketClient::checkTimeout);
}

WebSocketClient::~WebSocketClient()
{
    closeConnection();
}

bool WebSocketClient::connectToServer(const QString& url)
{
    if (webSocket.state() == QAbstractSocket::ConnectedState) {
        qDebug() << "WebSocket already connected";
        return true;
    }

    serverHelloReceived = false;
    
    // 设置请求头
    QNetworkRequest request(url);
    request.setRawHeader("Protocol-Version", "1");
    request.setRawHeader("Authorization", "Bearer " + QString(CONFIG_WEBSOCKET_ACCESS_TOKEN).toUtf8());
    
    // 获取设备信息作为ID
    QString deviceId = "60:a4:4c:59:44:61";  // TODO: 从实际设备获取 MAC 地址
    QString clientId = "23e62604-e78c-4efd-88df-6652c22b63e9";  // TODO: 生成唯一的客户端 ID
    request.setRawHeader("Device-Id", deviceId.toUtf8());
    request.setRawHeader("Client-Id", clientId.toUtf8());
    
    qDebug() << "Connecting to WebSocket server:" << url;
    webSocket.open(request);
    
    // 启动超时定时器
    timeoutTimer.start(TIMEOUT_MS);
    
    return true;
}

void WebSocketClient::sendAudio(const std::vector<uint8_t>& data)
{
    // qDebug() << "发送音频数据大小 B:" << data.size() << "字节";
    if (!isConnected()) {
        return;
    }
    // qDebug() << "发送音频数据大小 A:" << data.size() << "字节";
    
    QByteArray byteArray(reinterpret_cast<const char*>(data.data()), data.size());
    webSocket.sendBinaryMessage(byteArray);
}

void WebSocketClient::sendText(const QString& text)
{
    if (!isConnected()) {
        return;
    }
    qDebug() << "发送文本消息:" << text;
    webSocket.sendTextMessage(text);
}

void WebSocketClient::closeConnection()
{
    timeoutTimer.stop();
    webSocket.close();
}

bool WebSocketClient::isConnected() const
{
    return webSocket.state() == QAbstractSocket::ConnectedState && serverHelloReceived;
}

void WebSocketClient::onConnected()
{
    qDebug() << "WebSocket connected";
    sendHello();
}

void WebSocketClient::onDisconnected()
{
    qDebug() << "WebSocket disconnected";
    serverHelloReceived = false;
    if (onDisconnectedCallback) {
        onDisconnectedCallback();
    }
}

void WebSocketClient::onBinaryMessageReceived(const QByteArray &message)
{
    if (onAudioCallback) {
        std::vector<uint8_t> data(message.begin(), message.end());
        onAudioCallback(data);
    }
}

void WebSocketClient::onTextMessageReceived(const QString &message)
{
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isNull()) {
        qDebug() << "Invalid JSON received:" << message;
        return;
    }
    qDebug() << "Received message:" << message;

    QJsonObject root = doc.object();
    QString type = root["type"].toString();
    
    qDebug() << "Received message type:" << type;
    
    if (type == "hello") {
        if (!serverHelloReceived && root["transport"].toString() == "websocket") {
            qDebug() << "Server hello received successfully";
            serverHelloReceived = true;
            timeoutTimer.stop();
            
            // 只在首次收到hello时调用回调
            if (onConnectedCallback) {
                onConnectedCallback();
            }
            if (onJsonCallback) {
                onJsonCallback(message);
            }
            
            // 如果服务器发送了音频参数，通知客户端
            if (root.contains("audio_params") && onAudioParamsCallback) {
                QJsonObject params = root["audio_params"].toObject();
                AudioParams audioParams;
                audioParams.format = params["format"].toString();
                audioParams.sampleRate = params["sample_rate"].toInt();
                audioParams.channels = params["channels"].toInt();
                audioParams.frameDuration = params["frame_duration"].toInt();
                onAudioParamsCallback(audioParams);
            }
        }
    } else if (type == "error") {
        qDebug() << "Received error from server:" << root["message"].toString();
        closeConnection();
    } else if (!type.isEmpty() && onJsonCallback) {
        onJsonCallback(message);
    }
}

void WebSocketClient::onError(QAbstractSocket::SocketError error)
{
    qDebug() << "WebSocket error:" << error << webSocket.errorString();
}

void WebSocketClient::sendHello()
{
    qDebug() << "Sending hello message to server";
    
    QJsonObject hello;
    hello["type"] = "hello";
    hello["version"] = 1;
    hello["transport"] = "websocket";
    
    QJsonObject audioParams;
    audioParams["format"] = "opus";
    audioParams["sample_rate"] = 16000;
    audioParams["channels"] = 1;
    audioParams["frame_duration"] = OPUS_FRAME_DURATION_MS;
    
    hello["audio_params"] = audioParams;
    
    QJsonDocument doc(hello);
    QString message = doc.toJson(QJsonDocument::Compact);
    qDebug() << "Hello message:" << message;
    
    webSocket.sendTextMessage(message);
}

void WebSocketClient::checkTimeout()
{
    if (!serverHelloReceived) {
        qDebug() << "Server hello timeout";
        closeConnection();
    }
} 
