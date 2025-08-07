#include <QApplication>
#include "bluetooth_connector.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    BluetoothConnector window;
    window.setWindowTitle("蓝牙控制器");
    window.resize(600, 700);
    window.show();
    return app.exec();
} 