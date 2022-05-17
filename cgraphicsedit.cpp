#include "cgraphicsedit.h"
#include <QTimer>
#include <QFontMetrics>
#include <QPainter>
#include <QKeyEvent>
#include <QEvent>
#include <QGuiApplication>
#include <QGraphicsScene>
#include <QGraphicsSceneEvent>
#include <QDebug>
#include <QUndoCommand>

static const qreal DEFAULT_EDIT_HEIGHT = 300;

class CTextChanged : public QUndoCommand
{
public:
    CTextChanged(CGraphicsEdit* item, const QStringList& sl, int pos, int cols, int curCol):
        m_d(item),
        m_textList(sl),
        m_postion(pos),
        m_cols(cols),
        m_currColumn(curCol)

    {

    }

    void undo() override {
        if (m_d) {
            m_d->updateData(m_textList, m_cols, m_postion, m_currColumn);
        }
    }

    void redo() override {
    }
private:
    CGraphicsEdit*      m_d = nullptr;
    QStringList         m_textList;
    int                 m_postion = 0;
    int                 m_cols = 0;          //列数
    int                 m_currColumn = 0;    //当前列标号
};

//文本选中区域
class SelectedRegion{
    int m_startCol = 0;
    int m_endCol = 0;
    int m_startPos = 0;
    int m_endPos = 0;
public:
    SelectedRegion() {
    }
    inline void setStartCol(int col) { m_startCol = col;}
    inline void setEndCol(int col) { m_endCol = col;}
    inline void setStartPos(int pos) { m_startPos = pos;}
    inline void setEndPos(int pos) { m_endPos = pos;}
    bool selected() const { return (m_startCol != m_endCol || m_startPos != m_endPos); }
    void clean() { memset(this, 0, sizeof(*this)); }
    inline int startCol() const {
        return (m_startCol > m_endCol ? m_endCol : m_startCol);
    }
    inline int endCol() const {
        if (m_startCol != startCol())
            return m_startCol;
        return m_endCol;
    }
    inline int startPos() const {
        int sc = startCol();
        if (sc != m_startCol && m_startCol > m_endCol) {
            return m_endPos;
        }
        return m_startPos;
    }
    inline int endPos() const {
        int ec = endCol();
        if (ec != m_endCol && m_endCol < m_startCol) {
            return m_startPos;
        }
        return m_endPos;
    }
    //0: r->l 1: l->r 2: t->b 3: b->t
    inline int direction() const {
        if (m_startCol <= m_endCol) {
            if (m_startCol == m_endCol) {
                return (m_startPos > m_endPos ? 3 : 2);
            }
            return 0;
        }
        return 1;
    }
    inline void setRegion(int beginCol, int endCol, int beginPos, int endPos) {
        m_startCol = beginCol;
        m_startPos = beginPos;
        m_endCol = endCol;
        m_endPos = endPos;
    }
};

CGraphicsEdit::CGraphicsEdit(QGraphicsItem* parent):
    QGraphicsObject(parent),
    m_font(QFont("MicroSoft YaHei", 15)),
    m_showCursor(false),
    m_postion(0),
    m_cols(1),
    m_currColumn(0),
    m_selectedRegion(new SelectedRegion),
    m_undoStack(new QUndoStack(this)),
    m_direction(Direction_Top)
{   
    setFocus();
    setAcceptDrops(true);
    setAcceptHoverEvents(true);
    setFlag(ItemIsFocusable);
    //初始化文字列表
    m_textList << QString("");
    //光标闪烁
    m_timer = new QTimer(this);
    m_timer->start(500);
    connect(m_timer, &QTimer::timeout, this, &CGraphicsEdit::onTimeout);
}

CGraphicsEdit::~CGraphicsEdit()
{
    if (m_timer) {
        m_timer->stop();
        delete m_timer;
        m_timer = nullptr;
    }
}

QRectF CGraphicsEdit::boundingRect() const
{
    QFontMetricsF metrics(m_font);
    const qreal fontHeight = metrics.height();
    const qreal adjust = fontHeight/3;
    qreal maxHeight = DEFAULT_EDIT_HEIGHT;
    foreach(QString strLine, m_textList) {
        qreal h = getStrHeight(strLine);
        if (h > maxHeight) {
            maxHeight = h;
        }
    }

    qreal w = getColWidth() * m_cols;
    QRectF r =  QRectF(-w/2 - adjust, -maxHeight/2 - adjust, w + 2*adjust, maxHeight + 2*adjust);
    return r;
}

