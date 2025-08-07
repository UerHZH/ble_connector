#include "bluetooth_connector.h"
#include <QMessageBox>

// 构造函数：初始化 BluetoothConnector 类
BluetoothConnector::BluetoothConnector(QWidget *parent)
    : QMainWindow(parent), lastId(0), lastValue(0), currentWriteCharacteristic()
{
    setupUI();  // 设置用户界面
    
    // 初始化蓝牙设备发现代理
    discoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
    connect(discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &BluetoothConnector::deviceDiscovered);
            
    // 初始化定时器，用于定期发送数据
    sendTimer = new QTimer(this);
    sendTimer->setInterval(100);  // 100ms 防抖
    connect(sendTimer, &QTimer::timeout, this, &BluetoothConnector::updateAndSendValue);
    
    services.clear();  // 清空服务列表
}

// 设置用户界面
void BluetoothConnector::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    
    // 状态标签
    statusLabel = new QLabel("未连接", this);
    statusLabel->setAlignment(Qt::AlignCenter);
    
    // 按钮布局
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    scanButton = new QPushButton("扫描设备", this);
    connectButton = new QPushButton("连接", this);
    disconnectButton = new QPushButton("断开连接", this);
    
    buttonLayout->addWidget(scanButton);
    buttonLayout->addWidget(connectButton);
    buttonLayout->addWidget(disconnectButton);
    
    // 初始状态设置
    connectButton->setEnabled(false);
    disconnectButton->setEnabled(false);
    
    // 设备和服务列表
    deviceList = new QListWidget(this);
    serviceList = new QListWidget(this);
    characteristicList = new QListWidget(this);
    
    // 滑块和标签
    idSlider = new QSlider(Qt::Horizontal, this);
    valueSlider = new QSlider(Qt::Horizontal, this);
    idLabel = new QLabel("默认Forward: 0", this);
    valueLabel = new QLabel("默认Turn: 0", this);
    
    idSlider->setRange(-100, 100);
    idSlider->setValue(0);
    valueSlider->setRange(-90, 90);
    valueSlider->setValue(0);
    
    // 文本框
    idLineEdit = new QLineEdit(QString::number(idSlider->value()), this);
    valueLineEdit = new QLineEdit(QString::number(valueSlider->value()), this);
    
    // 置中按钮
    QPushButton *centerIdButton = new QPushButton("STOP", this);
    QPushButton *centerValueButton = new QPushButton("置中 TURN", this);
    
    // 添加所有控件到布局
    mainLayout->addWidget(statusLabel);
    mainLayout->addLayout(buttonLayout);
    mainLayout->addWidget(new QLabel("设备列表:"));
    mainLayout->addWidget(deviceList);
    mainLayout->addWidget(new QLabel("服务列表:"));
    mainLayout->addWidget(serviceList);
    mainLayout->addWidget(new QLabel("特征列表:"));
    mainLayout->addWidget(characteristicList);
    mainLayout->addWidget(idLabel);
    mainLayout->addWidget(idSlider);
    mainLayout->addWidget(idLineEdit);
    mainLayout->addWidget(centerIdButton);  // 添加置中 FORWARD 按钮
    mainLayout->addWidget(valueLabel);
    mainLayout->addWidget(valueSlider);
    mainLayout->addWidget(valueLineEdit);
    mainLayout->addWidget(centerValueButton);  // 添加置中 TURN 按钮
    
    // 设置比例
    mainLayout->setStretchFactor(deviceList, 5);
    mainLayout->setStretchFactor(serviceList, 2);
    mainLayout->setStretchFactor(characteristicList, 2);
    
    // 连接信号和槽
    connect(scanButton, &QPushButton::clicked, this, &BluetoothConnector::startScanning);
    connect(deviceList, &QListWidget::itemClicked, [this](QListWidgetItem *item) {
        connectButton->setEnabled(true);
    });
    connect(connectButton, &QPushButton::clicked, [this]() {
        QListWidgetItem *item = deviceList->currentItem();
        if (item) {
            connectToDevice(item);
        }
    });
    connect(disconnectButton, &QPushButton::clicked, this, &BluetoothConnector::disconnectFromDevice);
    connect(serviceList, &QListWidget::itemClicked, this, &BluetoothConnector::onServiceSelected);
    connect(characteristicList, &QListWidget::itemClicked, this, &BluetoothConnector::onCharacteristicSelected);
    
    connect(idSlider, &QSlider::valueChanged, this, &BluetoothConnector::updateIdFromSlider);
    connect(valueSlider, &QSlider::valueChanged, this, &BluetoothConnector::updateValueFromSlider);
    connect(idLineEdit, &QLineEdit::editingFinished, this, &BluetoothConnector::updateIdFromLineEdit);
    connect(valueLineEdit, &QLineEdit::editingFinished, this, &BluetoothConnector::updateValueFromLineEdit);

    // 连接置中按钮
    connect(centerIdButton, &QPushButton::clicked, this, &BluetoothConnector::centerId);
    connect(centerValueButton, &QPushButton::clicked, this, &BluetoothConnector::centerValue);
}

