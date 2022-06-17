#include "widget.h"
#include <QGraphicsView>
#include <QGraphicsScene>
#include "cgraphicsedit.h"
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFontComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QColorDialog>
#include <QFile>

Widget::Widget(QWidget *parent)
    : QWidget(parent)
{
    setWindowState(Qt::WindowMaximized);

    fontComboBox = new QFontComboBox();
    fontComboBox->setEditable(false);

    colorBtn = new QPushButton(tr("color select"));
    slider = new QSlider();
    slider->setMinimum(1);
    slider->setMaximum(1000);
    slider->setOrientation(Qt::Horizontal);
    slider->setFixedWidth(300);

    boldCheckBox = new QCheckBox(tr("Bold"));
    italicCheckBox = new QCheckBox(tr("Italic"));
    delLineCheckBox = new QCheckBox(tr("Overline"));
    underlineCheckBox = new QCheckBox(tr("Underline"));
    strikeOutCheckBox = new QCheckBox(tr("StrikeOut"));
    aligentComboBox = new QComboBox();
    aligentComboBox->addItems({"Top", "VCenter", "Bottom", "Left", "Right", "HCenter"});
    rowspacingComboBox = new QComboBox();
    rowspacingComboBox->addItems({"0", "6", "8", "9", "10", "20", "30", "40", "50", "60", "70", "80"});
    letterspacingComboBox = new QComboBox;
    letterspacingComboBox->addItems({"200", "100", "75","50", "25", "10", "0", "-10", "-25", "-50", "-75", "-100"});
    letterspacingComboBox->setCurrentText("0");
    directionComboBox = new QComboBox;
    directionComboBox->addItems({"Horizontal", "Vertical"});
    directionComboBox->setCurrentIndex(1);

    QGraphicsScene* scene = new QGraphicsScene();
    view = new QGraphicsView();

    textEdit = new CGraphicsEdit();
    scene->addItem(textEdit);

    view->setScene(scene);

    QHBoxLayout* hLayout = new QHBoxLayout;
    hLayout->addWidget(new QLabel(tr("TextSize: ")));
    hLayout->addWidget(slider);
    hLayout->addStretch();

    QHBoxLayout* hLayout2 = new QHBoxLayout;
    QPushButton* saveBtn = new QPushButton(tr("Save"));
    QPushButton* loadBtn = new QPushButton(tr("Load"));
    hLayout2->addWidget(saveBtn);
    hLayout2->addWidget(loadBtn);

    QVBoxLayout* vLayout = new QVBoxLayout;
    vLayout->addWidget(fontComboBox);
    vLayout->addWidget(boldCheckBox);
    vLayout->addWidget(italicCheckBox);
    vLayout->addWidget(delLineCheckBox);
    vLayout->addWidget(underlineCheckBox);
    vLayout->addWidget(strikeOutCheckBox);
    vLayout->addWidget(colorBtn);
    vLayout->addLayout(hLayout);
    vLayout->addWidget(aligentComboBox);
    vLayout->addWidget(rowspacingComboBox);
    vLayout->addWidget(letterspacingComboBox);
    vLayout->addWidget(directionComboBox);
    vLayout->addLayout(hLayout2);
    vLayout->addStretch();

    QHBoxLayout* mainLayout = new QHBoxLayout();
    mainLayout->addLayout(vLayout, 1);
    mainLayout->addWidget(view, 5);
    this->setLayout(mainLayout);

    connect(fontComboBox, &QFontComboBox::currentTextChanged, textEdit, &CGraphicsEdit::onFontChanged);
    connect(colorBtn, &QPushButton::clicked, this, &Widget::onSelectColor);
    connect(saveBtn, &QPushButton::clicked, this, &Widget::onSave);
    connect(loadBtn, &QPushButton::clicked, this, &Widget::onLoad);
    connect(boldCheckBox, &QCheckBox::clicked, this, &Widget::onCheckBoxClicked);
    connect(italicCheckBox, &QCheckBox::clicked, this, &Widget::onCheckBoxClicked);
    connect(delLineCheckBox, &QCheckBox::clicked, this, &Widget::onCheckBoxClicked);
    connect(underlineCheckBox, &QCheckBox::clicked, this, &Widget::onCheckBoxClicked);
    connect(strikeOutCheckBox, &QCheckBox::clicked, this, &Widget::onCheckBoxClicked);
    connect(slider, &QSlider::valueChanged, this, &Widget::onFontSizeChanged);
    connect(rowspacingComboBox, &QComboBox::currentTextChanged, this, &Widget::onRowSpaceChanged);
    connect(aligentComboBox, &QComboBox::currentTextChanged, this, &Widget::onaligentchanged);
    connect(letterspacingComboBox, &QComboBox::currentTextChanged, this, &Widget::onLetterSpaceChanged);
    connect(directionComboBox, &QComboBox::currentTextChanged, this, &Widget::onDirectionChanged);
}

void Widget::onSelectColor()
{
    QColorDialog dialog(this);
    QObject::connect(&dialog, &QColorDialog::currentColorChanged, textEdit, &CGraphicsEdit::onColorSelected);
    dialog.exec();
}

void Widget::onSave()
{
    QString htmlText = textEdit->toHtml();
    //save
    if (!htmlText.isEmpty()) {
        QFile f("./test.html");
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(htmlText.toLocal8Bit());
            f.close();
        }
    }
}

void Widget::onLoad()
{
    QString htmlStr;
    QFile f("./test.html");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = f.readAll();
        htmlStr = QString::fromLocal8Bit(data);
    }
    textEdit->setText(htmlStr);
}

void Widget::onCheckBoxClicked(bool checked)
{
    QCheckBox* box = (QCheckBox*)sender();
    if (box == boldCheckBox) {
        textEdit->setBold(checked);
    } else if (box == italicCheckBox) {
        textEdit->setItalic(checked);
    } else if (box == delLineCheckBox) {
        textEdit->setOverline(checked);
    } else if (box == underlineCheckBox) {
        textEdit->setUnderline(checked);
    } else if (box ==strikeOutCheckBox) {
        textEdit->setStrikeOut(checked);
    }
}

void Widget::onFontSizeChanged(int value)
{
    textEdit->setFontSize(value);
}

void Widget::onRowSpaceChanged(const QString& text)
{
    int spacing = text.toInt();
    textEdit->setColumnSpacing(spacing);
}

void Widget::onaligentchanged(const QString& text)
{
    textEdit->setAlignment((CGraphicsEdit::TextAlignment)aligentComboBox->currentIndex());
}

void Widget::onLetterSpaceChanged(const QString& text)
{
    int spacing = text.toInt();
    textEdit->setLetterSpacing(spacing);
}

void Widget::onDirectionChanged(const QString& text)
{
    if (text == "Horizontal") {
        textEdit->setTextOriection(CGraphicsEdit::TextHorizontal);
    } else {
        textEdit->setTextOriection(CGraphicsEdit::TextVertical);
    }
}

