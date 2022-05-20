#if defined(_MSC_VER) && (_MSC_VER >= 1600)
# pragma execution_character_set("utf-8")
#endif

#include <QtWidgets/QApplication>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QTextCodec>
#include <QFontMetrics>
#include <QFont>
#include <QTextDocument>
#include "widget.h"
#include "cgraphicsedit.h"
#include <QFontComboBox>

void setTextCodec()
{
    qApp->setFont(QFont("Microsoft YaHei", 10));
#if (QT_VERSION <= QT_VERSION_CHECK(5,0,0))
    #if _MSC_VER
        QTextCodec* codec = QTextCodec::codecForName("GBK");
    #else
        QTextCodec* codec = QTextCodec::codecForName("UTF-8");
    #endif
        QTextCodec::setCodecForLocale(codec);
        QTextCodec::setCodecForCStrings(codec);
        QTextCodec::setCodecForTr(codec);
#else
    QTextCodec* codec = QTextCodec::codecForName("UTF-8");
    QTextCodec::setCodecForLocale(codec);
#endif
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    setTextCodec();

    Widget w;
    w.show();

    return a.exec();
}
