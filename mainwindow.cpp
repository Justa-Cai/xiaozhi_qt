#include "mainwindow.h"
#include <QLineEdit>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QKeyEvent>
#include <QApplication>
#include <QScreen>
#include <QRect>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFile>
#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , wsClient(nullptr)
    , networkManager(new QNetworkAccessManager(this))
    , micManager(nullptr)
    , speakerManager(nullptr)
    , opusEncoder(nullptr)
    , opusDecoder(nullptr)
    , isListening(false)
    , isRecording(false)
    , sessionId("")  // 显式初始化为空
{
    ui->setupUi(this);
    
    // 将窗口移动到屏幕中央
    QRect screenGeometry = QApplication::primaryScreen()->geometry();
    int x = (screenGeometry.width() - width()) / 2;
    int y = (screenGeometry.height() - height()) / 2;
    move(x, y);

    setupAudioModules();
    setupWebSocket();
    
    // 连接录音按钮信号
    connect(ui->recordButton, &QPushButton::clicked, this, [this]() {
        if (!isRecording) {
            startRecording();
        } else {
            stopRecording();
        }
    });
    
    // 自动连接服务器
    QTimer::singleShot(500, this, [this]() {
        onConnectClicked();
    });
}

MainWindow::~MainWindow()
{
    delete ui;
    delete wsClient;
    delete networkManager;
    delete micManager;
    delete speakerManager;
    delete opusEncoder;
    delete opusDecoder;
}

void MainWindow::setupAudioModules()
{
    // 创建音频模块实例
    micManager = new MicrophoneManager(this);
    speakerManager = new SpeakerManager(this);
    opusEncoder = new OpusEncoder(this);
    opusDecoder = new OpusDecoder(this);
    
    // 配置音频参数
    int sampleRate = 16000;  // 使用服务器要求的采样率
    int channels = 1;
    int frameDuration = 60;  // ms
    
    micManager->configureAudioParams(sampleRate, channels);
    speakerManager->configureAudioParams(sampleRate, channels);
    opusEncoder->initialize(sampleRate, channels, frameDuration);
    opusDecoder->initialize(sampleRate, channels, frameDuration);
    
    // 连接麦克风PCM数据信号到编码器
    connect(micManager, &MicrophoneManager::pcmDataReady,
            this, &MainWindow::onPCMDataReady);
}

void MainWindow::onPCMDataReady(const QByteArray& pcmData)
{
    if (!isRecording) {
        qDebug() << "当前未在录音状态，忽略PCM数据";
        return;
    }
    
    if (!wsClient || !wsClient->isConnected()) {
        qDebug() << "WebSocket未连接，无法发送音频数据";
        return;
    }
    
    if (!opusEncoder) {
        qDebug() << "Opus编码器未初始化";
        return;
    }
 
    QByteArray opusData = opusEncoder->encode(pcmData);
    if (opusData.isEmpty()) {
        qDebug() << "Opus编码失败";
        return;
    }
    
    if (wsClient && wsClient->isConnected()) {
        // 保存opus数据到文件
        QFile file("send.opus");
        if (file.open(QIODevice::Append)) {
            file.write(opusData);
            file.close();
        }

        std::vector<uint8_t> data(opusData.begin(), opusData.end());
        wsClient->sendAudio(data);
    }
}

void MainWindow::onAudioReceived(const std::vector<uint8_t>& data)
{
    QByteArray opusData(reinterpret_cast<const char*>(data.data()), data.size());
    
    // 保存opus数据到文件，包含帧长度信息
    QFile file("recv.opus");
    if (file.open(QIODevice::Append)) {
        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::LittleEndian);
        // 写入帧长度（2字节）和数据
        stream << (quint16)opusData.size();
        stream.writeRawData(opusData.constData(), opusData.size());
        file.close();
    }
    
    QByteArray pcmData = opusDecoder->decode(opusData);
    if (!pcmData.isEmpty()) {
        speakerManager->playPCM(pcmData);
    }
    // appendLog(QString("解码后的PCM数据帧长度: %1 字节").arg(pcmData.size()));
    // appendLog(QString("收到音频数据: %1 字节").arg(data.size()));
}

void MainWindow::startRecording()
{
    if (isRecording) {
        return;
    }
    
    // 开始录音
    if (micManager->startRecording()) {
        isRecording = true;
        ui->recordButton->setText("停止录音");
        appendLog("开始录音");
    } else {
        appendLog("启动录音失败");
    }
}

