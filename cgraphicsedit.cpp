#include "cgraphicsedit.h"
#include <QTimer>
#include <QFontMetrics>
#include <QPainter>
#include <QKeyEvent>
#include <QEvent>
#include <QGuiApplication>
#include <QGraphicsScene>
#include <QGraphicsSceneEvent>
#include <QUndoCommand>
#include <QTextCursor>
#include <QMutex>
#include <QDebug>

static const qreal DEFAULT_EDIT_SIZE = 300;

class CTextChanged : public QUndoCommand
{
public:
    CTextChanged(CGraphicsEdit* item, const QStringList& sl, int pos, int cols, int curCol,
                 const QList<QList<SCharFormat>>& sf):
        m_d(item),
        m_textList(sl),
        m_postion(pos),
        m_cols(cols),
        m_currColumn(curCol),
        m_sf(sf)
    {

    }

    void undo() override {
        if (m_d) {
            m_d->updateData(m_textList, m_cols, m_postion, m_currColumn, m_sf);
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
    QList<QList<SCharFormat>> m_sf;
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
    m_showCursor(false),
    m_postion(0),
    m_cols(1),
    m_currColumn(0),
    m_selectedRegion(new SelectedRegion),
    m_undoStack(new QUndoStack(this)),
    m_alignment(AlignmentTop),
    m_oriection(TextVertical)
{   
    setAcceptDrops(true);
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    //😂setFlag(ItemIsFocusable);
    //setFocus();
    //初始化文字列表
    m_textList << QString("");
    QList<SCharFormat> sf;
    m_charFormats << sf;
    //光标闪烁
    m_timer = new QTimer(this);
    m_timer->setInterval(500);
    connect(m_timer, &QTimer::timeout, this, &CGraphicsEdit::onTimeout);
    m_textItem = new QGraphicsTextItem();
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
    const qreal adjust = 5.0;
    qreal maxSize = DEFAULT_EDIT_SIZE;
    for(int i = 0; i < m_textList.size(); ++i) {
        qreal size;
        if (m_oriection == TextVertical) {
            size = getStrHeight(i);
        } else {
            size = getStrWidth(i);
        }

        if (size > maxSize) {
            maxSize = size;
        }
    }

    QRectF r;
    if (m_oriection == TextVertical) {
        qreal w = getColXPostion(m_cols - 1);
        r =  QRectF(-w/2 - adjust, -maxSize/2 - adjust, w + 2*adjust, maxSize + 2*adjust);
    } else {
        qreal h = getRowYPostion(m_cols - 1);
        r = QRectF(-maxSize/2 - adjust, -h/2 - adjust, maxSize + 2*adjust, h + 2*adjust);
    }
    return r;
}

void CGraphicsEdit::verticalPaint(QPainter* painter, const QRectF& r)
{
    const qreal adjust = 5.0;
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
            const qreal colx = getColXPostion(i);
            const qreal cw = getColWidth(i);
            int ep = (i != endCol ? s.length() : endPos);
            int bp = (i != startCol ? 0 : startPos);
            if (ep == bp) continue;
            ep -= 1;

            qreal sy = 0;
            if (m_alignment == AlignmentTop) {
                sy = r.top() + adjust;
            } else if (m_alignment == AlignmentCenter) {
                qreal textHeight = getStrHeight(i);
                sy = -textHeight/2 - adjust/2;
            } else {
                qreal textHeight = getStrHeight(i);
                sy = r.bottom() - adjust - textHeight;
            }

            qreal sx = r.right() - colx - adjust;
            qreal starty = sy;
            qreal endy = starty;
            for (int j = 0; j < s.length(); j++) {
                SCharFormat sf = m_charFormats.at(i).at(j);
                QFont font;
                sf.setFont(&font);
                QFontMetricsF m(font);
                if (j == bp) {
                    starty = sy;
                }
                QChar c = s.at(j);
                if (c < 128) {
                    sy += m.width(c);
                } else {
                    sy += m.height();
                }
                sy += sf.letterSpacing;
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
    int index = 0;
    qreal hx, vy;
    qreal cursorX = r.right() - adjust - getColXPostion(m_currColumn);
    qreal cursorY = r.top() + adjust;
    if (m_alignment == AlignmentCenter) {
        qreal textHeight = getStrHeight(m_currColumn);
        cursorY = -textHeight/2 - adjust;
    } else if (m_alignment == AlignmentBottom) {
        qreal textHeight = getStrHeight(m_currColumn);
        cursorY = r.bottom() - adjust - textHeight;
    }

    for (int i = 0; i < m_textList.size(); i++) {
        QString l = m_textList.at(i);
        qreal vx = 0;
        if (m_alignment == AlignmentTop) {
            vx = r.top() + adjust;
        } else if (m_alignment == AlignmentCenter) {
            qreal textHeight = getStrHeight(i);
            vx = -textHeight/2 - adjust/2;
        } else {
            qreal textHeight = getStrHeight(i);
            vx = r.bottom() - adjust - textHeight;
        }
        bool isCurrLine = (index == m_currColumn);
        int cursorIndex = 0;
        qreal underBeginY = vx, underEndY = vx, overBeginY = vx, overEndY = vx, strikeBeginY = vx, strikeEndY = vx;
        for(int j = 0; j < l.size(); ++j) {
            QChar c = l.at(j);
            QPen  textPen;
            if (m_selectedRegion->selected() && index >= startCol && index <= endCol) {
                if (startCol == endCol) {
                    if (cursorIndex >= startPos &&
                        cursorIndex < endPos) {
                        textPen.setColor(QColor("#FFF8F0"));
                    } else {
                        textPen.setColor(m_charFormats.at(i).at(j).fontColor);
                    }
                } else {
                    if (index == startCol)
                        if (cursorIndex >= startPos)
                            textPen.setColor(QColor("#FFF8F0"));
                        else
                            textPen.setColor(m_charFormats.at(i).at(j).fontColor);
                    else if (index == endCol)
                        if (cursorIndex < endPos)
                            textPen.setColor(QColor("#FFF8F0"));
                        else
                            textPen.setColor(m_charFormats.at(i).at(j).fontColor);
                    else
                        textPen.setColor(QColor("#FFF8F0"));
                }
            } else {
                textPen.setColor(m_charFormats.at(i).at(j).fontColor);
            }
            painter->setPen(textPen);
            QFont font;
            m_charFormats.at(i).at(j).setFont(&font);
            bool underline = font.underline();
            bool overline = font.overline();
            bool strikeout = font.strikeOut();
            //把删除线，上下划线属性去除，效果不好，自定义实现
            font.setUnderline(false);
            font.setStrikeOut(false);
            font.setOverline(false);
            painter->setFont(font);
            underBeginY = vx;
            overBeginY = vx;
            strikeBeginY = vx;
            const qreal colx = getColXPostion(i);
            const qreal cw = getColWidth(i);
            QFontMetricsF m(font);
            if (c < 128) {
                painter->rotate(90);
                qreal w = m.width(c);
                vy = -r.right() + colx - cw/4 + adjust;
                painter->drawText(QPointF(vx, vy), c);
                vx += w + font.letterSpacing();
                painter->rotate(-90);
            } else {
                qreal w = m.width(c);
                hx = r.right() - colx + (cw - w)/2 - adjust;
                painter->drawText(QPointF(hx, vx + m.ascent()), c);
                vx += m.height() + font.letterSpacing();
            }

            //左划线
            if (underline) {
                underEndY = vx;
                if (underEndY != underBeginY) {
                    painter->drawLine(r.right() - colx - adjust, underBeginY,
                                      r.right() - colx - adjust, underEndY);
                }
            }
            //右划线
            if (overline) {
                overEndY = vx;
                if (overEndY != overBeginY) {
                    qreal linex = r.right() - adjust;
                    if (i > 0) {
                        linex -= getColXPostion(i - 1);
                    }
                    painter->drawLine(linex, overBeginY, linex, overEndY);
                }
            }
            //删除线.
            if (strikeout) {
                strikeEndY = vx;
                if (strikeEndY != strikeBeginY) {
                    painter->drawLine(r.right() - colx + cw/2 - adjust, strikeBeginY,
                                      r.right() - colx + cw/2 - adjust, strikeEndY);
                }
            }

            //光标y坐标
            if (isCurrLine && ((cursorIndex + 1) == m_postion)) {
                cursorY = vx;
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
    painter->drawLine(QPointF(cursorX, cursorY), QPointF(cursorX + getColWidth(m_currColumn), cursorY));
    painter->restore();
}

void CGraphicsEdit::horizontalPaint(QPainter* painter, const QRectF& r)
{
    const qreal adjust = 5.0;
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
            const qreal rowy = (i == 0 ? 0 : getRowYPostion(i - 1));
            const qreal rh = getRowHeight(i);
            int ep = (i != endCol ? s.length() : endPos);
            int bp = (i != startCol ? 0 : startPos);
            if (ep == bp) continue;
            ep -= 1;

            qreal sx = 0;
            if (m_alignment == AlignmentRight) {
                qreal textWidth = getStrWidth(i);
                sx = r.right() - adjust - textWidth;
            } else if (m_alignment == AlignmentHCenter) {
                qreal textWidth = getStrWidth(i);
                sx = r.left() + adjust + textWidth/2;
            } else {
                sx = r.left() + adjust;
            }

            qreal sy = r.top() + rowy + adjust;
            qreal startx = sx, endx = sx;
            for (int j = 0; j < s.length(); j++) {
                SCharFormat sf = m_charFormats.at(i).at(j);
                QFont font;
                sf.setFont(&font);
                QFontMetricsF m(font);
                if (j == bp) {
                    startx = sx;
                }
                QChar c = s.at(j);
                sx += m.width(c) + sf.letterSpacing;
                if (j == ep) {
                    endx = sx;
                }
            }

            if (startx != endx) {
                QPen pen;
                pen.setWidth(0);
                painter->setPen(pen);
                painter->setBrush(QColor("#0078D7"));
                painter->drawRect(QRectF(startx, sy, endx - startx, rh));
            }
        }
        painter->restore();
    }
    //绘制文字
    painter->save();
    qreal cursory = r.top() + adjust + getRowYPostion(m_currColumn);
    qreal cursorx = r.left() + adjust;
    if (m_alignment == AlignmentRight) {
        qreal textWidth = getStrWidth(m_currColumn);
        cursorx = r.right() - adjust - textWidth;
    } else if (m_alignment == AlignmentHCenter) {
        qreal textWidth = getStrWidth(m_currColumn);
        cursorx = r.left() + adjust + textWidth/2;
    }

    for (int i = 0; i < m_textList.size(); i++) {
        QString l = m_textList.at(i);
        qreal textx = r.left() + adjust;
        const qreal texty = r.top() + getRowYPostion(i);
        if (m_alignment == AlignmentRight) {
            qreal textWidth = getStrWidth(i);
            textx = r.right() - adjust - textWidth;
        } else if (m_alignment == AlignmentHCenter) {
            qreal textWidth = getStrWidth(m_currColumn);
            textx = r.left() + adjust + textWidth/2;
        }

        bool isCurrLine = (i == m_currColumn);
        for(int j = 0; j < l.size(); ++j) {
            QChar c = l.at(j);
            QPen  textPen;
            if (m_selectedRegion->selected() && i >= startCol && i <= endCol) {
                if (startCol == endCol) {
                    if (j >= startPos && j < endPos) {
                        textPen.setColor(QColor("#FFF8F0"));
                    } else {
                        textPen.setColor(m_charFormats.at(i).at(j).fontColor);
                    }
                } else {
                    if (i == startCol)
                        if (j >= startPos)
                            textPen.setColor(QColor("#FFF8F0"));
                        else
                            textPen.setColor(m_charFormats.at(i).at(j).fontColor);
                    else if (i == endCol)
                        if (j < endPos)
                            textPen.setColor(QColor("#FFF8F0"));
                        else
                            textPen.setColor(m_charFormats.at(i).at(j).fontColor);
                    else
                        textPen.setColor(QColor("#FFF8F0"));
                }
            } else {
                textPen.setColor(m_charFormats.at(i).at(j).fontColor);
            }
            painter->setPen(textPen);
            QFont font;
            m_charFormats.at(i).at(j).setFont(&font);
            painter->setFont(font);
            QFontMetricsF m(font);

            painter->drawText(QPointF(textx, texty - m.descent()), c);
            textx += m.width(c) + font.letterSpacing();

            //光标x坐标
            if (isCurrLine && ((j + 1) == m_postion)) {
                cursorx = textx;
            }
        }
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
    painter->drawLine(QPointF(cursorx, cursory), QPointF(cursorx, cursory - getRowHeight(m_currColumn)));
    painter->restore();
}

void CGraphicsEdit::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget )
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    const QRectF r = boundingRect();
    //绘制虚线框
    QPen pen;
    pen.setColor(Qt::black);
    pen.setStyle(Qt::DotLine);
    painter->save();
    painter->setPen(pen);
    painter->drawRect(r);
    painter->restore();
    if (m_oriection == TextVertical) {
        verticalPaint(painter, r);
    } else {
        horizontalPaint(painter, r);
    }
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
    if (e->key() == Qt::Key_Backspace && !(e->modifiers() & ~Qt::ShiftModifier)) {
        do {
            if (m_selectedRegion->selected()) {
                deleteSelectText();
                break;
            }

            if (m_currColumn == 0 && m_postion == 0)
                break;

            QUndoCommand* command = new CTextChanged(this, m_textList, m_postion, m_cols, m_currColumn, m_charFormats);
            m_undoStack->push(command);

            QString l = m_textList.at(m_currColumn);
            QList<SCharFormat> sf = m_charFormats.at(m_currColumn);
            if (m_postion == 0) {
                if (m_cols > 1) {
                    m_textList.removeAt(m_currColumn);
                    m_charFormats.removeAt(m_currColumn);
                }
                m_cols--;
                m_currColumn--;
                QString lstr = m_textList.at(m_currColumn);
                QList<SCharFormat> lsf = m_charFormats.at(m_currColumn);
                m_postion = lstr.length();
                m_textList[m_currColumn] = lstr + l;
                lsf.append(sf);
                m_charFormats[m_currColumn].swap(lsf);
            } else {
                l.remove(m_postion - 1, 1);
                sf.erase(sf.begin() + (m_postion - 1), sf.begin() + m_postion);
                m_textList[m_currColumn] = l;
                m_charFormats[m_currColumn].swap(sf);
                m_postion--;
            }
        } while (0);
        scene()->update();
        goto accept;
    } else if (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return) {
        deleteSelectText();
        QString l = m_textList.at(m_currColumn);
        QString rStr = l.right(l.length() - m_postion);
        QString lStr = l.left(m_postion);
        QList<SCharFormat>& sf = m_charFormats[m_currColumn];
        QList<SCharFormat> rsf;
        for (int i = 0; i < sf.size(); ++i) {
            if (i >= m_postion) {
                rsf.push_back(sf.at(i));
            }
        }
        sf.erase(sf.begin() + m_postion, sf.end());
        m_textList[m_currColumn] = lStr;
        m_cols++;
        m_currColumn++;
        m_textList.insert(m_currColumn, rStr);
        m_charFormats.insert(m_currColumn, rsf);
        m_postion = 0;
        scene()->update();
        goto accept;
    } else if (e->key() == Qt::Key_Delete) {
        do {
            if (m_selectedRegion->selected()) {
                deleteSelectText();
                break;
            }

            QUndoCommand* command = new CTextChanged(this, m_textList, m_postion, m_cols, m_currColumn, m_charFormats);
            m_undoStack->push(command);

            QString s = m_textList.at(m_currColumn);
            QList<SCharFormat> sf = m_charFormats.at(m_currColumn);
            if ((m_currColumn == 0 && s.isEmpty() && m_cols == 1) ||
                (m_currColumn + 1 == m_cols && s.length() == m_postion))
                break;
            if (m_postion == s.length()) {
                QString ls = m_textList.at(m_currColumn + 1);
                QList<SCharFormat> lsf = m_charFormats.at(m_currColumn + 1);
                m_textList[m_currColumn] = s + ls;
                sf.append(lsf);
                m_charFormats[m_currColumn].swap(sf);
                m_textList.removeAt(m_currColumn + 1);
                m_charFormats.removeAt(m_currColumn + 1);
                m_cols--;
            } else {
                s.remove(m_postion, 1);
                sf.erase(sf.begin() + m_postion, sf.begin() + m_postion + 1);
                m_textList[m_currColumn] = s;
                m_charFormats[m_currColumn].swap(sf);
            }
        }while (0);
        scene()->update();
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
            if (m_oriection == TextVertical) {
                if (m_currColumn + 1 >= m_cols)
                    break;
                QString ls = m_textList.at(m_currColumn + 1);
                if (ls.length() < m_postion) {
                    m_postion = ls.length();
                }
                m_currColumn++;
            } else {
                if (m_postion == 0 && m_currColumn == 0)
                    break;
                if (m_postion == 0) {
                    m_currColumn--;
                    m_postion = m_textList.at(m_currColumn).length();
                } else {
                    m_postion--;
                }
            }
        }while(0);
        m_selectedRegion->clean();
        goto accept;
    } else if (e->key() == Qt::Key_Right) {
        do {
            if (m_oriection == TextVertical) {
                if (m_currColumn == 0)
                    break;
                QString rs = m_textList.at(m_currColumn - 1);
                if (m_postion > rs.length()) {
                    m_postion = rs.length();
                }
                m_currColumn--;
            } else {
                QString s = m_textList.at(m_currColumn);
                if (m_postion == s.length() && m_currColumn + 1 == m_cols)
                    break;
                if (m_postion == s.length()) {
                    m_currColumn++;
                    m_postion = 0;
                } else {
                    m_postion++;
                }
            }

        }while(0);
        m_selectedRegion->clean();
        goto accept;
    } else if (e->key() == Qt::Key_Up) {
        do {
            if (m_oriection == TextVertical) {
                if (m_postion == 0 && m_currColumn == 0)
                    break;
                if (m_postion == 0) {
                    m_currColumn--;
                    m_postion = m_textList.at(m_currColumn).length();
                } else {
                    m_postion--;
                }
            } else {
                if (m_currColumn == 0)
                    break;
                QString rs = m_textList.at(m_currColumn - 1);
                if (m_postion > rs.length()) {
                    m_postion = rs.length();
                }
                m_currColumn--;
            }

        } while(0);
        m_selectedRegion->clean();
        goto accept;
    }else if (e->key() == Qt::Key_Down) {
        do {
            if (m_oriection == TextVertical) {
                QString s = m_textList.at(m_currColumn);
                if (m_postion == s.length() && m_currColumn + 1 == m_cols)
                    break;
                if (m_postion == s.length()) {
                    m_currColumn++;
                    m_postion = 0;
                } else {
                    m_postion++;
                }
            } else {
                if (m_currColumn + 1 >= m_cols)
                    break;
                QString ls = m_textList.at(m_currColumn + 1);
                if (ls.length() < m_postion) {
                    m_postion = ls.length();
                }
                m_currColumn++;
            }

        }while(0);
        m_selectedRegion->clean();
        goto accept;
    }
    else {
        if (e->modifiers() & Qt::ControlModifier) {
            e->ignore();
            return;
        }

        if (e->modifiers() & Qt::ShiftModifier && e->key() == Qt::Key_Shift) {
            e->ignore();
            return;
        }

        QUndoCommand* command = new CTextChanged(this, m_textList, m_postion, m_cols, m_currColumn, m_charFormats);
        m_undoStack->push(command);
        //format
        for (int i = 0; i < e->text().length(); ++i) {
            m_charFormats[m_currColumn].insert(m_postion + i, m_textFormat);
        }
        //text
        QString currText = m_textList.at(m_currColumn);
        currText.insert(m_postion, e->text());
        m_textList[m_currColumn] = currText;

        m_postion += e->text().length();
        goto accept;
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
        QUndoCommand* command = new CTextChanged(this, m_textList, m_postion, m_cols, m_currColumn, m_charFormats);
        m_undoStack->push(command);

        //format
        for (int i = 0; i < event->commitString().length(); ++i) {
            m_charFormats[m_currColumn].insert(m_postion + i, m_textFormat);
        }
        //text
        QString currText = m_textList.at(m_currColumn);
        currText.insert(m_postion, event->commitString());
        m_textList[m_currColumn] = currText;
        m_postion += event->commitString().length();
        scene()->update();
    }
}

void CGraphicsEdit::onTimeout()
{
    m_showCursor = !m_showCursor;
    update();
}

bool CGraphicsEdit::sceneEvent(QEvent *event)
{
    qDebug() << "sceneEvent: " << event->type();
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
            //QGuiApplication::inputMethod()->reset();
            focusEvent(static_cast<QFocusEvent *>(event));
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
        case QEvent::InputMethod:
            inputMethodEvent(static_cast<QInputMethodEvent *>(e));
            break;
        case QEvent::FocusIn:
        case QEvent::FocusOut:
            focusEvent(static_cast<QFocusEvent *>(e));
            break;
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
    if (e->type() == QFocusEvent::FocusOut) {
        m_timer->stop();
        m_showCursor = false;
        setTextInteractionFlags(Qt::NoTextInteraction);
    }
}

void CGraphicsEdit::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    do {
        if (interactionFlags == Qt::NoTextInteraction || event->button() != Qt::LeftButton)
            break;
        m_mousePressed = true;
        m_timer->stop();
        QPointF p = event->pos();
        const QRectF r = boundingRect();
        const qreal adjust = 5.0;
        qDebug() << "boundingRECT:" << r;
        for (int i = 0; i < m_cols; i++) {
            if (m_oriection == TextVertical) {
                qreal rx = r.right() - adjust - (i == 0 ? 0 : getColXPostion(i - 1));
                qreal lx = r.right() - adjust - getColXPostion(i);
                if (p.x() <= rx && p.x() > lx) {
                    m_currColumn = i;
                    //计算y坐标位置
                    qreal y = r.top() + adjust;
                    QString s = m_textList.at(i);
                    if (m_alignment == AlignmentCenter) {
                        qreal textHeight = getStrHeight(i);
                        y = -textHeight/2;
                    } else if (m_alignment == AlignmentBottom) {
                        qreal textHeight = getStrHeight(i);
                        y = r.bottom() - adjust - textHeight;
                    }

                    if (s.isEmpty() || p.y() < y) {
                        m_postion = 0;
                    } else {
                        for (int n = 0; n < s.length(); ++n) {
                            qreal prevY = y;
                            SCharFormat sf = m_charFormats.at(i).at(n);
                            QFont font;
                            sf.setFont(&font);
                            QFontMetricsF m(font);
                            QChar c = s.at(n);
                            if (c < 128) {
                                y += m.width(c);
                            } else {
                                y += m.height();
                            }
                            y += sf.letterSpacing;
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
            } else {
                qreal ty = r.top() - adjust + (i == 0 ? 0 : getRowYPostion(i - 1));
                qreal by = r.top() - adjust + getRowYPostion(i);
                if (p.y() >= ty && p.y() < by) {
                    m_currColumn = i;
                    //计算x坐标位置
                    qreal x = r.left() + adjust;
                    QString s = m_textList.at(i);
                    if (m_alignment == AlignmentRight) {
                        qreal textWidth = getStrWidth(i);
                        x = r.right() - adjust - textWidth;
                    } else if (m_alignment == AlignmentHCenter) {
                        qreal textWidth = getStrWidth(i);
                        x = r.left() + adjust + textWidth/2;
                    }

                    if (s.isEmpty() || p.x() < x) {
                        m_postion = 0;
                    } else {
                        for (int n = 0; n < s.length(); ++n) {
                            qreal prevx = x;
                            SCharFormat sf = m_charFormats.at(i).at(n);
                            QFont font;
                            sf.setFont(&font);
                            QFontMetricsF m(font);
                            QChar c = s.at(n);
                            x += m.width(c) + sf.letterSpacing;
                            if (p.x() >= prevx && p.x() < x) {
                                m_postion = n;
                                break;
                            }
                        }
                        if (s.length() > 0 && p.x() > x) {
                            m_postion = s.length();
                        }
                    }
                    break;
                }
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
        const qreal adjust = 5.0;
        for (int i = 0; i < m_cols; i++) {
            if (m_oriection == TextVertical) {
                qreal rx = r.right() - adjust - (i == 0 ? 0 : getColXPostion(i - 1));
                qreal lx = r.right() - adjust - getColXPostion(i);
                if (p.x() <= rx && p.x() > lx) {
                    m_currColumn = i;
                    //计算y坐标位置
                    qreal y = r.top() + adjust;
                    QString s = m_textList.at(i);
                    if (m_alignment == AlignmentCenter) {
                        qreal textHeight = getStrHeight(i);
                        y = -textHeight/2;
                    } else if (m_alignment == AlignmentBottom) {
                        qreal textHeight = getStrHeight(i);
                        y = r.bottom() - adjust - textHeight;
                    }

                    if (s.isEmpty() || p.y() < y) {
                        m_postion = 0;
                    } else {
                        for (int n = 0; n < s.length(); ++n) {
                            qreal prevY = y;
                            SCharFormat sf = m_charFormats.at(i).at(n);
                            QFont font;
                            sf.setFont(&font);
                            QFontMetricsF m(font);
                            QChar c = s.at(n);
                            if (c < 128) {
                                y += m.width(c);
                            } else {
                                y += m.height();
                            }
                            y += sf.letterSpacing;
                            if (p.y() >= prevY && p.y() < y) {
                                m_postion = n;
                                break;
                            }
                        }
                        if (s.length() > 0 && p.y() > y) {
                            m_postion = s.length();
                        }
                    }
                    break;
                }
            } else {
                qreal ty = r.top() - adjust + (i == 0 ? 0 : getRowYPostion(i - 1));
                qreal by = r.top() - adjust + getRowYPostion(i);
                if (p.y() >= ty && p.y() < by) {
                    m_currColumn = i;
                    //计算x坐标位置
                    qreal x = r.left() + adjust;
                    QString s = m_textList.at(i);
                    if (m_alignment == AlignmentRight) {
                        qreal textWidth = getStrWidth(i);
                        x = r.right() - adjust - textWidth;
                    } else if (m_alignment == AlignmentHCenter) {
                        qreal textWidth = getStrWidth(i);
                        x = r.left() + adjust + textWidth/2;
                    }

                    if (s.isEmpty() || p.x() < x) {
                        m_postion = 0;
                    } else {
                        for (int n = 0; n < s.length(); ++n) {
                            qreal prevx = x;
                            SCharFormat sf = m_charFormats.at(i).at(n);
                            QFont font;
                            sf.setFont(&font);
                            QFontMetricsF m(font);
                            QChar c = s.at(n);
                            x += m.width(c) + sf.letterSpacing;
                            if (p.x() >= prevx && p.x() < x) {
                                m_postion = n + 1;
                                break;
                            }
                        }
                        if (s.length() > 0 && p.x() > x) {
                            m_postion = s.length();
                        }
                    }
                    break;
                }
            }
        }
        m_selectedRegion->setEndCol(m_currColumn);
        m_selectedRegion->setEndPos(m_postion);
        update();
    }
}

void CGraphicsEdit::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_mousePressed) {
        m_timer->start();
        m_mousePressed = false;
    }
}

void CGraphicsEdit::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (interactionFlags == Qt::TextEditorInteraction) {
        QPointF p = event->pos();
        const QRectF r = boundingRect();
        const qreal adjust = 5.0;
        for (int i = 0; i < m_cols; i++) {
            qreal rx = r.right() - adjust - (i == 0 ? 0 : getColXPostion(i - 1));
            qreal lx = r.right() - adjust - getColXPostion(i);
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
    } else {
        setTextInteractionFlags(Qt::TextEditorInteraction);
        m_postion = 0;
        m_currColumn = 0;
        m_timer->start();
        setFocus();
    }
}

void CGraphicsEdit::setTextInteractionFlags(Qt::TextInteractionFlags flags)
{
    if (flags == Qt::NoTextInteraction)
        setFlags(this->flags() & ~(QGraphicsItem::ItemIsFocusable | QGraphicsItem::ItemAcceptsInputMethod));
    else {
        setFlags(this->flags() | QGraphicsItem::ItemIsFocusable | QGraphicsItem::ItemAcceptsInputMethod);
    }
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
    scene()->update();
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
    scene()->update();
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
        QUndoCommand* command = new CTextChanged(this, m_textList, m_postion, m_cols, m_currColumn, m_charFormats);
        m_undoStack->push(command);

        const int startCol = m_selectedRegion->startCol();
        int endCol = m_selectedRegion->endCol();
        int startPos = m_selectedRegion->startPos();
        int endPos = m_selectedRegion->endPos();
        if (startCol == endCol && startPos > endPos) qSwap(startPos, endPos);
        QString headStr, tailStr;
        QList<SCharFormat> headFormats, tailFormats;
        if (startCol != endCol) {
            headStr = m_textList.at(startCol).left(startPos);
            int l = m_textList.at(endCol).length();
            tailStr = m_textList.at(endCol).right(l - endPos);
            for (int i = 0; i < startPos; i++) {
                headFormats.push_back(m_charFormats.at(startCol).at(i));
            }

            for (int i = endPos; i < m_charFormats.at(endCol).size(); i++) {
                tailFormats.push_back(m_charFormats.at(endCol).at(i));
            }
        }
        for (int i = startCol; i <= endCol; ++i) {
            QString s = m_textList.at(i);
            QList<SCharFormat> sf = m_charFormats.at(i);
            int ep = (i != endCol ? s.length() : endPos);
            int bp = (i != startCol ? 0 : startPos);
            if (startCol != endCol) {
                if ((ep == s.length() && bp == 0) || i == endCol) {
                    if (i == endCol) {
                        m_textList[startCol] = headStr + tailStr;
                        headFormats.append(tailFormats);
                        m_charFormats[startCol] = headFormats;
                    }
                    m_textList.removeAt(i);
                    m_charFormats.removeAt(i);
                    endCol--;
                    m_cols--;
                    i--;
                }
            } else {
                s.remove(bp, ep - bp);
                m_textList[i] = s;
                sf.erase(sf.begin() + bp, sf.begin() + ep);
                m_charFormats[i].swap(sf);
            }
        }

        if (m_textList.size() == 0) {
            m_textList << QString("");
            QList<SCharFormat> sf;
            m_charFormats << sf;
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

void CGraphicsEdit::updateData(const QStringList& sl, int cols, int pos, int currCol, const QList<QList<SCharFormat>>& sf)
{
    QStringList nl = sl;
    if (nl.size() == 0) {
        nl << QString("");
        m_textList.swap(nl);
        QList<SCharFormat> lsf;
        m_charFormats.erase(m_charFormats.begin(), m_charFormats.end());
        m_charFormats << lsf;
        m_cols = 1;
        m_postion = 0;
        m_currColumn = 0;
    } else {
        m_textList.swap(nl);
        m_cols = m_textList.size();
        m_postion = pos;
        m_currColumn = currCol;
        m_charFormats.erase(m_charFormats.begin(), m_charFormats.end());
        for (int i = 0; i < sf.size(); i++) {
            QList<SCharFormat> lsf;
            for(int j = 0; j < sf.at(i).size(); j++) {
                lsf.push_back(sf.at(i).at(j));
            }
            m_charFormats << lsf;
        }
    }
    scene()->update();
}

void CGraphicsEdit::setAlignment(TextAlignment d)
{
    if (m_oriection == TextVertical) {
        if (d > AlignmentBottom)
            return;
    } else {
        if (d < AlignmentLeft)
            return;
    }
    m_alignment = d;
    scene()->update();
}

qreal CGraphicsEdit::getStrHeight(int index) const
{
    QString str = m_textList.at(index);
    qreal h = 0;
    for (int i = 0; i < str.length(); ++i) {
        SCharFormat f = m_charFormats.at(index).at(i);
        QFont font;
        f.setFont(&font);
        QFontMetricsF m(font);
        if (str[i] < 128) {
            h += m.width(str[i]);
        } else {
            h += m.height();
        }
        if (i != str.length() - 1) {
            h += f.letterSpacing;
        }
    }
    return h;
}

qreal CGraphicsEdit::getStrWidth(int index) const
{
    QString str = m_textList.at(index);
    qreal w = 0;
    for (int i = 0; i < str.length(); ++i) {
        SCharFormat f = m_charFormats.at(index).at(i);
        QFont font;
        f.setFont(&font);
        QFontMetricsF m(font);
        w += m.width(str[i]);
        if (i != str.length() - 1) {
            w += f.letterSpacing;
        }
    }
    return w;
}

qreal CGraphicsEdit::getColWidth(int index) const
{
    qreal colWidth = 0;
    QString s = m_textList.at(index);
    if (s.isEmpty()) {
        QFont font;
        m_textFormat.setFont(&font);
        QFontMetricsF m(font);
        colWidth = m.height() > m.maxWidth() ? m.height() : m.maxWidth();
    } else {
        for (int i = 0; i < s.size(); ++i) {
            SCharFormat f = m_charFormats.at(index).at(i);
            QFont font;
            f.setFont(&font);
            QFontMetricsF m(font);
            qreal w = m.height() > m.maxWidth() ? m.height() : m.maxWidth();
            if (w > colWidth) {
                colWidth = w;
            }
        }
    }
    return colWidth;
}

qreal CGraphicsEdit::getColXPostion(int index) const
{
    qreal x = 0;
    for (int i = 0; i <= index; ++i) {
        QString s = m_textList.at(i);
        if (s.isEmpty()) {
            QFont font;
            m_textFormat.setFont(&font);
            QFontMetricsF m(font);
            qreal colWidth = m.height() > m.maxWidth() ? m.height() : m.maxWidth();
            x += colWidth;
        } else {
            qreal colWidth = 0;
            int size = s.size() > m_charFormats.at(i).size() ? m_charFormats.at(i).size() : s.size();
            for (int j = 0; j < size; ++j) {
                SCharFormat f = m_charFormats.at(i).at(j);
                QFont font;
                f.setFont(&font);
                QFontMetricsF m(font);
                qreal w = m.height() > m.maxWidth() ? m.height() : m.maxWidth();
                if (w > colWidth) {
                    colWidth = w;
                }
            }
            x += colWidth;
        }
    }

    if (m_cols > 1) {
        x += (m_cols - 1)*m_columnSpacing;
    }

    return x;
}

qreal CGraphicsEdit::getRowHeight(int index) const
{
    qreal h = 0;
    QString s = m_textList.at(index);
    if (s.isEmpty()) {
        QFont font;
        m_textFormat.setFont(&font);
        QFontMetricsF m(font);
        h = m.height();
    } else {
        int size = s.size() > m_charFormats.at(index).size() ? m_charFormats.at(index).size() : s.size();
        for (int j = 0; j < size; ++j) {
            SCharFormat f = m_charFormats.at(index).at(j);
            QFont font;
            f.setFont(&font);
            QFontMetricsF m(font);
            if (m.height() > h) {
                h = m.height();
            }
        }
    }
    return h;
}

qreal CGraphicsEdit::getRowYPostion(int index) const
{
    qreal y = 0;
    for (int i = 0; i <= index; ++i) {
        QString s = m_textList.at(i);
        if (s.isEmpty()) {
            QFont font;
            m_textFormat.setFont(&font);
            QFontMetricsF m(font);
            qreal h = m.height();
            y += h;
        } else {
            qreal h = 0;
            int size = s.size() > m_charFormats.at(i).size() ? m_charFormats.at(i).size() : s.size();
            for (int j = 0; j < size; ++j) {
                SCharFormat f = m_charFormats.at(i).at(j);
                QFont font;
                f.setFont(&font);
                QFontMetricsF m(font);
                if (m.height() > h) {
                    h = m.height();
                }
            }
            y += h;
        }
    }

    if (m_cols > 1) {
        y += (m_cols - 1)*m_columnSpacing;
    }

    return y;
}

void CGraphicsEdit::onFontChanged(const QString& text)
{
    if (!m_selectedRegion->selected()) {
        m_textFormat.fontText = text;
    } else {
        const int startCol = m_selectedRegion->startCol();
        int endCol = m_selectedRegion->endCol();
        int startPos = m_selectedRegion->startPos();
        int endPos = m_selectedRegion->endPos();
        if (startCol == endCol && startPos > endPos) qSwap(startPos, endPos);
        for (int i = startCol; i <= endCol; ++i) {
            int bp = (i == startCol ? startPos : 0);
            int ep = (i == endCol ? endPos : m_charFormats.at(i).length());
            for (int j = bp; j < ep; ++j) {
                m_charFormats[i][j].fontText = text;
            }
        }
    }
    scene()->update();
}

void CGraphicsEdit::setBold(bool enabled)
{
    if (!m_selectedRegion->selected()) {
        m_textFormat.bold = enabled;
    } else {
        const int startCol = m_selectedRegion->startCol();
        int endCol = m_selectedRegion->endCol();
        int startPos = m_selectedRegion->startPos();
        int endPos = m_selectedRegion->endPos();
        if (startCol == endCol && startPos > endPos) qSwap(startPos, endPos);
        for (int i = startCol; i <= endCol; ++i) {
            int bp = (i == startCol ? startPos : 0);
            int ep = (i == endCol ? endPos : m_charFormats.at(i).length());
            for (int j = bp; j < ep; ++j) {
                m_charFormats[i][j].bold = enabled;
            }
        }
    }
    scene()->update();
}

void CGraphicsEdit::setItalic(bool enabled)
{
    if (!m_selectedRegion->selected()) {
        m_textFormat.italic = enabled;
    } else {
        const int startCol = m_selectedRegion->startCol();
        int endCol = m_selectedRegion->endCol();
        int startPos = m_selectedRegion->startPos();
        int endPos = m_selectedRegion->endPos();
        if (startCol == endCol && startPos > endPos) qSwap(startPos, endPos);
        for (int i = startCol; i <= endCol; ++i) {
            int bp = (i == startCol ? startPos : 0);
            int ep = (i == endCol ? endPos : m_charFormats.at(i).length());
            for (int j = bp; j < ep; ++j) {
                m_charFormats[i][j].italic = enabled;
            }
        }
    }
    scene()->update();
}

void CGraphicsEdit::setOverline(bool enabled)
{
    if (!m_selectedRegion->selected()) {
        m_textFormat.overline = enabled;
    } else {
        const int startCol = m_selectedRegion->startCol();
        int endCol = m_selectedRegion->endCol();
        int startPos = m_selectedRegion->startPos();
        int endPos = m_selectedRegion->endPos();
        if (startCol == endCol && startPos > endPos) qSwap(startPos, endPos);
        for (int i = startCol; i <= endCol; ++i) {
            int bp = (i == startCol ? startPos : 0);
            int ep = (i == endCol ? endPos : m_charFormats.at(i).length());
            for (int j = bp; j < ep; ++j) {
                m_charFormats[i][j].overline = enabled;
            }
        }
    }
    scene()->update();
}

void CGraphicsEdit::setUnderline(bool enabled)
{
    if (!m_selectedRegion->selected()) {
        m_textFormat.underline = enabled;
    } else {
        const int startCol = m_selectedRegion->startCol();
        int endCol = m_selectedRegion->endCol();
        int startPos = m_selectedRegion->startPos();
        int endPos = m_selectedRegion->endPos();
        if (startCol == endCol && startPos > endPos) qSwap(startPos, endPos);
        for (int i = startCol; i <= endCol; ++i) {
            int bp = (i == startCol ? startPos : 0);
            int ep = (i == endCol ? endPos : m_charFormats.at(i).length());
            for (int j = bp; j < ep; ++j) {
                m_charFormats[i][j].underline = enabled;
            }
        }
    }
    scene()->update();
}

void CGraphicsEdit::setFontSize(int size)
{
    if (!m_selectedRegion->selected()) {
        m_textFormat.fontSize = size;
    } else {
        const int startCol = m_selectedRegion->startCol();
        int endCol = m_selectedRegion->endCol();
        int startPos = m_selectedRegion->startPos();
        int endPos = m_selectedRegion->endPos();
        if (startCol == endCol && startPos > endPos) qSwap(startPos, endPos);
        for (int i = startCol; i <= endCol; ++i) {
            int bp = (i == startCol ? startPos : 0);
            int ep = (i == endCol ? endPos : m_charFormats.at(i).length());
            for (int j = bp; j < ep; ++j) {
                m_charFormats[i][j].fontSize = size;
            }
        }
    }
    scene()->update();
}

void CGraphicsEdit::setStrikeOut(bool enabled)
{
    if (!m_selectedRegion->selected()) {
        m_textFormat.strikeOut = enabled;
    } else {
        const int startCol = m_selectedRegion->startCol();
        int endCol = m_selectedRegion->endCol();
        int startPos = m_selectedRegion->startPos();
        int endPos = m_selectedRegion->endPos();
        if (startCol == endCol && startPos > endPos) qSwap(startPos, endPos);
        for (int i = startCol; i <= endCol; ++i) {
            int bp = (i == startCol ? startPos : 0);
            int ep = (i == endCol ? endPos : m_charFormats.at(i).length());
            for (int j = bp; j < ep; ++j) {
                m_charFormats[i][j].strikeOut = enabled;
            }
        }
    }
    scene()->update();
}

void CGraphicsEdit::setColumnSpacing(qreal spacing)
{
    m_columnSpacing = spacing;
    scene()->update();
}

void CGraphicsEdit::setLetterSpacing(qreal spacing)
{
    if (!m_selectedRegion->selected()) {
        m_textFormat.letterSpacing = spacing;
    } else {
        const int startCol = m_selectedRegion->startCol();
        int endCol = m_selectedRegion->endCol();
        int startPos = m_selectedRegion->startPos();
        int endPos = m_selectedRegion->endPos();
        if (startCol == endCol && startPos > endPos) qSwap(startPos, endPos);
        for (int i = startCol; i <= endCol; ++i) {
            int bp = (i == startCol ? startPos : 0);
            int ep = (i == endCol ? endPos : m_charFormats.at(i).length());
            for (int j = bp; j < ep; ++j) {
                m_charFormats[i][j].letterSpacing = spacing;
            }
        }
    }
    scene()->update();
}

void CGraphicsEdit::setTextOriection(TextOriection oriection)
{
    m_oriection = oriection;
    if (oriection == TextVertical) {
        m_alignment = AlignmentTop;
    } else {
        m_alignment = AlignmentLeft;
    }
    scene()->update();
}

void CGraphicsEdit::onColorSelected(const QColor &color)
{
    if (!m_selectedRegion->selected()) {
        m_textFormat.fontColor = color;
    } else {
        const int startCol = m_selectedRegion->startCol();
        int endCol = m_selectedRegion->endCol();
        int startPos = m_selectedRegion->startPos();
        int endPos = m_selectedRegion->endPos();
        if (startCol == endCol && startPos > endPos) qSwap(startPos, endPos);
        for (int i = startCol; i <= endCol; ++i) {
            int bp = (i == startCol ? startPos : 0);
            int ep = (i == endCol ? endPos : m_charFormats.at(i).length());
            for (int j = bp; j < ep; ++j) {
                m_charFormats[i][j].fontColor = color;
            }
        }
    }
    scene()->update();
}

QString CGraphicsEdit::toHtml() const
{
    QString htmlStr;
    //先清空
    m_textItem->setHtml("");

    QTextCursor cursor = m_textItem->textCursor();
    for (int i = 0; i < m_textList.size(); ++i) {
        QString s = m_textList.at(i);
        for(int j = 0; j < s.length(); ++j) {
            QTextCharFormat tf;
            SCharFormat sf = m_charFormats.at(i).at(j);
            QFont font;
            sf.setFont(&font);
            tf.setFont(font);
            tf.setForeground(sf.fontColor);
            cursor.insertText(s.at(j), tf);
        }
        if (i != m_textList.size() - 1)
            cursor.insertText("\n");
    }
    QTextBlockFormat blockFormat = cursor.blockFormat();
    blockFormat.setLineHeight(m_columnSpacing, QTextBlockFormat::LineDistanceHeight);
    cursor.setBlockFormat(blockFormat);
    m_textItem->setTextCursor(cursor);
    htmlStr = m_textItem->toHtml(); 
    return htmlStr;
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
        m_charFormats.erase(m_charFormats.begin(), m_charFormats.end());
        m_textList.erase(m_textList.begin(), m_textList.end());
        //先通过QGraphicsTextItem转换格式
        m_textItem->setHtml(text);
        //set text
        QString plainText = m_textItem->toPlainText();
        QStringList textList = plainText.split(QChar('\n'));
        m_textList.swap(textList);
        m_cols += m_textList.size() - 1;
        m_currColumn += m_textList.size() - 1;
        m_postion = m_textList.at(m_currColumn).length();
        //set format
        QTextCursor cursor = m_textItem->textCursor();
        QList<SCharFormat> nullSf;
        m_charFormats << nullSf;
        int i = 1;
        for(int pos = 0; pos < plainText.length(); ++pos) {
            if (plainText.at(pos) == QChar('\n')) {
                i++;
                m_charFormats << nullSf;
                continue;
            }
            cursor.setPosition(pos);
            QTextCharFormat tf = cursor.charFormat();
            QFont f = tf.font();
            SCharFormat sf;
            sf.fromFont(f);
            sf.fontColor = tf.foreground().color();
            m_charFormats[i - 1].push_back(sf);
        }
        QTextBlockFormat blockFormat = cursor.blockFormat();
        m_columnSpacing = blockFormat.lineHeight();
        scene()->update();
    } while(0);
}
