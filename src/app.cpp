#include <QApplication>
#include <QFileInfo>
#include <QMainWindow>

#include "ui/startup_dialog.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    StartupDialog startup;
    if (startup.exec() != QDialog::Accepted)
        return 0;

    QMainWindow window;

    const QString path = startup.selectedPath();

    window.setWindowTitle(
        path.isEmpty()
            ? QStringLiteral("vBird — New Project")
            : QStringLiteral("vBird — %1").arg(QFileInfo(path).fileName())
    );

    window.showMaximized();
    window.show();

    return app.exec();
}
