#include "widget.h"
#include <QTextEdit>
Widget::Widget(QWidget *parent)
    : QWidget{parent}
{
    QTextEdit* txtScrollEdit = new QTextEdit(this);
    txtScrollEdit->setFixedSize(this->size());
    connect(txtScrollEdit, SIGNAL(textChanged()), this, SLOT(slotTextChanged()));
}

void Widget::slotTextChanged()
{
    QTextEdit* txtScrollEdit = (QTextEdit*)sender();
    QStringList sl;
    sl=txtScrollEdit->toPlainText().split(" ");
    txtScrollEdit->setText(sl.join("\n\n"));
    txtScrollEdit->setWordWrapMode (QTextOption::WrapAnywhere);// txtScrollEdit is QTextEdit[/COLOR]
    txtScrollEdit->setLineWrapMode(QTextEdit::FixedColumnWidth);
    txtScrollEdit->setLineWrapColumnOrWidth(1);
}