void CGraphicsEdit::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget )
{
    if (m_repaint) {
        scene()->update();
        m_repaint = false;
    }

    QFontMetricsF metrics(m_font);
    const qreal fontHeight = metrics.height();
    const qreal adjust = fontHeight/3;
    const QRectF r = boundingRect();
    const qreal cw = getColWidth();

    //绘制虚线框
    QPen pen;
    pen.setColor(Qt::black);
    pen.setStyle(Qt::DotLine);
    painter->save();
    painter->setPen(pen);
    painter->drawRect(r);
    painter->restore();
    //绘制选中区域
    int startCol = m_selectedRegion->startCol();
    int endCol = m_selectedRegion->endCol();
    int startPos = m_selectedRegion->startPos();
    int endPos = m_selectedRegion->endPos();
    if (startCol == endCol && startPos > endPos) {
        qSwap(startPos, endPos);
    }
    if (m_selectedRegion->selected()) {
        painter->save();
        for (int i = startCol; i <= endCol; ++i) {
            const QString s = m_textList.at(i);
            int ep = (i != endCol ? s.length() : endPos);
            int bp = (i != startCol ? 0 : startPos);
            if (ep == bp) continue;
            ep -= 1;

            qreal sy = 0;
            if (m_direction == Direction_Top) {
                sy = r.top() + adjust;
            } else if (m_direction == Direction_Center) {
                qreal textHeight = getStrHeight(s);
                sy = -textHeight/2 - adjust/2;
            } else {
                qreal textHeight = getStrHeight(s);
                sy = r.bottom() - adjust - textHeight;
            }

            qreal sx = r.right() - (i + 1) * cw - adjust;
            qreal starty = sy;
            qreal endy = starty;
            for (int j = 0; j < s.length(); j++) {
                if (j == bp) {
                    starty = sy;
                }
                QChar c = s.at(j);
                if (c < 128) {
                    sy += metrics.width(c);
                } else {
                    sy += fontHeight;
                }
                if (j == ep) {
                    endy = sy;
                }
            }

            if (starty != endy) {
                painter->setBrush(QColor("#0078D7"));
                painter->drawRect(QRectF(sx, starty, cw, endy - starty));
            }
        }
        painter->restore();
    }
    //绘制文字
    painter->save();  
    painter->setFont(m_font);
    int index = 0;
    qreal hx, vy;
    qreal cursorX = r.left() + adjust + (m_cols - m_currColumn - 1)*cw;
    qreal cursorY = r.top() + adjust;
    if (m_direction == Direction_Center) {
        qreal textHeight = getStrHeight(m_textList.at(m_currColumn));
        cursorY = -textHeight/2 - adjust;
    } else if (m_direction == Direction_Bottom) {
        qreal textHeight = getStrHeight(m_textList.at(m_currColumn));
        cursorY = r.bottom() - adjust - textHeight;
    }

    foreach (QString l, m_textList) {
        qreal vx = 0;
        if (m_direction == Direction_Top) {
            vx = r.top() + adjust;
        } else if (m_direction == Direction_Center) {
            qreal textHeight = getStrHeight(l);
            vx = -textHeight/2 - adjust/2;
        } else {
            qreal textHeight = getStrHeight(l);
            vx = r.bottom() - adjust - textHeight;
        }
        bool isCurrLine = (index == m_currColumn);
        int cursorIndex = 0;
        foreach(QChar c, l) {
            QPen  textPen;
            if (m_selectedRegion->selected() && index >= startCol && index <= endCol) {
                if (startCol == endCol) {
                    if (cursorIndex >= startPos &&
                        cursorIndex < endPos) {
                        textPen.setColor(QColor("#FFF8F0"));
                    } else {
                        textPen.setColor(Qt::black);
                    }
                } else {
                    if (index == startCol)
                        if (cursorIndex >= startPos)
                            textPen.setColor(QColor("#FFF8F0"));
                        else
                            textPen.setColor(Qt::black);
                    else if (index == endCol)
                        if (cursorIndex < endPos)
                            textPen.setColor(QColor("#FFF8F0"));
                        else
                            textPen.setColor(Qt::black);
                    else
                        textPen.setColor(QColor("#FFF8F0"));
                }
            } else {
                textPen.setColor(Qt::black);
            }
            painter->setPen(textPen);
            if (c < 128) {
                painter->rotate(90);
                qreal w = metrics.width(c);
                vy = -r.right() + (index + 1) * cw - cw/4 + adjust;
                painter->drawText(QPointF(vx, vy), c);
                vx += w;
                painter->rotate(-90);
            } else {
                qreal w = metrics.width(c);
                hx = r.right() - (index + 1) * cw + (cw - w)/2 - adjust;
                if (m_direction == Direction_Bottom) {
                    painter->drawText(QPointF(hx, vx + fontHeight*7/8), c);
                } else if (m_direction == Direction_Top) {
                    painter->drawText(QPointF(hx, vx + fontHeight*2/3), c);
                } else {
                    painter->drawText(QPointF(hx, vx + fontHeight*7/8), c);
                }

                vx += fontHeight;
            }
            //光标y坐标
            if (isCurrLine && ((cursorIndex + 1) == m_postion)) {
                cursorY = vx;
                qDebug() << "cursorY: " << cursorY << ", pos: " << m_postion;
            }
            ++cursorIndex;
        }
        ++index;
    }
    painter->restore();
    //绘制光标
    painter->save();
    if (!m_showCursor) {
        QPen pen2;
        pen2.setStyle(Qt::SolidLine);
        pen2.setColor(Qt::transparent);
        painter->setPen(pen2);
    }
    painter->drawLine(QPointF(cursorX, cursorY), QPointF(cursorX + getColWidth(), cursorY));
    painter->restore();
}

