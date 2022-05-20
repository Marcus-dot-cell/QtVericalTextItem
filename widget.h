#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
class QFontComboBox;
class QPushButton;
class QSlider;
class QGraphicsView;
class QCheckBox;
class QComboBox;
class CGraphicsEdit;

class Widget : public QWidget
{
    Q_OBJECT
public:
    explicit Widget(QWidget *parent = nullptr);
signals:
public slots:
    void onSelectColor();
    void onSave();
    void onLoad();
    void onCheckBoxClicked(bool checked = false);
    void onFontSizeChanged(int value);
    void onRowSpaceChanged(const QString& text);
    void onaligentchanged(const QString& text);
    void onLetterSpaceChanged(const QString& text);
private:
    QFontComboBox* fontComboBox;
    QPushButton*   colorBtn;
    QSlider*       slider;
    QGraphicsView* view;
    QCheckBox*     boldCheckBox;
    QCheckBox*     italicCheckBox;
    QCheckBox*     delLineCheckBox;
    QCheckBox*     underlineCheckBox;
    QCheckBox*     strikeOutCheckBox;
    QComboBox*     aligentComboBox;
    QComboBox*     rowspacingComboBox;
    QComboBox*     letterspacingComboBox;
    CGraphicsEdit* textEdit;
};

#endif // WIDGET_H
