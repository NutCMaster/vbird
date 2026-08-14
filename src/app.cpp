#include <QApplication>
#include <QMessageBox>
#include <QVulkanInstance>

#include "ui/main_window.h"
#include "ui/startup_dialog.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    StartupDialog startup;
    if (startup.exec() != QDialog::Accepted)
        return 0;

    // Every VulkanViewport's surface/device stays valid only as long as this
    // instance does, so it must outlive the MainWindow below -- locals are
    // destroyed in reverse declaration order.
    QVulkanInstance vulkanInstance;
#ifdef VBIRD_VULKAN_VALIDATION
    vulkanInstance.setLayers({ QByteArrayLiteral("VK_LAYER_KHRONOS_validation") });
#endif
    if (!vulkanInstance.create()) {
        QMessageBox::critical(nullptr, QStringLiteral("vBird"),
            QStringLiteral("Failed to create a Vulkan instance (error %1). "
                            "A Vulkan-capable graphics driver is required.")
                .arg(vulkanInstance.errorCode()));
        return 1;
    }

    MainWindow window(startup.selectedPath(), &vulkanInstance);
    window.showMaximized();
    window.show();

    return app.exec();
}