void CGraphicsEdit::keyPressEvent(QKeyEvent *e)
{
    if (!(interactionFlags & Qt::TextEditable)) {
        e->ignore();
        return;
    }

#ifndef QT_NO_SHORTCUT
    if (e == QKeySequence::SelectAll) {
            selectAll();
            goto accept;
    } else if (e == QKeySequence::Undo) {
        undo();
        goto accept;
    }
    else if (e == QKeySequence::Redo) {
        redo();
        goto accept;
    }
#ifndef QT_NO_CLIPBOARD
    else if (e == QKeySequence::Copy) {
            copy();
            goto accept;
    } else if (e == QKeySequence::Cut) {
        cut();
        goto accept;
    } else if (e == QKeySequence::Paste) {
        QClipboard::Mode mode = QClipboard::Clipboard;
        if (QGuiApplication::clipboard()->supportsSelection()) {
            if (e->modifiers() == (Qt::CTRL | Qt::SHIFT) && e->key() == Qt::Key_Insert)
                mode = QClipboard::Selection;
        }
        paste(mode);
        goto accept;
    }
#endif
#endif // QT_NO_SHORTCUT

//    if (interactionFlags & Qt::TextSelectableByKeyboard
//        && cursorMoveKeyEvent(e))
//        goto accept;

//    if (interactionFlags & Qt::LinksAccessibleByKeyboard) {
//        if ((e->key() == Qt::Key_Return
//             || e->key() == Qt::Key_Enter
//#ifdef QT_KEYPAD_NAVIGATION
//             || e->key() == Qt::Key_Select
//#endif
//             )
//            && cursor.hasSelection()) {

//            e->accept();
//            activateLinkUnderCursor();
//            return;
//        }
//    }



//    if (e->key() == Qt::Key_Direction_L || e->key() == Qt::Key_Direction_R) {
//        QTextBlockFormat fmt;
//        fmt.setLayoutDirection((e->key() == Qt::Key_Direction_L) ? Qt::LeftToRight : Qt::RightToLeft);
//        cursor.mergeBlockFormat(fmt);
//        goto accept;
//    }

    // schedule a repaint of the region of the cursor, as when we move it we
    // want to make sure the old cursor disappears (not noticeable when moving
    // only a few pixels but noticeable when jumping between cells in tables for
    // example)
    //repaintSelection();

    if (e->key() == Qt::Key_Backspace && !(e->modifiers() & ~Qt::ShiftModifier)) {
        do {
            if (m_selectedRegion->selected()) {
                deleteSelectText();
                break;
            }

            if (m_currColumn == 0 && m_postion == 0)
                break;

            QUndoCommand* command = new CTextChanged(this, m_textList, m_postion, m_cols, m_currColumn);
            m_undoStack->push(command);

            QString l = m_textList.at(m_currColumn);
            if (m_postion == 0) {
                if (m_cols > 1) {
                    m_textList.removeAt(m_currColumn);
                }
                m_cols--;
                m_currColumn--;
                QString lstr = m_textList.at(m_currColumn);
                m_postion = lstr.length();
                m_textList[m_currColumn] = lstr + l;
            } else {
                l.remove(m_postion - 1, 1);
                m_textList[m_currColumn] = l;
                m_postion--;
            }
        } while (0);
        m_repaint = true;
        goto accept;
    } else if (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return) {
        deleteSelectText();
        QString l = m_textList.at(m_currColumn);
        QString rStr = l.right(l.length() - m_postion);
        QString lStr = l.left(m_postion);
        m_textList[m_currColumn] = lStr;
        m_cols++;
        m_currColumn++;
        m_textList.insert(m_currColumn, rStr);
        m_postion = 0;
        m_repaint = true;
        goto accept;
    } else if (e->key() == Qt::Key_Delete) {
        do {
            if (m_selectedRegion->selected()) {
                deleteSelectText();
                break;
            }

            QUndoCommand* command = new CTextChanged(this, m_textList, m_postion, m_cols, m_currColumn);
            m_undoStack->push(command);

            QString s = m_textList.at(m_currColumn);
            if ((m_currColumn == 0 && s.isEmpty() && m_cols == 1) ||
                (m_currColumn + 1 == m_cols && s.length() == m_postion))
                break;
            if (m_postion == s.length()) {
                QString ls = m_textList.at(m_currColumn + 1);
                m_textList[m_currColumn] = s + ls;
                m_textList.removeAt(m_currColumn + 1);
                m_cols--;
            } else {
                s.remove(m_postion, 1);
                m_textList[m_currColumn] = s;
            }
        }while (0);
        m_repaint = true;
        goto accept;
    } else if (e->key() == Qt::Key_Home) {
        m_postion = 0;
        m_selectedRegion->clean();
        goto accept;
    } else if (e->key() == Qt::Key_End) {
        QString s = m_textList.at(m_currColumn);
        m_postion = s.length();
        m_selectedRegion->clean();
        goto accept;
    } else if (e->key() == Qt::Key_Left) {
        do {
            if (m_currColumn + 1 >= m_cols)
                break;
            QString ls = m_textList.at(m_currColumn + 1);
            if (ls.length() < m_postion) {
                m_postion = ls.length();
            }
            m_currColumn++;
        }while(0);
        m_selectedRegion->clean();
        goto accept;
    } else if (e->key() == Qt::Key_Right) {
        do {
            if (m_currColumn == 0)
                break;
            QString rs = m_textList.at(m_currColumn - 1);
            if (m_postion > rs.length()) {
                m_postion = rs.length();
            }
            m_currColumn--;
        }while(0);
        m_selectedRegion->clean();
        goto accept;
    } else if (e->key() == Qt::Key_Up) {
        do {
            if (m_postion == 0 && m_currColumn == 0)
                break;
            if (m_postion == 0) {
                m_currColumn--;
                m_postion = m_textList.at(m_currColumn).length();
            } else {
                m_postion--;
            }
        } while(0);
        m_selectedRegion->clean();
        goto accept;
    }else if (e->key() == Qt::Key_Down) {
        do {
            QString s = m_textList.at(m_currColumn);
            if (m_postion == s.length() && m_currColumn + 1 == m_cols)
                break;
            if (m_postion == s.length()) {
                m_currColumn++;
                m_postion = 0;
            } else {
                m_postion++;
            }

        }while(0);
        m_selectedRegion->clean();
        goto accept;
    }
    else {
        if (e->modifiers() & Qt::ControlModifier || e->modifiers() & Qt::ShiftModifier) {
            e->ignore();
            return;
        }

        QUndoCommand* command = new CTextChanged(this, m_textList, m_postion, m_cols, m_currColumn);
        m_undoStack->push(command);

        QString currText = m_textList.at(m_currColumn);
        currText.insert(m_postion, e->text());
        m_postion += e->text().length();
        m_textList[m_currColumn] = currText;

        goto accept;
    }

    if (false) {
    }
    else if (e == QKeySequence::Delete) {
    } else if (e == QKeySequence::Backspace) {

    }/*else if (e == QKeySequence::DeleteEndOfWord) {
        if (!cursor.hasSelection())
            cursor.movePosition(QTextCursor::NextWord, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
    }*/
//    else if (e == QKeySequence::DeleteStartOfWord) {
//        if (!cursor.hasSelection())
//            cursor.movePosition(QTextCursor::PreviousWord, QTextCursor::KeepAnchor);
//        cursor.removeSelectedText();
//    }
//    else if (e == QKeySequence::DeleteEndOfLine) {
//        QTextBlock block = cursor.block();
//        if (cursor.position() == block.position() + block.length() - 2)
//            cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
//        else
//            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
//        cursor.removeSelectedText();
//    }
    else {
        goto process;
    }
    goto accept;

process:
    {
        if (isAcceptableInput(e)) {
//            if (overwriteMode
//                // no need to call deleteChar() if we have a selection, insertText
//                // does it already
//                && !cursor.hasSelection()
//                && !cursor.atBlockEnd())
//                cursor.deleteChar();

//            cursor.insertText(e->text());
//            selectionChanged();
        } else {
            e->ignore();
            return;
        }
    }

 accept:
    e->accept();
    update();
}

