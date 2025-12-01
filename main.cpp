#include <QApplication>

#include "trainer_window.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    TrainerWindow window;
    window.show();

    return QApplication::exec();
}