void MainWindow::stopRecording()
{
    if (!isRecording) {
        return;
    }
    
    micManager->stopRecording();
    isRecording = false;
    ui->recordButton->setText("开始录音");
    appendLog("停止录音");
}

void MainWindow::setupWebSocket()
{
    wsClient = new WebSocketClient(this);
    
    // 设置WebSocket回调
    wsClient->setOnConnectedCallback([this]() {
        onWebSocketConnected();
    });
    
    wsClient->setOnDisconnectedCallback([this]() {
        onWebSocketDisconnected();
    });
    
    wsClient->setOnJsonCallback([this](const QString& json) {
        onJsonReceived(json);
    });
    
    wsClient->setOnAudioCallback([this](const std::vector<uint8_t>& data) {
        onAudioReceived(data);
    });
    
    wsClient->setOnAudioParamsCallback([this](const AudioParams& params) {
        // 只更新解码器和扬声器的参数，保持麦克风配置不变
        speakerManager->configureAudioParams(params.sampleRate, params.channels);
        opusDecoder->initialize(params.sampleRate, params.channels, params.frameDuration);
        
        qDebug() << "音频解码和播放参数已更新:"
                 << "\n  采样率:" << params.sampleRate
                 << "\n  通道数:" << params.channels
                 << "\n  帧时长:" << params.frameDuration;
    });
    
    // 创建中心部件
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    // 创建主布局
    mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // 连接控制
    connectButton = new QPushButton("连接服务器", this);
    connect(connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    mainLayout->addWidget(connectButton);
    
    // 状态显示
    statusLabel = new QLabel("未连接", this);
    mainLayout->addWidget(statusLabel);
    
    // 录音控制
    startListenButton = new QPushButton("开始录音", this);
    stopListenButton = new QPushButton("停止录音", this);
    connect(startListenButton, &QPushButton::clicked, this, &MainWindow::onStartListenClicked);
    connect(stopListenButton, &QPushButton::clicked, this, &MainWindow::onStopListenClicked);
    mainLayout->addWidget(startListenButton);
    mainLayout->addWidget(stopListenButton);
    
    // 日志显示
    logTextEdit = new QTextEdit(this);
    logTextEdit->setReadOnly(true);
    mainLayout->addWidget(logTextEdit);
    
    // 初始状态
    updateConnectionStatus(false);
}

void MainWindow::onConnectClicked()
{
    if (!wsClient->isConnected()) {
        QString url = "wss://api.tenclass.net/xiaozhi/v1/";  // 替换为实际的服务器地址
        wsClient->connectToServer(url);
        connectButton->setEnabled(false);
    } else {
        wsClient->closeConnection();
    }
}

void MainWindow::onStartListenClicked()
{
    if (!wsClient->isConnected()) {
        appendLog("WebSocket未连接");
        return;
    }
    
    isListening = true;
    isRecording = true;  // 设置录音状态
    updateConnectionStatus(true);
    
    // 发送开始监听状态
    sendListenState("start", "manual");
    appendLog("开始监听");
    
    // 开始录音
    if (micManager->startRecording()) {
        appendLog("开始录音");
    } else {
        appendLog("启动录音失败");
        isRecording = false;  // 如果启动失败，重置状态
        sendListenState("stop", "manual"); // 通知服务器停止监听
    }
}

void MainWindow::onStopListenClicked()
{
    if (isListening) {
        // 发送停止监听状态
        sendListenState("stop", "manual");
    }
    
    isListening = false;
    isRecording = false;  // 重置录音状态
    updateConnectionStatus(true);
    appendLog("停止监听");
    
    micManager->stopRecording();
}

void MainWindow::onWebSocketConnected()
{
    updateConnectionStatus(true);
    appendLog("已连接到服务器");
    
    // 连接成功后发送 hello 消息
    sendHelloMessage();
}

void MainWindow::onWebSocketDisconnected()
{
    updateConnectionStatus(false);
    appendLog("已断开连接");
    
    // 确保停止录音
    if (isListening) {
        micManager->stopRecording();
        isListening = false;
    }
}

void MainWindow::onJsonReceived(const QString& json)
{
    appendLog("收到消息: " + json);
    
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isNull()) {
        QJsonObject root = doc.object();
        QString type = root["type"].toString();
        
        if (type == "hello") {
            // 只在首次收到hello消息时处理
            if (sessionId.isEmpty()) {
                sessionId = root["session_id"].toString();
                appendLog("获取到session_id: " + sessionId);
                
                // 检查并更新音频参数
                if (root.contains("audio_params")) {
                    QJsonObject params = root["audio_params"].toObject();
                    int sampleRate = params["sample_rate"].toInt();
                    int channels = params["channels"].toInt();
                    int frameDuration = params["frame_duration"].toInt();
                    
                    // 更新音频参数
                    speakerManager->configureAudioParams(sampleRate, channels);
                    opusDecoder->initialize(sampleRate, channels, frameDuration);
                    
                    appendLog(QString("更新音频参数: 采样率=%1, 通道数=%2, 帧长=%3ms")
                            .arg(sampleRate)
                            .arg(channels)
                            .arg(frameDuration));
                }
                
                // 在成功连接并收到hello消息后检查固件版本
                checkFirmwareVersion();
            }
        }
        else if (type == "stt") {
            QString text = root["text"].toString();
            appendLog("语音识别结果: " + text);
            
            // 检查是否是唤醒词
            if (text.startsWith("你好小智") || text.startsWith("小智小智")) {
                sendWakeWordDetected(text);
            }
        }
        else if (type == "tts") {
            QString state = root["state"].toString();
            if (state == "start") {
                appendLog("开始播放TTS");
                // 如果正在录音，先停止录音并发送中断消息
                if (isListening) {
                    sendAbortMessage("tts_playback");
                    micManager->stopRecording();
                    isListening = false;
                    isRecording = false;
                    updateConnectionStatus(true);
                }
            }
            else if (state == "stop") {
                appendLog("TTS播放结束");
                speakerManager->stopPlaying();
                
                // TTS播放结束后，可以根据需要自动恢复录音
                if (!isListening) {
                    onStartListenClicked();
                }
            }
        }
    }
}