void CGraphicsEdit::keyReleaseEvent(QKeyEvent *event)
{

}

void CGraphicsEdit::inputMethodEvent(QInputMethodEvent *event)
{
    if (event->commitString().length() > 0) {
        QUndoCommand* command = new CTextChanged(this, m_textList, m_postion, m_cols, m_currColumn);
        m_undoStack->push(command);

        QString currText = m_textList.at(m_currColumn);
        currText.insert(m_postion, event->commitString());
        m_postion += event->commitString().length();
        m_textList[m_currColumn] = currText;
        m_repaint = true;
    }
}

void CGraphicsEdit::onTimeout()
{
    m_mutex.lock();
    m_showCursor = !m_showCursor;
    update();
    m_mutex.unlock();
}

bool CGraphicsEdit::sceneEvent(QEvent *event)
{
    QEvent::Type t = event->type();
    if (t == QEvent::KeyPress || t == QEvent::KeyRelease) {
        int k = ((QKeyEvent *)event)->key();
        if (k == Qt::Key_Tab || k == Qt::Key_Backtab) {
            return true;
        }
    }
    bool result = QGraphicsItem::sceneEvent(event);
    // Ensure input context is updated.
    switch (t) {
    case QEvent::ContextMenu:
    case QEvent::FocusIn:
    case QEvent::FocusOut:
    case QEvent::GraphicsSceneDragEnter:
    case QEvent::GraphicsSceneDragLeave:
    case QEvent::GraphicsSceneDragMove:
    case QEvent::GraphicsSceneDrop:
    case QEvent::GraphicsSceneHoverEnter:
    case QEvent::GraphicsSceneHoverLeave:
    case QEvent::GraphicsSceneHoverMove:
    case QEvent::GraphicsSceneMouseDoubleClick:
    case QEvent::GraphicsSceneMousePress:
    case QEvent::GraphicsSceneMouseMove:
    case QEvent::GraphicsSceneMouseRelease:
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
        // Reset the focus widget's input context, regardless
        // of how this item gained or lost focus.
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut) {
            QGuiApplication::inputMethod()->reset();
        } else {
            QGuiApplication::inputMethod()->update(Qt::ImQueryInput);
        }
        break;
    case QEvent::ShortcutOverride:
        processEvent(event);
        return true;
    default:
        break;
    }

    return result;
}