// 开始扫描蓝牙设备
void BluetoothConnector::startScanning()
{
    deviceList->clear();  // 清空设备列表
    serviceList->clear();  // 清空服务列表
    discoveryAgent->start();  // 开始设备扫描
    scanButton->setEnabled(false);  // 禁用扫描按钮
}

// 处理发现的设备
void BluetoothConnector::deviceDiscovered(const QBluetoothDeviceInfo &device)
{
    QString label = QString("%1 (%2)").arg(device.name()).arg(device.address().toString());
    QListWidgetItem *item = new QListWidgetItem(label);
    item->setData(Qt::UserRole, QVariant::fromValue(device));
    deviceList->addItem(item);  // 将设备添加到列表
}

// 连接到选定的设备
void BluetoothConnector::connectToDevice(QListWidgetItem *item)
{
    if (!item) return;
    
    QBluetoothDeviceInfo device = item->data(Qt::UserRole).value<QBluetoothDeviceInfo>();
    controller = QLowEnergyController::createCentral(device, this);
    
    // 连接信号和槽
    connect(controller, &QLowEnergyController::serviceDiscovered,
            this, &BluetoothConnector::serviceDiscovered);
    connect(controller, &QLowEnergyController::discoveryFinished,
            this, &BluetoothConnector::serviceScanDone);
    connect(controller, QOverload<QLowEnergyController::Error>::of(&QLowEnergyController::error),
            this, &BluetoothConnector::handleControllerError);
    connect(controller, &QLowEnergyController::stateChanged,
            this, &BluetoothConnector::handleConnectionStateChanged);
    
    qDebug() << "Connecting to device...";
    controller->connectToDevice();  // 开始连接设备
}

// 处理发现的服务
void BluetoothConnector::serviceDiscovered(const QBluetoothUuid &uuid)
{
    qDebug() << "发现服务 UUID:" << uuid.toString();
    QLowEnergyService *service = controller->createServiceObject(uuid, this);
    if (service) {
        services.insert(uuid, service);
        connect(service, &QLowEnergyService::stateChanged, this, &BluetoothConnector::serviceDetailsDiscovered);
        service->discoverDetails();  // 开始发现服务的详细信息
        
        // 添加服务到列表
        QListWidgetItem *item = new QListWidgetItem(uuid.toString());
        item->setData(Qt::UserRole, QVariant::fromValue(uuid));
        serviceList->addItem(item);
    } else {
        qDebug() << "无法创建服务对象";
    }
}

// 服务扫描完成
void BluetoothConnector::serviceScanDone()
{
    scanButton->setEnabled(true);  // 启用扫描按钮
    qDebug() << "服务扫描完成";
}

