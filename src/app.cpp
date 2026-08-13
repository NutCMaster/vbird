#include <QApplication>
#include <QFileInfo>
#include <QMainWindow>

#include "ui/startup_dialog.h"

int main (int argc, char *argv[]) {
    QApplication app (argc, argv);

    StartupDialog startup;
    if (startup.exec() != QDialog::Accepted) {
        return 0; // user hit Quit or closed the dialog
    }

    QMainWindow window;
    const QString path = startup.selectedPath();
    window.setWindowTitle(path.isEmpty()
        ? QStringLiteral("vBird — New Project")
        : QStringLiteral("vBird — %1").arg(QFileInfo(path).fileName()));

    // TODO: hand (path, startup.selectedFormat()) off to the project loader
    // once one exists. For now the startup dialog just decides what the
    // window opens as; nothing is actually parsed yet.

    window.showMaximized();
    window.show();
    return app.exec();
}