void CGraphicsEdit::processEvent(QEvent* e)
{
    QTransform transform;
    transform.translate(0, boundingRect().height());
    if (interactionFlags == Qt::NoTextInteraction) {
        e->ignore();
        return;
    }

    switch (e->type()) {
#if QT_CONFIG(graphicsview)
        case QEvent::GraphicsSceneMouseMove:
        case QEvent::GraphicsSceneMousePress:
        case QEvent::GraphicsSceneMouseRelease:
        case QEvent::GraphicsSceneMouseDoubleClick:
        case QEvent::GraphicsSceneContextMenu:
        case QEvent::GraphicsSceneHoverEnter:
        case QEvent::GraphicsSceneHoverMove:
        case QEvent::GraphicsSceneHoverLeave:
        case QEvent::GraphicsSceneHelp:
        case QEvent::GraphicsSceneDragEnter:
        case QEvent::GraphicsSceneDragMove:
        case QEvent::GraphicsSceneDragLeave:
        case QEvent::GraphicsSceneDrop: {
            QGraphicsSceneEvent *ev = static_cast<QGraphicsSceneEvent *>(e);
            contextWidget = ev->widget();
            break;
        }
#endif // QT_CONFIG(graphicsview)
        default: break;
    };

    switch (e->type()) {
        case QEvent::KeyPress:
            keyPressEvent(static_cast<QKeyEvent *>(e));
            break;
//        case QEvent::MouseButtonPress: {
//            QMouseEvent *ev = static_cast<QMouseEvent *>(e);
//            d->mousePressEvent(ev, ev->button(), transform.map(ev->pos()), ev->modifiers(),
//                               ev->buttons(), ev->globalPos());
//            break; }
//        case QEvent::MouseMove: {
//            QMouseEvent *ev = static_cast<QMouseEvent *>(e);
//            d->mouseMoveEvent(ev, ev->button(), transform.map(ev->pos()), ev->modifiers(),
//                              ev->buttons(), ev->globalPos());
//            break; }
//        case QEvent::MouseButtonRelease: {
//            QMouseEvent *ev = static_cast<QMouseEvent *>(e);
//            d->mouseReleaseEvent(ev, ev->button(), transform.map(ev->pos()), ev->modifiers(),
//                                 ev->buttons(), ev->globalPos());
//            break; }
//        case QEvent::MouseButtonDblClick: {
//            QMouseEvent *ev = static_cast<QMouseEvent *>(e);
//            d->mouseDoubleClickEvent(ev, ev->button(), transform.map(ev->pos()), ev->modifiers(),
//                                     ev->buttons(), ev->globalPos());
//            break; }
        case QEvent::InputMethod:
            inputMethodEvent(static_cast<QInputMethodEvent *>(e));
            break;
//#ifndef QT_NO_CONTEXTMENU
//    case QEvent::ContextMenu: {
//            QContextMenuEvent *ev = static_cast<QContextMenuEvent *>(e);
//            d->contextMenuEvent(ev->globalPos(), transform.map(ev->pos()), contextWidget);
//            break; }
//#endif // QT_NO_CONTEXTMENU
        case QEvent::FocusIn:
        case QEvent::FocusOut:
            focusEvent(static_cast<QFocusEvent *>(e));
            break;

//        case QEvent::EnabledChange:
//            d->isEnabled = e->isAccepted();
//            break;

//#ifndef QT_NO_TOOLTIP
//        case QEvent::ToolTip: {
//            QHelpEvent *ev = static_cast<QHelpEvent *>(e);
//            d->showToolTip(ev->globalPos(), transform.map(ev->pos()), contextWidget);
//            break;
//        }
//#endif // QT_NO_TOOLTIP

//#if QT_CONFIG(draganddrop)
//        case QEvent::DragEnter: {
//            QDragEnterEvent *ev = static_cast<QDragEnterEvent *>(e);
//            if (d->dragEnterEvent(e, ev->mimeData()))
//                ev->acceptProposedAction();
//            break;
//        }
//        case QEvent::DragLeave:
//            d->dragLeaveEvent();
//            break;
//        case QEvent::DragMove: {
//            QDragMoveEvent *ev = static_cast<QDragMoveEvent *>(e);
//            if (d->dragMoveEvent(e, ev->mimeData(), transform.map(ev->pos())))
//                ev->acceptProposedAction();
//            break;
//        }
//        case QEvent::Drop: {
//            QDropEvent *ev = static_cast<QDropEvent *>(e);
//            if (d->dropEvent(ev->mimeData(), transform.map(ev->pos()), ev->dropAction(), ev->source()))
//                ev->acceptProposedAction();
//            break;
//        }
//#endif

//#if QT_CONFIG(graphicsview)
//        case QEvent::GraphicsSceneMousePress: {
//            QGraphicsSceneMouseEvent *ev = static_cast<QGraphicsSceneMouseEvent *>(e);
//            d->mousePressEvent(ev, ev->button(), transform.map(ev->pos()), ev->modifiers(), ev->buttons(),
//                               ev->screenPos());
//            break; }
//        case QEvent::GraphicsSceneMouseMove: {
//            QGraphicsSceneMouseEvent *ev = static_cast<QGraphicsSceneMouseEvent *>(e);
//            d->mouseMoveEvent(ev, ev->button(), transform.map(ev->pos()), ev->modifiers(), ev->buttons(),
//                              ev->screenPos());
//            break; }
//        case QEvent::GraphicsSceneMouseRelease: {
//            QGraphicsSceneMouseEvent *ev = static_cast<QGraphicsSceneMouseEvent *>(e);
//            d->mouseReleaseEvent(ev, ev->button(), transform.map(ev->pos()), ev->modifiers(), ev->buttons(),
//                                 ev->screenPos());
//            break; }
//        case QEvent::GraphicsSceneMouseDoubleClick: {
//            QGraphicsSceneMouseEvent *ev = static_cast<QGraphicsSceneMouseEvent *>(e);
//            d->mouseDoubleClickEvent(ev, ev->button(), transform.map(ev->pos()), ev->modifiers(), ev->buttons(),
//                                     ev->screenPos());
//            break; }
//        case QEvent::GraphicsSceneContextMenu: {
//            QGraphicsSceneContextMenuEvent *ev = static_cast<QGraphicsSceneContextMenuEvent *>(e);
//            d->contextMenuEvent(ev->screenPos(), transform.map(ev->pos()), contextWidget);
//            break; }

//        case QEvent::GraphicsSceneHoverMove: {
//            QGraphicsSceneHoverEvent *ev = static_cast<QGraphicsSceneHoverEvent *>(e);
//            d->mouseMoveEvent(ev, Qt::NoButton, transform.map(ev->pos()), ev->modifiers(),Qt::NoButton,
//                              ev->screenPos());
//            break; }

//        case QEvent::GraphicsSceneDragEnter: {
//            QGraphicsSceneDragDropEvent *ev = static_cast<QGraphicsSceneDragDropEvent *>(e);
//            if (d->dragEnterEvent(e, ev->mimeData()))
//                ev->acceptProposedAction();
//            break; }
//        case QEvent::GraphicsSceneDragLeave:
//            d->dragLeaveEvent();
//            break;
//        case QEvent::GraphicsSceneDragMove: {
//            QGraphicsSceneDragDropEvent *ev = static_cast<QGraphicsSceneDragDropEvent *>(e);
//            if (d->dragMoveEvent(e, ev->mimeData(), transform.map(ev->pos())))
//                ev->acceptProposedAction();
//            break; }
//        case QEvent::GraphicsSceneDrop: {
//            QGraphicsSceneDragDropEvent *ev = static_cast<QGraphicsSceneDragDropEvent *>(e);
//            if (d->dropEvent(ev->mimeData(), transform.map(ev->pos()), ev->dropAction(), ev->source()))
//                ev->accept();
//            break; }
//#endif // QT_CONFIG(graphicsview)
//#ifdef QT_KEYPAD_NAVIGATION
//        case QEvent::EnterEditFocus:
//        case QEvent::LeaveEditFocus:
//            if (QApplicationPrivate::keypadNavigationEnabled())
//                d->editFocusEvent(e);
//            break;
//#endif
        case QEvent::ShortcutOverride:
            if (interactionFlags & Qt::TextEditable) {
                QKeyEvent* ke = static_cast<QKeyEvent *>(e);
                if (isCommonTextEditShortcut(ke))
                    ke->accept();
            }
            break;
        default:
            break;
    }
}

