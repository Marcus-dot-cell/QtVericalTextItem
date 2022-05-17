#ifndef CGRAPHICSEDIT_H
#define CGRAPHICSEDIT_H

#include <QGraphicsObject>
#include <QFont>
#include <QMutex>
#include <QClipboard>
#include <QUndoStack>

class QTimer;
class SelectedRegion;

class CGraphicsEdit : public QGraphicsObject
{
    Q_OBJECT
public:
    //对齐方式
    enum TextDirection{
        Direction_Top,
        Direction_Center,
        Direction_Bottom,
    };

    CGraphicsEdit(QGraphicsItem* parent = nullptr);
    virtual ~CGraphicsEdit();

    void setTextInteractionFlags(Qt::TextInteractionFlags flags);
    Qt::TextInteractionFlags textInteractionFlags() const;
    QString text() const;
    void setText(const QString& text);
    void updateData(const QStringList& sl, int cols, int pos, int currCol);
    int alignment() const { return m_direction; }
    void setAlignment(TextDirection d);
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
    qreal getColWidth() const;
    //获取选中文本
    QString getSelectedText() const;
    //删除选中文字
    void deleteSelectText();
    //更新选中文本
    void updateSelectedText(int beginCol, int endCol, int beginPos, int endPos);
    //获取字符串竖排高度
    qreal getStrHeight(const QString& str) const;
private:
    QFont          m_font;
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
    TextDirection  m_direction;
};

#endif // CGRAPHICSEDIT_H
