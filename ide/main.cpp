#include <QApplication>
#include <QPushButton>
#include <QtWidgets>

int main(int argc,char *argv[]){


    QApplication app(argc,argv);

    QWidget window;
    window.resize(1500,1500);
    window.show();
    window.setWindowTitle(QApplication::translate("toplevel","Starbytes IDE"));


    QPushButton button("Hello World!",&window);
    button.resize(200,50);
    button.show();

    QPalette palette {QColor {255,0,0}};
    QColor shadowColor {0,0,0};
    palette.setColor(QPalette::Shadow,shadowColor);
    button.setPalette(palette);

    return QApplication::exec();
};