void CGraphicsEdit::focusEvent(QFocusEvent* e)
{

}

void CGraphicsEdit::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    do {
        if (event->button() != Qt::LeftButton)
            break;
        m_mousePressed = true;
        m_timer->stop();
        QPointF p = event->pos();
        const QRectF r = boundingRect();
        QFontMetricsF metrics(m_font);
        const qreal fontHeight = metrics.height();
        const qreal adjust = fontHeight/3;
        const qreal colw = getColWidth();
        for (int i = 0; i < m_cols; i++) {
            qreal rx = r.right() - adjust - i*colw;
            qreal lx = r.right() - adjust - (i+1)*colw;
            if (p.x() <= rx && p.x() > lx) {
                m_currColumn = i;
                //计算y坐标位置
                qreal y = r.top() + adjust;
                QString s = m_textList.at(i);
                if (m_direction == Direction_Center) {
                    qreal textHeight = getStrHeight(s);
                    y = -textHeight/2;
                } else if (m_direction == Direction_Bottom) {
                    qreal textHeight = getStrHeight(s);
                    y = r.bottom() - adjust - textHeight;
                }
                qreal prevY = y;

                if (s.isEmpty() || p.y() < y) {
                    m_postion = 0;
                } else {
                    for (int n = 0; n < s.length(); ++n) {
                        QChar c = s.at(n);
                        if (c < 128) {
                            y += metrics.width(c);
                        } else {
                            y += fontHeight;
                        }
                        if (p.y() >= prevY && p.y() < y) {
                            m_postion = n + 1;
                            break;
                        }
                    }
                    if (s.length() > 0 && p.y() > y) {
                        m_postion = s.length();
                    }
                }
                break;
            }
        }
        m_selectedRegion->setStartCol(m_currColumn);
        m_selectedRegion->setEndCol(m_currColumn);
        m_selectedRegion->setStartPos(m_postion);
        m_selectedRegion->setEndPos(m_postion);
        update();
    } while (0);
}

