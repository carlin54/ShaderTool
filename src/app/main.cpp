#include <QApplication>
#include <QCoreApplication>
#include <QVersionNumber>
#include <QVulkanInstance>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ShaderTool"));
    QCoreApplication::setOrganizationName(QStringLiteral("ShaderTool"));

    QVulkanInstance vulkan;
    vulkan.setApiVersion(QVersionNumber(1, 2));
    if (qEnvironmentVariableIsSet("QT_VULKAN_DEBUG"))
        vulkan.setLayers(QByteArrayList() << QByteArrayLiteral("VK_LAYER_KHRONOS_validation"));
    if (!vulkan.create()) {
        qFatal("Failed to create Vulkan instance: %d", vulkan.errorCode());
        return 1;
    }

    MainWindow w(&vulkan);
    w.show();
    return app.exec();
}