// 处理选定的服务
void BluetoothConnector::onServiceSelected(QListWidgetItem *item)
{
    if (!item || !controller) return;

    QBluetoothUuid serviceUuid = item->data(Qt::UserRole).value<QBluetoothUuid>();
    qDebug() << "Selected Service UUID:" << serviceUuid.toString();

    // 从已发现的服务中获取服务对象
    if (services.contains(serviceUuid)) {
        currentService = services.value(serviceUuid);
        if (currentService) {
            characteristicList->clear();  // 清空特征列表

            // 获取并显示特征
            const QList<QLowEnergyCharacteristic> &characteristics = currentService->characteristics();
            for (const QLowEnergyCharacteristic &characteristic : characteristics) {
                QString charText = QString("特征 UUID: %1").arg(characteristic.uuid().toString());
                QListWidgetItem *charItem = new QListWidgetItem(charText);
                charItem->setData(Qt::UserRole, QVariant::fromValue(characteristic.uuid()));
                characteristicList->addItem(charItem);
                qDebug() << "发现特征 UUID:" << characteristic.uuid().toString();
            }
        }
    } else {
        qDebug() << "服务未找到";
    }
}

// 处理服务详细信息的发现
void BluetoothConnector::serviceDetailsDiscovered(QLowEnergyService::ServiceState newState)
{
    if (newState == QLowEnergyService::ServiceDiscovered) {
        QLowEnergyService *service = qobject_cast<QLowEnergyService*>(sender());
        if (service) {
            qDebug() << "服务已完全发现，UUID:" << service->serviceUuid().toString();
            
            // 获取并显示特征
            const QList<QLowEnergyCharacteristic> &characteristics = service->characteristics();
            for (const QLowEnergyCharacteristic &characteristic : characteristics) {
                QString charText = QString("特征 UUID: %1").arg(characteristic.uuid().toString());
                QListWidgetItem *charItem = new QListWidgetItem(charText);
                charItem->setData(Qt::UserRole, QVariant::fromValue(characteristic.uuid()));
                // serviceList->addItem(charItem);
                qDebug() << "发现特征 UUID:" << characteristic.uuid().toString();

                // 检查特征是否可写，并将其赋值给 currentWriteCharacteristic
                if (characteristic.isValid() && (characteristic.properties() & QLowEnergyCharacteristic::Write)) {
                    currentWriteCharacteristic = characteristic;
                    qDebug() << "Selected writable Characteristic UUID:" << characteristic.uuid().toString();
                    // 如果只需要第一个可写特征，可以在这里 break;
                }
            }
        }
    } else {
        qDebug() << "服务未完全发现，状态:" << newState;
    }
}

// 更新并发送滑块的值
void BluetoothConnector::updateAndSendValue()
{
    sendTimer->stop();  // 停止定时器
    
    if (!currentService || !currentWriteCharacteristic.isValid()) return; // 检查特征有效性
    
    int currentId = idSlider->value();
    int currentValue = valueSlider->value();
    
    // 创建要发送的数据数组
    QByteArray data;
    data.append(static_cast<char>(currentId));
    data.append(static_cast<char>(currentValue));
    
    // 使用当前的可写特征发送数据
    currentService->writeCharacteristic(currentWriteCharacteristic, data);
    qDebug() << "发送数据: " << data.toHex();
    
    lastId = currentId;
    lastValue = currentValue;
}

// 断开设备连接
void BluetoothConnector::disconnectFromDevice()
{
    if (controller) {
        controller->disconnectFromDevice();
        statusLabel->setText("已断开连接");
        connectButton->setEnabled(true);
        disconnectButton->setEnabled(false);
        serviceList->clear();
    }
}

// 处理控制器错误
void BluetoothConnector::handleControllerError(QLowEnergyController::Error error)
{
    QString errorString;
    switch (error) {
        case QLowEnergyController::UnknownError:
            errorString = "未知错误";
            break;
        case QLowEnergyController::ConnectionError:
            errorString = "连接错误";
            break;
        case QLowEnergyController::InvalidBluetoothAdapterError:
            errorString = "无效的蓝牙适配器";
            break;
        default:
            errorString = "其他错误";
    }
    
    qDebug() << "Controller error:" << errorString;
    statusLabel->setText("错误: " + errorString);
    QMessageBox::critical(this, "错误", "蓝牙连接错误: " + errorString);
}

