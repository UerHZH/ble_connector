#include <QApplication>
#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QListWidget>
#include <QMessageBox>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QBluetoothLocalDevice>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QLineEdit>
#include <QLabel>
#include <QSlider>

class BluetoothScanner : public QWidget {
    Q_OBJECT

public:
    BluetoothScanner(QWidget *parent = nullptr);

private slots:
    void scanDevices();
    void deviceDiscovered(const QBluetoothDeviceInfo &device);
    void scanFinished();
    void connectToDevice();
    void deviceConnected();
    void deviceDisconnected();
    void serviceDiscovered(const QBluetoothUuid &uuid);
    void serviceDetailsDiscovered(QLowEnergyService::ServiceState state);
    void sendData();  // 新增自动发送数据功能

private:
    QListWidget *deviceList;
    QPushButton *scanButton;
    QPushButton *connectButton;
    QListWidget *serviceList;
    QLineEdit *idInput;
    QLineEdit *valueInput;
    QSlider *idSlider;
    QSlider *valueSlider;
    QBluetoothDeviceDiscoveryAgent *discoveryAgent;
    QLowEnergyController *controller;
    QList<QBluetoothDeviceInfo> discoveredDevices;
    QMap<QBluetoothUuid, QLowEnergyService*> services;
    QLowEnergyCharacteristic writeCharacteristic;
};

BluetoothScanner::BluetoothScanner(QWidget *parent) : QWidget(parent), controller(nullptr) {
    QVBoxLayout *layout = new QVBoxLayout(this);

    deviceList = new QListWidget(this);
    layout->addWidget(new QLabel("已发现的蓝牙设备："));
    layout->addWidget(deviceList);

    scanButton = new QPushButton("扫描设备", this);
    layout->addWidget(scanButton);

    connectButton = new QPushButton("连接设备", this);
    connectButton->setEnabled(false);
    layout->addWidget(connectButton);

    serviceList = new QListWidget(this);
    layout->addWidget(new QLabel("设备服务和特征："));
    layout->addWidget(serviceList);

    // 新增 ID 和 Value 输入框和滑块
    idInput = new QLineEdit(this);
    idInput->setPlaceholderText("ID (0-255)");
    idInput->setText("127");
    idInput->setEnabled(false);
    layout->addWidget(idInput);

    idSlider = new QSlider(Qt::Horizontal, this);
    idSlider->setRange(0, 255);
    idSlider->setValue(127);
    idSlider->setEnabled(false);
    layout->addWidget(idSlider);

    valueInput = new QLineEdit(this);
    valueInput->setPlaceholderText("值 (0-255)");
    valueInput->setText("127");
    valueInput->setEnabled(false);
    layout->addWidget(valueInput);

    valueSlider = new QSlider(Qt::Horizontal, this);
    valueSlider->setRange(0, 255);
    valueSlider->setValue(127);
    valueSlider->setEnabled(false);
    layout->addWidget(valueSlider);

    discoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
    discoveryAgent->setLowEnergyDiscoveryTimeout(10000);

    connect(scanButton, &QPushButton::clicked, this, &BluetoothScanner::scanDevices);
    connect(connectButton, &QPushButton::clicked, this, &BluetoothScanner::connectToDevice);

    // 更新 ID 和 Value 输入框与滑块的同步变化
    connect(idSlider, &QSlider::valueChanged, this, [this](int value) {
        idInput->setText(QString::number(value));
        sendData();  // 变化时发送数据
    });
    connect(idInput, &QLineEdit::textChanged, this, [this](const QString &text) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok && value >= 0 && value <= 255) {
            idSlider->setValue(value);
        }
    });

    connect(valueSlider, &QSlider::valueChanged, this, [this](int value) {
        valueInput->setText(QString::number(value));
        sendData();  // 变化时发送数据
    });
    connect(valueInput, &QLineEdit::textChanged, this, [this](const QString &text) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok && value >= 0 && value <= 255) {
            valueSlider->setValue(value);
        }
    });
}

void BluetoothScanner::scanDevices() {
    deviceList->clear();
    discoveredDevices.clear();
    connectButton->setEnabled(false);

    QBluetoothLocalDevice localDevice;
    if (localDevice.hostMode() == QBluetoothLocalDevice::HostPoweredOff) {
        QMessageBox::warning(this, "错误", "请打开蓝牙");
        return;
    }

    scanButton->setEnabled(false);
    scanButton->setText("正在扫描...");

    discoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
    connect(discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, this, &BluetoothScanner::deviceDiscovered);
    connect(discoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished, this, &BluetoothScanner::scanFinished);
}

void BluetoothScanner::deviceDiscovered(const QBluetoothDeviceInfo &device) {
    if (device.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration) {
        QString label = QString("%1 (%2)").arg(device.name(), device.address().toString());
        deviceList->addItem(label);
        discoveredDevices.append(device);
    }
}

