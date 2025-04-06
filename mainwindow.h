#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QLineEdit>
#include <QVBoxLayout>
#include "websocket_client.h"
#include "microphone_manager.h"
#include "speaker_manager.h"
#include "opus_encoder.h"
#include "opus_decoder.h"
#include <QNetworkAccessManager>
#include <QDir>
#include <QFile>
#include <QWebSocket>
#include <QTimer>
#include "ui_mainwindow.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private slots:
    void onConnectClicked();
    void onStartListenClicked();
    void onStopListenClicked();
    
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onJsonReceived(const QString& json);
    void onAudioReceived(const std::vector<uint8_t>& data);
    
    void onPCMDataReady(const QByteArray& pcmData);
    void startRecording();
    void stopRecording();

private:
    void setupAudioModules();
    void setupWebSocket();
    void updateConnectionStatus(bool connected);
    void appendLog(const QString& text);
    void checkFirmwareVersion();

    // 新增的WebSocket消息处理方法
    void sendHelloMessage();
    void sendListenState(const QString& state, const QString& mode = "manual");
    void sendAbortMessage(const QString& reason);
    void sendWakeWordDetected(const QString& text);
    void sendIoTState(const QJsonObject& states);
    void sendIoTDescriptors(const QJsonObject& descriptors);

private:
    Ui::MainWindow *ui;
    WebSocketClient *wsClient;
    QNetworkAccessManager *networkManager;
    
    MicrophoneManager *micManager;
    SpeakerManager *speakerManager;
    OpusEncoder *opusEncoder;
    OpusDecoder *opusDecoder;
    
    bool isListening;
    bool isRecording;
    QString sessionId;
    QString deviceMacAddress;
    QString getMacAddress();

    // UI组件
    QVBoxLayout *mainLayout;
    QPushButton *connectButton;
    QLabel *statusLabel;
    QPushButton *startListenButton;
    QPushButton *stopListenButton;
    QTextEdit *logTextEdit;
}; 