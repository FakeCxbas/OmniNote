#include "ImageBlockItem.hpp"
#include "../app/AppIcons.hpp"
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QCursor>
#include <QPainterPath>
#include <QtMath>

namespace OmniNote {

ImageBlockItem::ImageBlockItem(const QImage &image, const QPointF &pos, QGraphicsItem *parent)
    : QGraphicsObject(parent), m_image(image) {
    setPos(pos);
    setZValue(-5.0); // Rendered above page background (-20), below pen strokes (+10)
    setFlags(ItemIsSelectable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    setCacheMode(QGraphicsItem::DeviceCoordinateCache);

    if (!m_image.isNull()) {
        qreal w = m_image.width();
        qreal h = m_image.height();
        // Scale down large images to reasonable canvas initial size (max 520px)
        const qreal maxDim = 520.0;
        if (w > maxDim || h > maxDim) {
            qreal ratio = qMin(maxDim / w, maxDim / h);
            w *= ratio;
            h *= ratio;
        }
        m_rect = QRectF(0, 0, qMax(80.0, w), qMax(60.0, h));
        updatePixmap();
    }
}

void ImageBlockItem::setImage(const QImage &image) {
    prepareGeometryChange();
    m_image = image;
    if (!m_image.isNull()) {
        qreal w = m_image.width();
        qreal h = m_image.height();
        const qreal maxDim = 520.0;
        if (w > maxDim || h > maxDim) {
            qreal ratio = qMin(maxDim / w, maxDim / h);
            w *= ratio;
            h *= ratio;
        }
        m_rect = QRectF(0, 0, qMax(80.0, w), qMax(60.0, h));
    }
    updatePixmap();
    update();
    emit blockModified();
}

void ImageBlockItem::setSize(qreal w, qreal h) {
    prepareGeometryChange();
    m_rect = QRectF(0, 0, qMax(80.0, w), qMax(60.0, h));
    updatePixmap();
    update();
    emit blockModified();
}

void ImageBlockItem::updatePixmap() {
    if (!m_image.isNull()) {
        m_cachedPixmap = QPixmap::fromImage(m_image.scaled(m_rect.size().toSize(),
                                                           Qt::IgnoreAspectRatio,
                                                           Qt::SmoothTransformation));
    } else {
        m_cachedPixmap = QPixmap();
    }
}

QRectF ImageBlockItem::boundingRect() const {
    const qreal pad = 12.0; // Margin for handles and border glow
    return m_rect.adjusted(-pad, -pad, pad, pad);
}

QRectF ImageBlockItem::closeButtonRect() const {
    return QRectF(m_rect.right() - 24.0, m_rect.top() + 4.0, 20.0, 20.0);
}

ImageBlockItem::ResizeHandle ImageBlockItem::hitTestHandle(const QPointF &pos) const {
    const qreal hs = 14.0;
    if (QRectF(m_rect.left() - hs * 0.5, m_rect.top() - hs * 0.5, hs, hs).contains(pos)) return TopLeft;
    if (QRectF(m_rect.right() - hs * 0.5, m_rect.top() - hs * 0.5, hs, hs).contains(pos)) return TopRight;
    if (QRectF(m_rect.right() - hs * 0.5, m_rect.bottom() - hs * 0.5, hs, hs).contains(pos)) return BottomRight;
    if (QRectF(m_rect.left() - hs * 0.5, m_rect.bottom() - hs * 0.5, hs, hs).contains(pos)) return BottomLeft;
    return NoHandle;
}

void ImageBlockItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    const bool showControls = m_isHovered || isSelected();

    // 1. Drop shadow / subtle ambient halo
    if (showControls) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 80));
        painter->drawRoundedRect(m_rect.adjusted(-2, 2, 2, 6), 10.0, 10.0);
    }

    // 2. Draw Rounded Image Clip
    QPainterPath clipPath;
    clipPath.addRoundedRect(m_rect, 8.0, 8.0);
    painter->save();
    painter->setClipPath(clipPath);

    if (!m_cachedPixmap.isNull()) {
        painter->drawPixmap(m_rect.toRect(), m_cachedPixmap);
    } else {
        // Fallback placeholder card
        painter->fillRect(m_rect, QColor("#1e2030"));
        QPixmap imgPix = AppIcons::getPixmap(AppIcons::IconType::Image, QColor("#7aa2f7"), 24);
        painter->drawPixmap(m_rect.center().x() - 12, m_rect.center().y() - 18, imgPix);
        painter->setPen(QPen(QColor("#7aa2f7"), 1.0));
        painter->drawText(QRectF(m_rect.left(), m_rect.center().y() + 8, m_rect.width(), 20), Qt::AlignCenter, "[Sin Imagen]");
    }
    painter->restore();

    // 3. Subtle Outer Border
    QColor borderColor = showControls ? QColor("#7aa2f7") : QColor(47, 53, 73, 160);
    qreal borderWidth = showControls ? 1.8 : 1.0;
    painter->setPen(QPen(borderColor, borderWidth));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(m_rect, 8.0, 8.0);

    // 4. Interactive Corner Handles (when hovered or selected)
    if (showControls) {
        const qreal hs = 9.0;
        auto drawHandle = [&](const QPointF &center, bool isHovered) {
            QRectF hr(center.x() - hs * 0.5, center.y() - hs * 0.5, hs, hs);
            painter->setPen(QPen(QColor(15, 17, 26), 1.5));
            painter->setBrush(isHovered ? QColor("#7dcfff") : QColor("#7aa2f7"));
            painter->drawRoundedRect(hr, 2.5, 2.5);
        };

        drawHandle(m_rect.topLeft(), m_hoverResizeHandle == TopLeft);
        drawHandle(m_rect.topRight(), m_hoverResizeHandle == TopRight);
        drawHandle(m_rect.bottomRight(), m_hoverResizeHandle == BottomRight);
        drawHandle(m_rect.bottomLeft(), m_hoverResizeHandle == BottomLeft);

        // 5. Close Button (✕)
        QRectF cb = closeButtonRect();
        bool cbHover = cb.contains(m_dragStartMousePos); // or hover pos
        painter->setPen(Qt::NoPen);
        painter->setBrush(cbHover ? QColor(247, 118, 142, 220) : QColor(20, 22, 30, 180));
        painter->drawEllipse(cb);

        painter->setPen(QPen(QColor("#ffffff"), 1.6, Qt::SolidLine, Qt::RoundCap));
        painter->drawLine(cb.center() + QPointF(-3.5, -3.5), cb.center() + QPointF(3.5, 3.5));
        painter->drawLine(cb.center() + QPointF(-3.5, 3.5), cb.center() + QPointF(3.5, -3.5));
    }
}

