#include <QApplication>
#include <QLabel>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QLabel label("Qt toolchain probe");
    label.resize(240, 80);
    label.show();
    return 0;
}