void MainWindow::updateConnectionStatus(bool connected)
{
    connectButton->setText(connected ? "断开连接" : "连接服务器");
    connectButton->setEnabled(true);
    
    statusLabel->setText(connected ? "已连接" : "未连接");
    
    startListenButton->setEnabled(connected);
    stopListenButton->setEnabled(connected && isListening);
}

void MainWindow::appendLog(const QString& text)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    logTextEdit->append(QString("[%1] %2").arg(timestamp).arg(text));
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_F2 && !event->isAutoRepeat()) {
        if (wsClient->isConnected() && !isListening) {
            onStartListenClicked();
        }
    }
    else if (event->key() == Qt::Key_Escape) {
        close();  // 按ESC键关闭窗口
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_F2 && !event->isAutoRepeat()) {
        if (wsClient->isConnected() && isListening) {
            onStopListenClicked();
        }
    }
    QMainWindow::keyReleaseEvent(event);
}

void MainWindow::checkFirmwareVersion()
{
    QString url = "https://api.tenclass.net/xiaozhi/ota/";
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Device-Id", deviceMacAddress.toUtf8());
    
    // 构建请求数据
    QJsonObject payload;
    payload["flash_size"] = 16777216;  // 16MB
    payload["minimum_free_heap_size"] = 8318916;
    payload["mac_address"] = deviceMacAddress;
    payload["chip_model_name"] = "esp32s3";
    
    QJsonObject chipInfo;
    chipInfo["model"] = 9;
    chipInfo["cores"] = 2;
    chipInfo["revision"] = 2;
    chipInfo["features"] = 18;
    payload["chip_info"] = chipInfo;
    
    QJsonObject application;
    application["name"] = "xiaozhi";
    application["version"] = "1.1.3";
    application["idf_version"] = "v5.3.2-dirty";
    payload["application"] = application;
    
    payload["partition_table"] = QJsonArray();
    
    QJsonObject ota;
    ota["label"] = "factory";
    payload["ota"] = ota;
    
    QJsonObject board;
    board["type"] = "bread-compact-wifi";
    board["mac"] = deviceMacAddress;
    payload["board"] = board;
    
    QJsonDocument doc(payload);
    QByteArray data = doc.toJson();
    
    // 发送POST请求
    QNetworkReply* reply = networkManager->post(request, data);
    
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(response);
            QJsonObject root = doc.object();
            
            // 显示详细的配置信息
            QString logMsg = "收到固件版本检查响应：";
            if (root.contains("mqtt")) {
                QJsonObject mqtt = root["mqtt"].toObject();
                logMsg += "\nMQTT配置:";
                if (mqtt.contains("endpoint")) {
                    logMsg += "\n- 服务器: " + mqtt["endpoint"].toString();
                }
                if (mqtt.contains("client_id")) {
                    logMsg += "\n- 客户端ID: " + mqtt["client_id"].toString();
                }
                if (mqtt.contains("publish_topic")) {
                    logMsg += "\n- 发布主题: " + mqtt["publish_topic"].toString();
                }
                if (mqtt.contains("subscribe_topic")) {
                    logMsg += "\n- 订阅主题: " + mqtt["subscribe_topic"].toString();
                }
            }
            if (root.contains("firmware")) {
                QJsonObject firmware = root["firmware"].toObject();
                if (firmware.contains("version")) {
                    logMsg += "\n固件版本: " + firmware["version"].toString();
                }
            }
            if (root.contains("activation")) {
                QJsonObject activation = root["activation"].toObject();
                if (activation.contains("code")) {
                    logMsg += "\n激活码: " + activation["code"].toString();
                }
            }
            appendLog(logMsg);
        } else {
            appendLog("固件版本检查失败: " + reply->errorString());
        }
        
        reply->deleteLater();
    });
}