void ImageBlockItem::hoverEnterEvent(QGraphicsSceneHoverEvent *) {
    m_isHovered = true;
    update();
}

void ImageBlockItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *) {
    m_isHovered = false;
    m_hoverResizeHandle = NoHandle;
    setCursor(Qt::ArrowCursor);
    update();
}

void ImageBlockItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    QPointF pos = event->pos();
    ResizeHandle h = hitTestHandle(pos);
    m_hoverResizeHandle = h;

    if (h == TopLeft || h == BottomRight) {
        setCursor(Qt::SizeFDiagCursor);
    } else if (h == TopRight || h == BottomLeft) {
        setCursor(Qt::SizeBDiagCursor);
    } else if (closeButtonRect().contains(pos)) {
        setCursor(Qt::PointingHandCursor);
    } else {
        setCursor(Qt::SizeAllCursor);
    }
    update();
}

void ImageBlockItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        QPointF pos = event->pos();

        // Check close button
        if (closeButtonRect().contains(pos)) {
            emit itemDeleted(this);
            event->accept();
            return;
        }

        // Check resize handles
        ResizeHandle h = hitTestHandle(pos);
        if (h != NoHandle) {
            m_activeResizeHandle = h;
            m_dragStartMousePos = event->scenePos();
            m_dragStartRect = m_rect;
            event->accept();
            return;
        }

        // Drag move
        m_isDragging = true;
        m_dragStartMousePos = event->scenePos();
        m_dragStartItemPos = this->pos();
        event->accept();
        return;
    }
    QGraphicsObject::mousePressEvent(event);
}

void ImageBlockItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    if (m_activeResizeHandle != NoHandle) {
        QPointF delta = event->scenePos() - m_dragStartMousePos;
        qreal newW = m_dragStartRect.width();
        qreal newH = m_dragStartRect.height();

        // Aspect ratio locked scaling
        const qreal aspect = (m_dragStartRect.height() > 0) ? (m_dragStartRect.width() / m_dragStartRect.height()) : 1.0;

        switch (m_activeResizeHandle) {
        case BottomRight: {
            newW = qMax(80.0, m_dragStartRect.width() + delta.x());
            newH = newW / aspect;
            break;
        }
        case BottomLeft: {
            newW = qMax(80.0, m_dragStartRect.width() - delta.x());
            newH = newW / aspect;
            break;
        }
        case TopRight: {
            newW = qMax(80.0, m_dragStartRect.width() + delta.x());
            newH = newW / aspect;
            break;
        }
        case TopLeft: {
            newW = qMax(80.0, m_dragStartRect.width() - delta.x());
            newH = newW / aspect;
            break;
        }
        default:
            break;
        }

        prepareGeometryChange();
        m_rect = QRectF(0, 0, newW, newH);
        updatePixmap();
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

void ImageBlockItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    if (m_activeResizeHandle != NoHandle) {
        m_activeResizeHandle = NoHandle;
        updatePixmap();
        update();
        event->accept();
        return;
    }

    if (m_isDragging) {
        m_isDragging = false;
        event->accept();
        return;
    }

    QGraphicsObject::mouseReleaseEvent(event);
}

QString ImageBlockItem::serialize() const {
    QJsonObject root;
    root["x"] = pos().x();
    root["y"] = pos().y();
    root["width"] = m_rect.width();
    root["height"] = m_rect.height();

    if (!m_image.isNull()) {
        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::WriteOnly);
        m_image.save(&buffer, "PNG");
        root["imageData"] = QString::fromLatin1(ba.toBase64());
    }

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void ImageBlockItem::deserialize(const QString &data) {
    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
    if (!doc.isObject()) return;

    QJsonObject root = doc.object();
    qreal x = root["x"].toDouble(pos().x());
    qreal y = root["y"].toDouble(pos().y());
    qreal w = root["width"].toDouble(300.0);
    qreal h = root["height"].toDouble(200.0);

    setPos(x, y);

    if (root.contains("imageData")) {
        QByteArray ba = QByteArray::fromBase64(root["imageData"].toString().toLatin1());
        QImage img;
        img.loadFromData(ba, "PNG");
        m_image = img;
    }

    prepareGeometryChange();
    m_rect = QRectF(0, 0, qMax(80.0, w), qMax(60.0, h));
    updatePixmap();
    update();
}

} // namespace OmniNote
