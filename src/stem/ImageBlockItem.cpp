#include "ImageBlockItem.hpp"
#include "../app/AppIcons.hpp"
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QCursor>
#include <QPainterPath>
#include <QtMath>

namespace OmniNote {

ImageBlockItem::ImageBlockItem(const QImage &image, const QPointF &pos, QGraphicsItem *parent)
    : QGraphicsObject(parent), m_originalImage(image), m_image(image) {
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
    m_originalImage = image;
    m_image = image;
    m_backgroundRemoved = false;
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

void ImageBlockItem::toggleBackgroundRemoval() {
    setBackgroundRemoved(!m_backgroundRemoved);
}

void ImageBlockItem::setBackgroundRemoved(bool removed) {
    if (m_originalImage.isNull() && !m_image.isNull()) {
        m_originalImage = m_image;
    }
    if (m_originalImage.isNull()) return;

    m_backgroundRemoved = removed;
    if (m_backgroundRemoved) {
        m_image = processRemoveBackground(m_originalImage);
    } else {
        m_image = m_originalImage;
    }
    updatePixmap();
    update();
    emit blockModified();
}

QImage ImageBlockItem::processRemoveBackground(const QImage &source, qreal tolerance, qreal feather) {
    if (source.isNull()) return source;

    QImage img = source.convertToFormat(QImage::Format_ARGB32);
    const int w = img.width();
    const int h = img.height();
    if (w <= 0 || h <= 0) return source;

    // Sample background color around edges and corners
    const QVector<QPoint> samplePoints = {
        QPoint(0, 0), QPoint(w - 1, 0), QPoint(0, h - 1), QPoint(w - 1, h - 1),
        QPoint(w / 2, 0), QPoint(0, h / 2), QPoint(w - 1, h / 2), QPoint(w / 2, h - 1),
        QPoint(qMin(4, w - 1), qMin(4, h - 1)), QPoint(qMax(0, w - 5), qMin(4, h - 1))
    };

    qreal bgR = 0, bgG = 0, bgB = 0;
    int validSamples = 0;
    for (const QPoint &pt : samplePoints) {
        QRgb pixel = img.pixel(pt);
        if (qAlpha(pixel) > 10) {
            bgR += qRed(pixel);
            bgG += qGreen(pixel);
            bgB += qBlue(pixel);
            validSamples++;
        }
    }

    if (validSamples > 0) {
        bgR /= validSamples;
        bgG /= validSamples;
        bgB /= validSamples;
    } else {
        bgR = 255.0; bgG = 255.0; bgB = 255.0;
    }

    const qreal tolDist = tolerance * 255.0;
    const qreal featherDist = feather * 255.0;

    for (int y = 0; y < h; ++y) {
        auto *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < w; ++x) {
            QRgb p = line[x];
            int a = qAlpha(p);
            if (a == 0) continue;

            int r = qRed(p);
            int g = qGreen(p);
            int b = qBlue(p);

            // Perceptually weighted color distance to background
            qreal dr = r - bgR;
            qreal dg = g - bgG;
            qreal db = b - bgB;
            qreal dist = std::sqrt(0.299 * dr * dr + 0.587 * dg * dg + 0.114 * db * db);

            if (dist <= tolDist) {
                line[x] = qRgba(r, g, b, 0);
            } else if (dist < tolDist + featherDist && featherDist > 0.0) {
                qreal factor = (dist - tolDist) / featherDist;
                int newAlpha = qBound(0, static_cast<int>(std::round(a * factor)), 255);

                // Anti-fringe defringing: eliminate background tint from semi-transparent edge pixels
                if (newAlpha > 0) {
                    qreal alphaNorm = newAlpha / 255.0;
                    int defringedR = qBound(0, static_cast<int>(std::round((r - bgR * (1.0 - alphaNorm)) / alphaNorm)), 255);
                    int defringedG = qBound(0, static_cast<int>(std::round((g - bgG * (1.0 - alphaNorm)) / alphaNorm)), 255);
                    int defringedB = qBound(0, static_cast<int>(std::round((b - bgB * (1.0 - alphaNorm)) / alphaNorm)), 255);
                    line[x] = qRgba(defringedR, defringedG, defringedB, newAlpha);
                } else {
                    line[x] = qRgba(r, g, b, 0);
                }
            }
        }
    }

    return img;
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

QRectF ImageBlockItem::magicButtonRect() const {
    const qreal btnW = m_backgroundRemoved ? 94.0 : 106.0;
    return QRectF(m_rect.left() + 6.0, m_rect.top() + 6.0, btnW, 22.0);
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

        // 5. Magic Background Remover Pill ("✦ Quitar Fondo" / "↺ Con Fondo")
        QRectF mb = magicButtonRect();
        painter->setPen(QPen(m_backgroundRemoved ? QColor("#7dcfff") : QColor("#7aa2f7"), 1.2));
        painter->setBrush(QColor(22, 24, 34, 235));
        painter->drawRoundedRect(mb, 5.0, 5.0);

        QFont btnFont("Inter", 8, QFont::DemiBold);
        btnFont.setStyleHint(QFont::SansSerif);
        painter->setFont(btnFont);
        painter->setPen(m_backgroundRemoved ? QColor("#7dcfff") : QColor("#c0caf5"));
        QString btnText = m_backgroundRemoved ? "↺ Con Fondo" : "✦ Quitar Fondo";
        painter->drawText(mb, Qt::AlignCenter, btnText);

        // 6. Close Button (✕)
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
    } else if (closeButtonRect().contains(pos) || magicButtonRect().contains(pos)) {
        setCursor(Qt::PointingHandCursor);
    } else {
        setCursor(Qt::SizeAllCursor);
    }
    update();
}

void ImageBlockItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        QPointF pos = event->pos();

        // Check magic background button
        if (magicButtonRect().contains(pos)) {
            toggleBackgroundRemoval();
            event->accept();
            return;
        }

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
    root["backgroundRemoved"] = m_backgroundRemoved;

    // Save the original full-fidelity image so background removal can always be undone
    QImage toSave = m_originalImage.isNull() ? m_image : m_originalImage;
    if (!toSave.isNull()) {
        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::WriteOnly);
        toSave.save(&buffer, "PNG");
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
    m_backgroundRemoved = root["backgroundRemoved"].toBool(false);

    setPos(x, y);

    if (root.contains("imageData")) {
        QByteArray ba = QByteArray::fromBase64(root["imageData"].toString().toLatin1());
        QImage img;
        img.loadFromData(ba, "PNG");
        m_originalImage = img;
        if (m_backgroundRemoved && !m_originalImage.isNull()) {
            m_image = processRemoveBackground(m_originalImage);
        } else {
            m_image = img;
        }
    }

    prepareGeometryChange();
    m_rect = QRectF(0, 0, qMax(80.0, w), qMax(60.0, h));
    updatePixmap();
    update();
}

} // namespace OmniNote