void CGraphicsEdit::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_mousePressed) {
        QPointF p = event->pos();
        const QRectF r = boundingRect();
        QFontMetricsF metrics(m_font);
        const qreal fontHeight = metrics.height();
        const qreal adjust = fontHeight/3;
        const qreal colw = getColWidth();
        for (int i = 0; i < m_cols; i++) {
            qreal rx = r.right() - adjust - i*colw;
            qreal lx = r.right() - adjust - (i+1)*colw;
            if (p.x() <= rx && p.x() > lx) {
                m_currColumn = i;
                //计算y坐标位置
                qreal y = r.top() + adjust;
                QString s = m_textList.at(i);
                if (m_direction == Direction_Center) {
                    qreal textHeight = getStrHeight(s);
                    y = -textHeight/2;
                } else if (m_direction == Direction_Bottom) {
                    qreal textHeight = getStrHeight(s);
                    y = r.bottom() - adjust - textHeight;
                }
                qreal prevY = y;
                if (s.isEmpty() || p.y() < y) {
                    m_postion = 0;
                } else {
                    for (int n = 0; n < s.length(); ++n) {
                        QChar c = s.at(n);
                        if (c < 128) {
                            y += metrics.width(c);
                        } else {
                            y += fontHeight;
                        }
                        if (p.y() >= prevY && p.y() < y) {
                            m_postion = n + 1;
                            break;
                        }
                    }
                    if (s.length() > 0 && p.y() > y) {
                        m_postion = s.length();
                    }
                }
                break;
            }
        }
        m_selectedRegion->setEndCol(m_currColumn);
        m_selectedRegion->setEndPos(m_postion);
        update();
    }
}

void CGraphicsEdit::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    m_timer->start();
    m_mousePressed = false;
}

void CGraphicsEdit::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    QPointF p = event->pos();
    const QRectF r = boundingRect();
    QFontMetricsF metrics(m_font);
    const qreal fontHeight = metrics.height();
    const qreal adjust = fontHeight/3;
    const qreal colw = getColWidth();
    for (int i = 0; i < m_cols; i++) {
        qreal rx = r.right() - adjust - i*colw;
        qreal lx = r.right() - adjust - (i+1)*colw;
        if (p.x() <= rx && p.x() > lx) {
            m_currColumn = i;
            break;
        }
    }

    int sl = m_textList.at(m_currColumn).length();
    if (sl > 0)
        updateSelectedText(m_currColumn, m_currColumn, 0,  sl);
    else
        m_postion = 0;
}

void CGraphicsEdit::setTextInteractionFlags(Qt::TextInteractionFlags flags)
{
    if (flags == Qt::NoTextInteraction)
        setFlags(this->flags() & ~(QGraphicsItem::ItemIsFocusable | QGraphicsItem::ItemAcceptsInputMethod));
    else
        setFlags(this->flags() | QGraphicsItem::ItemIsFocusable | QGraphicsItem::ItemAcceptsInputMethod);
    interactionFlags = flags;
}

Qt::TextInteractionFlags CGraphicsEdit::textInteractionFlags() const
{
    return interactionFlags;
}

bool CGraphicsEdit::isAcceptableInput(QKeyEvent* e)
{
    return true;
}

bool CGraphicsEdit::isCommonTextEditShortcut(QKeyEvent* e)
{
    return true;
}

void CGraphicsEdit::selectAll()
{
    updateSelectedText(0, m_cols - 1, 0, m_textList.at(m_cols - 1).length());
}

void CGraphicsEdit::copy()
{
    if (m_selectedRegion->selected()) {
        QString text = getSelectedText();
        QGuiApplication::clipboard()->setText(text);
    }
}

void CGraphicsEdit::undo()
{
    m_undoStack->undo();
}

void CGraphicsEdit::redo()
{
    m_undoStack->redo();
}

void CGraphicsEdit::cut()
{
    copy();
    deleteSelectText();
    m_repaint = true;
}

void CGraphicsEdit::paste(QClipboard::Mode)
{
    QString text = QGuiApplication::clipboard()->text();
    do {
        if (text.isEmpty())
            break;
        QStringList textList = text.split(QChar('\n'));
        QString lastStr = m_textList.at(m_currColumn);
        QString lstr = lastStr.left(m_postion);
        QString rstr = lastStr.right(lastStr.length() - m_postion);
        if (textList.size() == 1) {
            m_textList[m_currColumn] = lstr + textList.at(0) + rstr;
            m_postion += textList.at(0).length();
        } else {
            for (int i = 0; i < textList.size(); ++i) {
                if (i == 0)
                    m_textList[m_currColumn] = lstr + textList.at(i);
                else if (i == textList.size() - 1) {
                    m_textList.insert(m_currColumn + i, textList.at(i) + rstr);
                    m_postion = textList.at(i).length();
                } else {
                    m_textList.insert(m_currColumn + i, textList.at(i));
                }
            }
            m_cols += textList.size() - 1;
            m_currColumn += textList.size() - 1;
        }
    }while(0);
    m_repaint = true;
}

