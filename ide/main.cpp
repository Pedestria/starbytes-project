#include <QApplication>
#include <QPushButton>
#include <QtWidgets>

int main(int argc,char *argv[]){


    QApplication app(argc,argv);

    QWidget window;
    window.resize(1000,1000);
    window.show();
    window.setWindowTitle(QApplication::translate("toplevel","Starbytes IDE"));


    QPushButton button("Hello World!",&window);
    button.resize(200,50);
    button.show();

    return QApplication::exec();
};
