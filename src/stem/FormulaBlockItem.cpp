#include "FormulaBlockItem.hpp"
#include "LatexRenderer.hpp"
#include "../app/AppIcons.hpp"
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QCursor>
#include <QPainterPath>
#include <QClipboard>
#include <QGuiApplication>
#include <QtMath>

namespace OmniNote {

FormulaBlockItem::FormulaBlockItem(const QString &latex, const QPointF &pos, const QColor &color, QGraphicsItem *parent)
    : QGraphicsObject(parent), m_color(color) {
    setPos(pos);
    setZValue(-4.0); // Above background (-20), below active pen strokes (+10)
    setFlags(ItemIsSelectable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    setCacheMode(QGraphicsItem::DeviceCoordinateCache);

    if (!latex.trimmed().isEmpty()) {
        m_latex = latex.trimmed();
    }
    updateRenderedPixmap();
}

void FormulaBlockItem::setLatex(const QString &latex, const QColor &color) {
    m_latex = latex.trimmed();
    m_color = color;
    updateRenderedPixmap();
    update();
    emit blockModified();
}

void FormulaBlockItem::setColor(const QColor &color) {
    m_color = color;
    updateRenderedPixmap();
    update();
    emit blockModified();
}

void FormulaBlockItem::updateRenderedPixmap() {
    m_renderedPixmap = LatexRenderer::instance().render(m_latex, m_color, 240);

    qreal w = 240.0;
    qreal h = 70.0;

    if (!m_renderedPixmap.isNull()) {
        // Adjust for device pixel ratio
        qreal dpr = m_renderedPixmap.devicePixelRatio();
        if (dpr <= 0.0) dpr = 1.0;
        w = (m_renderedPixmap.width() / dpr) * 0.75 + 24.0;
        h = (m_renderedPixmap.height() / dpr) * 0.75 + 20.0;
    }

    w = qMax(120.0, w);
    h = qMax(48.0, h);

    prepareGeometryChange();
    m_rect = QRectF(0, 0, w, h);
}

QRectF FormulaBlockItem::boundingRect() const {
    const qreal pad = 24.0; // Margin for floating pills on top and resize handle
    return m_rect.adjusted(-pad, -pad, pad, pad);
}

QRectF FormulaBlockItem::closeButtonRect() const {
    // Floating action button on top right
    return QRectF(m_rect.right() - 20.0, m_rect.top() - 22.0, 18.0, 18.0);
}

QRectF FormulaBlockItem::copyButtonRect() const {
    // Floating copy pill
    return QRectF(m_rect.right() - 68.0, m_rect.top() - 22.0, 44.0, 18.0);
}

QRectF FormulaBlockItem::editButtonRect() const {
    // Floating edit pill
    return QRectF(m_rect.right() - 116.0, m_rect.top() - 22.0, 44.0, 18.0);
}

QRectF FormulaBlockItem::resizeGripRect() const {
    return QRectF(m_rect.right() - 14.0, m_rect.bottom() - 14.0, 14.0, 14.0);
}

bool FormulaBlockItem::isControlHit(const QPointF &localPos) const {
    if (closeButtonRect().contains(localPos) || editButtonRect().contains(localPos) || copyButtonRect().contains(localPos)) {
        return true;
    }
    if (resizeGripRect().contains(localPos)) {
        return true;
    }
    if (localPos.y() <= 30.0 && localPos.x() >= m_rect.left() && localPos.x() <= m_rect.right()) {
        return true;
    }
    return false;
}

void FormulaBlockItem::openEditorDialog() {
    emit editRequested(this);
}

void FormulaBlockItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    const bool showControls = m_isHovered || isSelected();

    // 1. Subtle selection / hover indicator (completely invisible during normal reading)
    if (showControls) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 70));
        painter->drawRoundedRect(m_rect.adjusted(-2, 2, 2, 5), 8.0, 8.0);

        painter->setBrush(QColor(24, 28, 42, 160));
        QPen outlinePen(QColor("#7aa2f7"), 1.2, Qt::DashLine);
        painter->setPen(outlinePen);
        painter->drawRoundedRect(m_rect, 6.0, 6.0);
    }

    // 2. Render Mathematical Formula Pixmap (Crisp, High-DPI Vector Math)
    if (!m_renderedPixmap.isNull()) {
        QRectF formulaArea = m_rect.adjusted(12.0, 10.0, -12.0, -10.0);
        painter->drawPixmap(formulaArea.toRect(), m_renderedPixmap);
    } else {
        painter->setPen(QColor("#7aa2f7"));
        painter->setFont(QFont("Sans-Serif", 11));
        painter->drawText(m_rect, Qt::AlignCenter, m_latex);
    }

    // 3. Floating Modern Action Bar (Only visible on hover/selection)
    if (showControls) {
        // Floating Edit Pill [ Edit ]
        QRectF ed = editButtonRect();
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(36, 40, 59, 235));
        painter->drawRoundedRect(ed, 4.0, 4.0);
        QPixmap editPx = AppIcons::getPixmap(AppIcons::IconType::Edit, QColor("#c0caf5"), 11);
        painter->drawPixmap(ed.left() + 4.0, ed.center().y() - 5.5, editPx);
        painter->setPen(QColor("#c0caf5"));
        QFont pillFont("Sans-Serif", 7, QFont::Bold);
        painter->setFont(pillFont);
        painter->drawText(QRectF(ed.left() + 16.0, ed.top(), ed.width() - 18.0, ed.height()), Qt::AlignCenter, "Edit");

        // Floating Copy Pill [ Copy ]
        QRectF cop = copyButtonRect();
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(36, 40, 59, 235));
        painter->drawRoundedRect(cop, 4.0, 4.0);
        QPixmap copyPx = AppIcons::getPixmap(AppIcons::IconType::Copy, QColor("#c0caf5"), 11);
        painter->drawPixmap(cop.left() + 4.0, cop.center().y() - 5.5, copyPx);
        painter->setPen(QColor("#c0caf5"));
        painter->setFont(pillFont);
        painter->drawText(QRectF(cop.left() + 16.0, cop.top(), cop.width() - 18.0, cop.height()), Qt::AlignCenter, "Copy");

        // Floating Close Pill [ X ]
        QRectF cb = closeButtonRect();
        painter->setBrush(QColor(247, 118, 142, 220));
        painter->drawRoundedRect(cb, 4.0, 4.0);
        QPixmap closePx = AppIcons::getPixmap(AppIcons::IconType::Close, QColor("#ffffff"), 11);
        painter->drawPixmap(cb.center().x() - 5.5, cb.center().y() - 5.5, closePx);

        // Subtle Resize Grip corner
        QRectF rg = resizeGripRect();
        painter->setPen(QPen(QColor("#7aa2f7"), 1.3));
        painter->drawLine(rg.right() - 2.0, rg.bottom() - 8.0, rg.right() - 8.0, rg.bottom() - 2.0);
        painter->drawLine(rg.right() - 2.0, rg.bottom() - 4.0, rg.right() - 4.0, rg.bottom() - 2.0);
    }
}