qreal CGraphicsEdit::getColWidth() const
{
    QFontMetricsF m(m_font);
    return m.height() > m.maxWidth() ? m.height() : m.maxWidth();
}

QString CGraphicsEdit::getSelectedText() const
{
    QString s;
    do {
        if (!m_selectedRegion->selected())
            break;
        const int startCol = m_selectedRegion->startCol();
        const int endCol = m_selectedRegion->endCol();
        const int startPos = m_selectedRegion->startPos();
        const int endPos = m_selectedRegion->endPos();
        if (startCol != endCol) {
            for (int i = startCol; i <= endCol; ++i) {
                QString str = m_textList.at(i);
                if (i == startCol) {
                    s += str.right(str.length() - startPos);
                    s.append(QChar('\n'));
                }
                else if (i == endCol)
                    s += str.left(endPos);
                else {
                    s += str;
                    s.append(QChar('\n'));
                }
            }
        } else {
            QString str = m_textList.at(startCol);
            s = str.right(str.length() - startPos).left(endPos - startPos);
        }

    }while(0);
    return s;
}

void CGraphicsEdit::deleteSelectText()
{
    if (m_selectedRegion->selected()) {
        QUndoCommand* command = new CTextChanged(this, m_textList, m_postion, m_cols, m_currColumn);
        m_undoStack->push(command);

        const int startCol = m_selectedRegion->startCol();
        int endCol = m_selectedRegion->endCol();
        int startPos = m_selectedRegion->startPos();
        int endPos = m_selectedRegion->endPos();
        if (startCol == endCol && startPos > endPos) qSwap(startPos, endPos);
        QString headStr, tailStr;
        if (startCol != endCol) {
            headStr = m_textList.at(startCol).left(startPos);
            int l = m_textList.at(endCol).length();
            tailStr = m_textList.at(endCol).right(l - endPos);
        }
        for (int i = startCol; i <= endCol; ++i) {
            QString s = m_textList.at(i);
            int ep = (i != endCol ? s.length() : endPos);
            int bp = (i != startCol ? 0 : startPos);
            if (startCol != endCol) {
                if ((ep == s.length() && bp == 0) || i == endCol) {
                    if (i == endCol) {
                        m_textList[startCol] = headStr + tailStr;
                    }
                    m_textList.removeAt(i);
                    endCol--;
                    m_cols--;
                    i--;
                }
            } else {
                s.remove(bp, ep - bp);
                m_textList[i] = s;
            }
        }

        if (m_textList.size() == 0) {
            m_textList << QString("");
            m_cols = 1;
        }
        m_postion = startPos;
        m_currColumn = startCol;
        m_selectedRegion->clean();
    }
}

void CGraphicsEdit::updateSelectedText(int beginCol, int endCol, int beginPos, int endPos)
{
    m_selectedRegion->setRegion(beginCol, endCol, beginPos, endPos);
    m_currColumn = endCol;
    m_postion = endPos;
    update();
}

QString CGraphicsEdit::text() const
{
    QString s;
    for (int i = 0; i < m_textList.size(); i++) {
        s += m_textList.at(i);
        if (i != m_textList.size() - 1) {
            s.append(QChar('\n'));
        }
    }
    return s;
}

void CGraphicsEdit::setText(const QString& text)
{
    do{
        if (text.isEmpty())
            break;
        //clean
        m_cols = 1;
        m_currColumn = 0;
        m_postion = 0;
        //set
        QStringList textList = text.split(QChar('\n'));
        m_textList.swap(textList);
        m_cols += textList.size() - 1;
        m_currColumn += textList.size() - 1;
        m_postion = textList.at(m_currColumn).length();
        m_repaint = true;
    } while(0);
}

void CGraphicsEdit::updateData(const QStringList& sl, int cols, int pos, int currCol)
{
    QStringList nl = sl;
    if (nl.size() == 0) {
        nl << QString("");
        m_textList.swap(nl);
        m_cols = 1;
        m_postion = 0;
        m_currColumn = 0;
    } else {
        m_textList.swap(nl);
        m_cols = m_textList.size();
        m_postion = pos;
        m_currColumn = currCol;
    }
    m_repaint = true;
    update();
}

void CGraphicsEdit::setAlignment(TextDirection d)
{
    m_direction = d;
    m_repaint = true;
    update();
}

qreal CGraphicsEdit::getStrHeight(const QString& str) const
{
    QFontMetricsF m(m_font);
    qreal h = 0;
    foreach(QChar c, str) {
        if (c < 128) {
            h += m.width(c);
        } else {
            h += m.height();
        }
    }
    return h;
}
