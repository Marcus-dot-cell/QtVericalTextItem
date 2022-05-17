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
#if 1
    QGraphicsScene scene;
    QGraphicsView view;
    view.setScene(&scene);
    view.setFixedSize(1024, 768);
#if 0
    CVericalTextItem* item = new CVericalTextItem();
    item->setFont(QFont("Microsoft YaHei", 10));
#else
    CGraphicsEdit* item = new CGraphicsEdit();
    item->setTextInteractionFlags(Qt::TextEditorInteraction);
    item->setAlignment(CGraphicsEdit::Direction_Center);
#endif

    scene.addItem(item);
    view.show();
#else
    Widget w;
    w.show();
#endif

    return a.exec();
}
