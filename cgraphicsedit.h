#ifndef CGRAPHICSEDIT_H
#define CGRAPHICSEDIT_H

#include <QGraphicsObject>
#include <QFont>
#include <QMutex>
#include <QClipboard>
#include <QUndoStack>
#include <QGraphicsTextItem>
#include <QTextCharFormat>

typedef struct SCharFormat{
    QString  fontText = "MicroSoft YaHei";
    QColor   fontColor = Qt::black;
    int      fontSize = 10;
    bool     bold = false;
    bool     italic = false;
    bool     overline = false;
    bool     underline = false;
    bool     strikeOut = false;
    qreal    letterSpacing = 0;

    void setFont(QFont* f) const {
        f->setFamily(fontText);
        f->setBold(bold);
        f->setItalic(italic);
        f->setOverline(overline);
        f->setPointSize(fontSize);
        f->setUnderline(underline);
        f->setStrikeOut(strikeOut);
        f->setLetterSpacing(QFont::AbsoluteSpacing, letterSpacing);
    }

    void fromFont(const QFont& f) {
        fontText = f.family();
        fontSize = f.pointSize();
        bold = f.bold();
        italic = f.italic();
        overline = f.overline();
        underline = f.underline();
        strikeOut = f.strikeOut();
        letterSpacing = f.letterSpacing();
    }
} SCharFormat;

class QTimer;
class SelectedRegion;

class CGraphicsEdit : public QGraphicsObject
{
    Q_OBJECT
public:
    //对齐方式
    enum TextAlignment{
        AlignmentTop,
        AlignmentCenter,
        AlignmentBottom,
        AlignmentLeft,
        AlignmentRight,
        AlignmentHCenter,
    };
    //文字方向
    enum TextOriection{
        TextHorizontal,
        TextVertical,
    };

    CGraphicsEdit(QGraphicsItem* parent = nullptr);
    virtual ~CGraphicsEdit();

    void setTextInteractionFlags(Qt::TextInteractionFlags flags);
    Qt::TextInteractionFlags textInteractionFlags() const;
    QString text() const;
    void setText(const QString& text);
    void updateData(const QStringList& sl, int cols, int pos, int currCol, const QList<QList<SCharFormat>>& sf);
    int alignment() const { return m_alignment; }
    void setAlignment(TextAlignment d);
    QString toHtml() const;
    void setBold(bool enabled);
    void setItalic(bool enabled);
    void setOverline(bool enabled);
    void setUnderline(bool enabled);
    void setFontSize(int size);
    void setStrikeOut(bool enabled);
    void setColumnSpacing(qreal spacing);
    void setLetterSpacing(qreal spacing);
    void setTextOriection(TextOriection oriection);
protected:
    virtual QRectF boundingRect() const override;
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    virtual void keyPressEvent(QKeyEvent *event) override;
    virtual void keyReleaseEvent(QKeyEvent *event) override;
    virtual void inputMethodEvent(QInputMethodEvent *event) override;
    virtual bool sceneEvent(QEvent *event) override;
    virtual void focusEvent(QFocusEvent* e);
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
public slots:
    void onTimeout();
    void onFontChanged(const QString& text);
    void onColorSelected(const QColor &color);
private:
    void processEvent(QEvent* event);
    bool isAcceptableInput(QKeyEvent* e);
    bool isCommonTextEditShortcut(QKeyEvent* e);
    void selectAll();
    void copy();
    void undo();
    void redo();
    void cut();
    void paste(QClipboard::Mode);
    //获取列宽
    qreal getColWidth(int index) const;
    qreal getColXPostion(int index) const;
    //获取行高
    qreal getRowHeight(int index) const;
    qreal getRowYPostion(int index) const;
    //获取选中文本
    QString getSelectedText() const;
    //删除选中文字
    void deleteSelectText();
    //更新选中文本
    void updateSelectedText(int beginCol, int endCol, int beginPos, int endPos);
    //获取字符串竖排高度
    qreal getStrHeight(int index) const;
    //获取字符串横排宽度
    qreal getStrWidth(int index) const;
    //垂直绘制
    void verticalPaint(QPainter* painter, const QRectF& r);
    //水平绘制
    void horizontalPaint(QPainter* painter, const QRectF& r);
private:
    QStringList    m_textList;
    QTimer         *m_timer;
    bool           m_showCursor;
    int            m_postion;   //光标位置
    QMutex         m_mutex;
    int            m_cols;          //列数
    int            m_currColumn;    //当前列标号
    Qt::TextInteractionFlags interactionFlags;
    bool           m_repaint = false;
    //选中范围
    SelectedRegion*  m_selectedRegion;
    QWidget*       contextWidget = nullptr;
    bool           m_mousePressed = false;
    QUndoStack*    m_undoStack;
    TextAlignment  m_alignment;
    TextOriection  m_oriection;
    SCharFormat    m_textFormat;
    //兼容html
    QGraphicsTextItem*   m_textItem;
    QList<QList<SCharFormat>>   m_charFormats;
    qreal         m_columnSpacing = 0;
};

#endif // CGRAPHICSEDIT_H
