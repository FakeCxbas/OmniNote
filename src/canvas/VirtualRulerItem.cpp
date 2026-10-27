#include "VirtualRulerItem.hpp"
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QCursor>
#include <QPainterPath>
#include <QtMath>
#include <QFont>

namespace OmniNote {

VirtualRulerItem::VirtualRulerItem(const QPointF &pos, QGraphicsItem *parent)
    : QGraphicsObject(parent) {
    setPos(pos);
    setZValue(5.0); // Rendered above images/blocks, below active drawing pen strokes
    setFlags(ItemIsSelectable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    setTransformOriginPoint(m_length * 0.5, m_height * 0.5);
}

void VirtualRulerItem::setRulerLength(qreal len) {
    prepareGeometryChange();
    m_length = qBound(300.0, len, 1600.0);
    setTransformOriginPoint(m_length * 0.5, m_height * 0.5);
    update();
}

void VirtualRulerItem::setRulerAngle(qreal degrees) {
    m_angleDegrees = normalizeAngle(degrees);
    setRotation(m_angleDegrees);
    update();
    emit rulerMoved();
}

QRectF VirtualRulerItem::boundingRect() const {
    const qreal pad = 16.0;
    return QRectF(-pad, -pad, m_length + pad * 2.0, m_height + pad * 2.0);
}

QRectF VirtualRulerItem::closeButtonRect() const {
    return QRectF(m_length - 26.0, 10.0, 20.0, 20.0);
}

QRectF VirtualRulerItem::dialRect() const {
    qreal cx = m_length * 0.5;
    qreal cy = m_height * 0.5 + 4.0;
    qreal radius = 26.0;
    return QRectF(cx - radius, cy - radius, radius * 2.0, radius * 2.0);
}

bool VirtualRulerItem::isDrawingEdgeHit(const QPointF &localPos) const {
    // Top edge area (from y = -6 to y = 14) along the length of the ruler
    return (localPos.x() >= -10.0 && localPos.x() <= m_length + 10.0 && localPos.y() >= -8.0 && localPos.y() <= 16.0);
}

bool VirtualRulerItem::isControlHit(const QPointF &localPos) const {
    if (closeButtonRect().contains(localPos)) return true;
    if (dialRect().contains(localPos)) return true;
    // Anywhere else on the body except the top drawing edge
    return (localPos.x() >= 0.0 && localPos.x() <= m_length && localPos.y() > 16.0 && localPos.y() <= m_height);
}

qreal VirtualRulerItem::normalizeAngle(qreal deg) const {
    while (deg < 0.0) deg += 360.0;
    while (deg >= 360.0) deg -= 360.0;
    return deg;
}

qreal VirtualRulerItem::snapAngle(qreal deg) const {
    deg = normalizeAngle(deg);
    // Standard STEM angles: 0, 15, 30, 45, 60, 75, 90, 105, 120, 135, 150, 165, 180...
    const qreal snapInterval = 15.0;
    qreal nearest = std::round(deg / snapInterval) * snapInterval;
    if (std::abs(deg - nearest) <= 3.0) {
        return normalizeAngle(nearest);
    }
    return deg;
}

bool VirtualRulerItem::snapPoint(const QPointF &scenePt, QPointF &snappedPt, qreal threshold) const {
    if (!isVisible()) return false;

    // Top straight drawing edge from (0, 0) to (m_length, 0)
    QPointF A = mapToScene(QPointF(0.0, 0.0));
    QPointF B = mapToScene(QPointF(m_length, 0.0));

    QPointF AB = B - A;
    qreal lenSq = AB.x() * AB.x() + AB.y() * AB.y();
    if (lenSq < 1e-4) return false;

    QPointF AP = scenePt - A;
    qreal t = (AP.x() * AB.x() + AP.y() * AB.y()) / lenSq;

    // Allow drawing slightly beyond ruler tips (-0.05 to 1.05)
    if (t < -0.05 || t > 1.05) return false;

    qreal tClamped = qBound(0.0, t, 1.0);
    QPointF proj = A + tClamped * AB;

    qreal dist = std::hypot(scenePt.x() - proj.x(), scenePt.y() - proj.y());
    if (dist <= threshold) {
        snappedPt = proj;
        return true;
    }
    return false;
}

void VirtualRulerItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    QRectF bodyRect(0, 0, m_length, m_height);

    // 1. Subtle drop shadow
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0, 0, 0, 90));
    painter->drawRoundedRect(bodyRect.adjusted(-2, 3, 2, 7), 8.0, 8.0);

    // 2. Translucent Glass Body (Tokyo Night style)
    painter->setBrush(QColor(26, 27, 38, 220)); // #1a1b26 semi-transparent
    painter->setPen(QPen(QColor(122, 162, 247, 200), 1.5)); // #7aa2f7 accent border
    painter->drawRoundedRect(bodyRect, 6.0, 6.0);

    // 3. Highlight on Top Straight Drawing Edge
    painter->setPen(QPen(QColor("#7dcfff"), 2.2, Qt::SolidLine, Qt::FlatCap));
    painter->drawLine(QPointF(0.0, 0.0), QPointF(m_length, 0.0));

    // 4. Centimeter & Millimeter Scale Ticks
    // Let 1 cm = 40 pixels
    const qreal pxPerCm = 40.0;
    const qreal pxPerMm = pxPerCm / 10.0;
    const int totalMm = static_cast<int>(m_length / pxPerMm);

    QFont markFont("Sans-Serif", 8, QFont::DemiBold);
    painter->setFont(markFont);

    for (int mm = 0; mm <= totalMm; ++mm) {
        qreal x = mm * pxPerMm;
        if (x > m_length - 2.0) break;

        if (mm % 10 == 0) {
            // Major tick: 1 cm
            int cm = mm / 10;
            painter->setPen(QPen(QColor("#7dcfff"), 1.6));
            painter->drawLine(QPointF(x, 0.0), QPointF(x, 18.0));

            // Centimeter number
            painter->setPen(QColor("#c0caf5"));
            QRectF txtRect(x - 14.0, 20.0, 28.0, 14.0);
            painter->drawText(txtRect, Qt::AlignCenter, QString::number(cm));
        } else if (mm % 5 == 0) {
            // Medium tick: 0.5 cm
            painter->setPen(QPen(QColor(192, 202, 245, 180), 1.2));
            painter->drawLine(QPointF(x, 0.0), QPointF(x, 12.0));
        } else {
            // Minor tick: 1 mm
            painter->setPen(QPen(QColor(86, 95, 137, 160), 1.0));
            painter->drawLine(QPointF(x, 0.0), QPointF(x, 6.0));
        }
    }

    // 5. Central Rotation Dial with Angle Readout
    QRectF dial = dialRect();
    painter->setPen(QPen(m_isRotatingDial ? QColor("#bb9af7") : QColor("#7aa2f7"), 1.8));
    painter->setBrush(QColor(36, 40, 59, 230));
    painter->drawEllipse(dial);

    // Dial angle tick marks
    QPointF dc = dial.center();
    painter->setPen(QPen(QColor(122, 162, 247, 120), 1.0));
    for (int deg = 0; deg < 360; deg += 45) {
        qreal rad = qDegreesToRadians(static_cast<qreal>(deg));
        QPointF p1 = dc + QPointF(std::cos(rad) * 18.0, std::sin(rad) * 18.0);
        QPointF p2 = dc + QPointF(std::cos(rad) * 23.0, std::sin(rad) * 23.0);
        painter->drawLine(p1, p2);
    }

    // Angle text inside dial
    QFont dialFont("Sans-Serif", 8, QFont::Bold);
    painter->setFont(dialFont);
    painter->setPen(QColor("#ffffff"));
    QString angleStr = QString("%1°").arg(std::round(m_angleDegrees));
    painter->drawText(dial, Qt::AlignCenter, angleStr);

    // 6. Close Button (✕)
    QRectF cb = closeButtonRect();
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(247, 118, 142, 200));
    painter->drawEllipse(cb);

    painter->setPen(QPen(QColor("#ffffff"), 1.6, Qt::SolidLine, Qt::RoundCap));
    painter->drawLine(cb.center() + QPointF(-3.5, -3.5), cb.center() + QPointF(3.5, 3.5));
    painter->drawLine(cb.center() + QPointF(-3.5, 3.5), cb.center() + QPointF(3.5, -3.5));
}