void MainWindow::sendHelloMessage()
{
    if (!wsClient || !wsClient->isConnected()) {
        return;
    }
    
    QJsonObject hello;
    hello["type"] = "hello";
    hello["version"] = 1;
    hello["transport"] = "websocket";
    
    QJsonObject audioParams;
    audioParams["format"] = "opus";
    audioParams["sample_rate"] = 16000;
    audioParams["channels"] = 1;
    audioParams["frame_duration"] = 60;
    
    hello["audio_params"] = audioParams;
    
    QJsonDocument doc(hello);
    wsClient->sendText(doc.toJson());
    appendLog("发送Hello消息");
}

void MainWindow::sendListenState(const QString& state, const QString& mode)
{
    if (!wsClient || !wsClient->isConnected()) {
        return;
    }
    
    QJsonObject listen;
    listen["type"] = "listen";
    listen["session_id"] = sessionId;
    listen["state"] = state;
    listen["mode"] = mode;
    
    QJsonDocument doc(listen);
    wsClient->sendText(doc.toJson());
    appendLog(QString("发送Listen状态: %1, 模式: %2").arg(state).arg(mode));
}

void MainWindow::sendAbortMessage(const QString& reason)
{
    if (!wsClient || !wsClient->isConnected()) {
        return;
    }
    
    QJsonObject abort;
    abort["type"] = "abort";
    abort["session_id"] = sessionId;
    abort["reason"] = reason;
    
    QJsonDocument doc(abort);
    wsClient->sendText(doc.toJson());
    appendLog(QString("发送Abort消息，原因: %1").arg(reason));
}

void MainWindow::sendWakeWordDetected(const QString& text)
{
    if (!wsClient || !wsClient->isConnected()) {
        return;
    }
    
    QJsonObject detect;
    detect["type"] = "listen";
    detect["session_id"] = sessionId;
    detect["state"] = "detect";
    detect["text"] = text;
    
    QJsonDocument doc(detect);
    wsClient->sendText(doc.toJson());
    appendLog(QString("发送唤醒词检测消息: %1").arg(text));
}

void MainWindow::sendIoTState(const QJsonObject& states)
{
    if (!wsClient || !wsClient->isConnected()) {
        return;
    }
    
    QJsonObject iot;
    iot["type"] = "iot";
    iot["session_id"] = sessionId;
    iot["states"] = states;
    
    QJsonDocument doc(iot);
    wsClient->sendText(doc.toJson());
    appendLog("发送IoT状态更新");
}

void MainWindow::sendIoTDescriptors(const QJsonObject& descriptors)
{
    if (!wsClient || !wsClient->isConnected()) {
        return;
    }
    
    QJsonObject iot;
    iot["type"] = "iot";
    iot["session_id"] = sessionId;
    iot["descriptors"] = descriptors;
    
    QJsonDocument doc(iot);
    wsClient->sendText(doc.toJson());
    appendLog("发送IoT设备描述");
} 