void FormulaBlockItem::hoverEnterEvent(QGraphicsSceneHoverEvent *) {
    m_isHovered = true;
    update();
}

void FormulaBlockItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *) {
    m_isHovered = false;
    setCursor(Qt::ArrowCursor);
    update();
}

void FormulaBlockItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    QPointF pos = event->pos();
    if (closeButtonRect().contains(pos) || editButtonRect().contains(pos) || copyButtonRect().contains(pos)) {
        setCursor(Qt::PointingHandCursor);
    } else if (resizeGripRect().contains(pos)) {
        setCursor(Qt::SizeFDiagCursor);
    } else if (m_rect.contains(pos)) {
        setCursor(Qt::SizeAllCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
    update();
}

void FormulaBlockItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        QPointF pos = event->pos();

        if (closeButtonRect().contains(pos)) {
            emit itemDeleted(this);
            event->accept();
            return;
        }

        if (copyButtonRect().contains(pos)) {
            QClipboard *cb = QGuiApplication::clipboard();
            if (cb) cb->setText(m_latex);
            event->accept();
            return;
        }

        if (editButtonRect().contains(pos)) {
            openEditorDialog();
            event->accept();
            return;
        }

        if (resizeGripRect().contains(pos)) {
            m_isResizing = true;
            m_dragStartMousePos = event->scenePos();
            m_dragStartRect = m_rect;
            event->accept();
            return;
        }

        if (m_rect.contains(pos)) {
            m_isDragging = true;
            m_dragStartMousePos = event->scenePos();
            m_dragStartItemPos = this->pos();
            event->accept();
            return;
        }
    }
    QGraphicsObject::mousePressEvent(event);
}

void FormulaBlockItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    if (m_isResizing) {
        QPointF delta = event->scenePos() - m_dragStartMousePos;
        prepareGeometryChange();
        qreal newW = qMax(80.0, m_dragStartRect.width() + delta.x());
        qreal newH = qMax(32.0, m_dragStartRect.height() + delta.y());
        m_rect = QRectF(0, 0, newW, newH);
        update();
        emit blockModified();
        event->accept();
        return;
    }

    if (m_isDragging) {
        QPointF delta = event->scenePos() - m_dragStartMousePos;
        setPos(m_dragStartItemPos + delta);
        emit blockModified();
        event->accept();
        return;
    }

    QGraphicsObject::mouseMoveEvent(event);
}

void FormulaBlockItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    m_isResizing = false;
    m_isDragging = false;
    QGraphicsObject::mouseReleaseEvent(event);
}

void FormulaBlockItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        openEditorDialog();
        event->accept();
        return;
    }
    QGraphicsObject::mouseDoubleClickEvent(event);
}

QString FormulaBlockItem::serialize() const {
    QJsonObject root;
    root["latex"] = m_latex;
    root["color"] = m_color.name();
    root["w"] = m_rect.width();
    root["h"] = m_rect.height();
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void FormulaBlockItem::deserialize(const QString &data) {
    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
    if (!doc.isNull() && doc.isObject()) {
        QJsonObject root = doc.object();
        m_latex = root["latex"].toString();
        if (root.contains("color")) {
            m_color = QColor(root["color"].toString());
        }
        updateRenderedPixmap();
        if (root.contains("w") && root.contains("h")) {
            prepareGeometryChange();
            m_rect = QRectF(0, 0, root["w"].toDouble(), root["h"].toDouble());
        }
        update();
    }
}

} // namespace OmniNote