void VirtualRulerItem::hoverEnterEvent(QGraphicsSceneHoverEvent *) {
    m_isHovered = true;
    update();
}

void VirtualRulerItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *) {
    m_isHovered = false;
    setCursor(Qt::ArrowCursor);
    update();
}

void VirtualRulerItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    QPointF pos = event->pos();
    if (closeButtonRect().contains(pos)) {
        setCursor(Qt::PointingHandCursor);
    } else if (dialRect().contains(pos)) {
        setCursor(Qt::CrossCursor);
    } else if (isDrawingEdgeHit(pos)) {
        setCursor(Qt::IBeamCursor);
    } else {
        setCursor(Qt::SizeAllCursor);
    }
    update();
}

void VirtualRulerItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        QPointF pos = event->pos();

        // 1. Close button
        if (closeButtonRect().contains(pos)) {
            emit closeRequested();
            event->accept();
            return;
        }

        // 2. Rotation dial
        if (dialRect().contains(pos)) {
            m_isRotatingDial = true;
            m_dragStartMouseScenePos = event->scenePos();
            QPointF centerScene = mapToScene(dialRect().center());
            QPointF diff = event->scenePos() - centerScene;
            m_dialStartMouseAngle = qRadiansToDegrees(std::atan2(diff.y(), diff.x()));
            m_dialStartRulerAngle = m_angleDegrees;
            setCursor(Qt::CrossCursor);
            event->accept();
            return;
        }

        // 3. Move body
        if (pos.y() > 14.0) {
            m_isDraggingBody = true;
            m_dragStartMouseScenePos = event->scenePos();
            m_dragStartItemPos = this->pos();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
    }
    QGraphicsObject::mousePressEvent(event);
}

void VirtualRulerItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    if (m_isRotatingDial) {
        QPointF centerScene = mapToScene(dialRect().center());
        QPointF diff = event->scenePos() - centerScene;
        qreal currentMouseAngle = qRadiansToDegrees(std::atan2(diff.y(), diff.x()));
        qreal deltaAngle = currentMouseAngle - m_dialStartMouseAngle;
        qreal rawAngle = m_dialStartRulerAngle + deltaAngle;
        qreal snapped = snapAngle(rawAngle);

        setRulerAngle(snapped);
        event->accept();
        return;
    }

    if (m_isDraggingBody) {
        QPointF delta = event->scenePos() - m_dragStartMouseScenePos;
        setPos(m_dragStartItemPos + delta);
        emit rulerMoved();
        event->accept();
        return;
    }

    QGraphicsObject::mouseMoveEvent(event);
}

void VirtualRulerItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    if (m_isRotatingDial) {
        m_isRotatingDial = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }

    if (m_isDraggingBody) {
        m_isDraggingBody = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }

    QGraphicsObject::mouseReleaseEvent(event);
}

} // namespace OmniNote
