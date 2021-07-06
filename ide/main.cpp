#include <QApplication>
#include <QPushButton>
#include <QtWidgets>

int main(int argc,char *argv[]){


    QApplication app(argc,argv);

    QWidget window;
    window.resize(500,500);
    window.show();
    window.setWindowTitle(QApplication::translate("toplevel","Starbytes IDE"));

    QPushButton button("Hello World!",&window);
    button.resize(200,50);
    button.move(QPoint{0,0});
    button.show();

    return app.exec();
};