#include <QApplication>
#include <QTimer>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindow window;
    window.show();

    if (app.arguments().contains("--smoke-test")) {
        QTimer::singleShot(150, &app, &QCoreApplication::quit);
    }

    return app.exec();
}
