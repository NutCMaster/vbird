#include <QApplication>
#include <QMainWindow>
#include <iostream>

int main (int argc, char *argv[]) {
    QApplication app (argc, argv);
    QMainWindow window;
    window.setWindowTitle("vBird");
    window.showMaximized();
    window.show();
    return app.exec();
}