void BluetoothScanner::scanFinished() {
    scanButton->setEnabled(true);
    scanButton->setText("扫描设备");
    connectButton->setEnabled(!discoveredDevices.isEmpty());
}

void BluetoothScanner::connectToDevice() {
    int currentRow = deviceList->currentRow();
    if (currentRow >= 0 && currentRow < discoveredDevices.size()) {
        const QBluetoothDeviceInfo &device = discoveredDevices.at(currentRow);

        if (controller) {
            controller->disconnectFromDevice();
            delete controller;
            controller = nullptr;
        }

        controller = QLowEnergyController::createCentral(device, this);

        connect(controller, &QLowEnergyController::connected, this, &BluetoothScanner::deviceConnected);
        connect(controller, &QLowEnergyController::disconnected, this, &BluetoothScanner::deviceDisconnected);
        connect(controller, &QLowEnergyController::serviceDiscovered, this, &BluetoothScanner::serviceDiscovered);

        connect(controller, QOverload<QLowEnergyController::Error>::of(&QLowEnergyController::error),
                this, [this](QLowEnergyController::Error error) {
            QMessageBox::critical(this, "错误", QString("连接错误: %1").arg(error));
        });

        connectButton->setEnabled(false);
        connectButton->setText("正在连接...");
        controller->connectToDevice();
    }
}

void BluetoothScanner::deviceConnected() {
    connectButton->setText("断开连接");
    connectButton->setEnabled(true);

    serviceList->clear();
    controller->discoverServices();

    QMessageBox::information(this, "连接状态", "设备连接成功！");
}

void BluetoothScanner::deviceDisconnected() {
    connectButton->setText("连接设备");
    connectButton->setEnabled(true);
    idInput->setEnabled(false);
    valueInput->setEnabled(false);
    idSlider->setEnabled(false);
    valueSlider->setEnabled(false);

    QMessageBox::information(this, "连接状态", "设备已断开连接");
}

void BluetoothScanner::serviceDiscovered(const QBluetoothUuid &uuid) {
    QLowEnergyService *service = controller->createServiceObject(uuid, this);
    if (service) {
        services.insert(uuid, service);
        connect(service, &QLowEnergyService::stateChanged, this, &BluetoothScanner::serviceDetailsDiscovered);
        service->discoverDetails();
    }
}

void BluetoothScanner::serviceDetailsDiscovered(QLowEnergyService::ServiceState state) {
    if (state == QLowEnergyService::ServiceDiscovered) {
        QLowEnergyService *service = qobject_cast<QLowEnergyService*>(sender());
        if (service) {
            QListWidgetItem *serviceItem = new QListWidgetItem(QString("服务 UUID: %1").arg(service->serviceUuid().toString()));
            serviceList->addItem(serviceItem);

            foreach (const QLowEnergyCharacteristic &characteristic, service->characteristics()) {
                QString charText = QString("  特征 UUID: %1").arg(characteristic.uuid().toString());
                if (characteristic.properties() & QLowEnergyCharacteristic::Write) {
                    charText += " (可写)";
                    writeCharacteristic = characteristic;  // 保存可写特征
                    idInput->setEnabled(true);
                    valueInput->setEnabled(true);
                    idSlider->setEnabled(true);
                    valueSlider->setEnabled(true);  // 激活滑块和输入框
                }
                if (characteristic.properties() & QLowEnergyCharacteristic::Read) charText += " (可读)";
                if (characteristic.properties() & QLowEnergyCharacteristic::Notify) charText += " (通知)";
                serviceList->addItem(new QListWidgetItem(charText));
            }
        }
    }
}

void BluetoothScanner::sendData() {
    if (!writeCharacteristic.isValid()) {
        QMessageBox::warning(this, "错误", "未找到可写特征");
        return;
    }

    bool idOk, valueOk;
    int id = idInput->text().toInt(&idOk);
    int value = valueInput->text().toInt(&valueOk);

    if (!idOk || !valueOk || id < 0 || id > 255 || value < 0 || value > 255) {
        QMessageBox::warning(this, "错误", "请输入有效的ID和值 (0-255)");
        return;
    }

    QByteArray data;
    data.append(static_cast<char>(id));
    data.append(static_cast<char>(value));

    services.begin().value()->writeCharacteristic(writeCharacteristic, data, QLowEnergyService::WriteWithoutResponse);
    // QMessageBox::information(this, "发送状态", QString("已发送数据: ID=%1, Value=%2").arg(id).arg(value));
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    BluetoothScanner window;
    window.setWindowTitle("ESP32 蓝牙扫描器");
    window.resize(800, 1200);
    window.show();
    return app.exec();
}

#include "ble_connect_search.moc"