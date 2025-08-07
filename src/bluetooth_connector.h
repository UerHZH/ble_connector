#pragma once

#include <QMainWindow>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QLowEnergyController>
#include <QListWidget>
#include <QSlider>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTimer>
#include <QMap>

class BluetoothConnector : public QMainWindow {
    Q_OBJECT

public:
    BluetoothConnector(QWidget *parent = nullptr);
    ~BluetoothConnector();

private slots:
    void deviceDiscovered(const QBluetoothDeviceInfo &device);
    void connectToDevice(QListWidgetItem *item);
    void disconnectFromDevice();
    void serviceDiscovered(const QBluetoothUuid &uuid);
    void serviceScanDone();
    void serviceDetailsDiscovered(QLowEnergyService::ServiceState newState);
    void onServiceSelected(QListWidgetItem *item);
    void updateAndSendValue();
    void handleControllerError(QLowEnergyController::Error error);
    void handleConnectionStateChanged(QLowEnergyController::ControllerState state);
    void onCharacteristicSelected(QListWidgetItem *item);
    void sendMessage();

private:
    void setupUI();
    void startScanning();
    void updateConnectionStatus(const QString &status);
    void updateIdFromSlider(int value);
    void updateValueFromSlider(int value);
    void updateValueFromLineEdit();
    void updateIdFromLineEdit();
    void centerId();
    void centerValue();


    QBluetoothDeviceDiscoveryAgent *discoveryAgent;
    QLowEnergyController *controller;
    QLowEnergyService *currentService;
    
    QListWidget *deviceList;
    QListWidget *serviceList;
    QListWidget *characteristicList;
    QPushButton *scanButton;
    QPushButton *connectButton;
    QPushButton *disconnectButton;
    QSlider *idSlider;
    QSlider *valueSlider;
    QLineEdit *idLineEdit;
    QLineEdit *valueLineEdit;
    QLabel *idLabel;
    QLabel *valueLabel;
    QLabel *statusLabel;
    
    int lastId;
    int lastValue;
    QTimer *sendTimer;
    QMap<QBluetoothUuid, QList<QLowEnergyCharacteristic>> serviceCharacteristics;
    QLowEnergyCharacteristic currentWriteCharacteristic;
    QMap<QBluetoothUuid, QLowEnergyService*> services;
}; 