// 处理连接状态变化
void BluetoothConnector::handleConnectionStateChanged(QLowEnergyController::ControllerState state)
{
    switch (state) {
        case QLowEnergyController::UnconnectedState:
            qDebug() << "Unconnected";
            statusLabel->setText("未连接");
            connectButton->setEnabled(true);
            disconnectButton->setEnabled(false);
            break;
        case QLowEnergyController::ConnectingState:
            qDebug() << "Connecting...";
            statusLabel->setText("正在连接...");
            connectButton->setEnabled(false);
            break;
        case QLowEnergyController::ConnectedState:
            qDebug() << "Connected";
            statusLabel->setText("已连接");
            connectButton->setEnabled(false);
            disconnectButton->setEnabled(true);
            controller->discoverServices();  // 开始发现服务
            break;
        case QLowEnergyController::DiscoveringState:
            qDebug() << "Discovering services...";
            statusLabel->setText("正在发现服务...");
            break;
        default:
            qDebug() << "Unknown state";
            break;
    }
}

// 处理选定的特征
void BluetoothConnector::onCharacteristicSelected(QListWidgetItem *item)
{
    if (!item || !currentService) return;

    // 获取用户选择的特征 UUID
    QBluetoothUuid charUuid = item->data(Qt::UserRole).value<QBluetoothUuid>();
    const QList<QLowEnergyCharacteristic> &characteristics = currentService->characteristics();
    
    // 遍历当前服务的特征列表
    for (const QLowEnergyCharacteristic &characteristic : characteristics) {
        // 检查特征 UUID 是否匹配
        if (characteristic.uuid() == charUuid) {
            // 检查特征是否有效且可写
            if (characteristic.isValid() && (characteristic.properties() & QLowEnergyCharacteristic::Write)) {
                currentWriteCharacteristic = characteristic;
                qDebug() << "Selected writable Characteristic UUID:" << charUuid.toString();
            } else {
                qDebug() << "Selected characteristic is not valid or not writable.";
            }
            break;
        }
    }
}

// 发送消息
void BluetoothConnector::sendMessage()
{
    qDebug() <<"currentService:" << currentService;
    qDebug() <<"currentWriteCharacteristic:" << currentWriteCharacteristic.isValid();
    if (!currentService || !currentWriteCharacteristic.isValid()) {
        qDebug() << "无效的服务或特征，无法发送数据";
        return;
    }
    
    QByteArray data;
    data.append(static_cast<char>(idSlider->value()));
    data.append(static_cast<char>(valueSlider->value()));
    
    // 使用当前的可写特征发送数据
    currentService->writeCharacteristic(currentWriteCharacteristic, data);
    qDebug() << "发送数据: " << data.toHex();
}

// 析构函数：清理资源
BluetoothConnector::~BluetoothConnector()
{
    if (controller) {
        controller->disconnectFromDevice();
    }
}

void BluetoothConnector::updateIdFromSlider(int value)
{
    idLineEdit->setText(QString::number(value));
    sendMessage();  // 发送消息
}

void BluetoothConnector::updateValueFromSlider(int value)
{
    valueLineEdit->setText(QString::number(value));
    sendMessage();  // 发送消息
}

void BluetoothConnector::updateIdFromLineEdit()
{
    bool ok;
    int value = idLineEdit->text().toInt(&ok);
    if (ok && value >= -100 && value <= 100) {
        idSlider->setValue(value);
    }
}

void BluetoothConnector::updateValueFromLineEdit()
{
    bool ok;
    int value = valueLineEdit->text().toInt(&ok);
    if (ok && value >= -90 && value <= 90) {
        valueSlider->setValue(value);
    }
} 

void BluetoothConnector::centerId()
{
    idSlider->setValue(0);
    idLineEdit->setText(QString::number(0));
}

void BluetoothConnector::centerValue()
{
    valueSlider->setValue(0);
    valueLineEdit->setText(QString::number(0));
}