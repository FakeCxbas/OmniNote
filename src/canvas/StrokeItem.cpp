#include "StrokeItem.hpp"
#include <QPainter>
#include <QPainterPathStroker>
#include <QDataStream>
#include <QIODevice>
#include <QtMath>
#include <array>

namespace OmniNote {

static inline qreal fastPow12(qreal p) {
    static const auto lut = []() {
        std::array<qreal, 256> table;
        for (int i = 0; i < 256; ++i) {
            table[i] = std::pow(static_cast<qreal>(i) / 255.0, 1.2);
        }
        return table;
    }();
    int idx = qBound(0, static_cast<int>(p * 255.0), 255);
    return lut[idx];
}

StrokeItem::StrokeItem(ToolType toolType, const QColor &color, qreal baseWidth, QGraphicsItem *parent)
    : QGraphicsItem(parent), m_toolType(toolType), m_color(color), m_baseWidth(baseWidth) {
    setFlags(ItemIsSelectable);
    if (m_toolType == ToolType::Highlighter) {
        setZValue(-10.0); // Rendered below normal pen ink
    } else {
        setZValue(10.0);  // Rendered above
    }
}

QRectF StrokeItem::boundingRect() const {
    return m_boundingRect;
}

QPainterPath StrokeItem::shape() const {
    if (!m_ribbon.isEmpty()) {
        QPainterPath path;
        path.addPolygon(m_ribbon);
        return path;
    }
    QPainterPath path;
    path.addRect(m_boundingRect);
    return path;
}

void StrokeItem::setToolType(ToolType t) {
    m_toolType = t;
    if (m_toolType == ToolType::Highlighter) {
        setZValue(-10.0);
    } else {
        setZValue(10.0);
    }
    rebuildGeometry();
}

void StrokeItem::setColor(const QColor &c) {
    m_color = c;
    update();
}

void StrokeItem::setBaseWidth(qreal w) {
    m_baseWidth = qBound(0.5, w, 50.0);
    rebuildGeometry();
}

void StrokeItem::setPoints(const QVector<StrokePoint> &pts) {
    m_points = pts;
    rebuildGeometry();
}

void StrokeItem::addPoint(const StrokePoint &pt) {
    addPointFast(pt);
}

void StrokeItem::addPointFast(const StrokePoint &pt) {
    prepareGeometryChange();
    m_points.append(pt);
    m_smoothedPoints.append(pt);

    const qreal widthMultiplier = (m_toolType == ToolType::Highlighter) ? 3.2 : 1.0;
    const qreal pad = m_baseWidth * widthMultiplier + 8.0;
    QRectF ptRect(pt.pos.x() - pad, pt.pos.y() - pad, 2.0 * pad, 2.0 * pad);

    QRectF dirtyRect = ptRect;
    if (m_boundingRect.isEmpty()) {
        m_boundingRect = ptRect;
    } else {
        m_boundingRect = m_boundingRect.united(ptRect);
        if (m_points.size() >= 2) {
            const QPointF &prevPos = m_points[m_points.size() - 2].pos;
            dirtyRect = dirtyRect.united(QRectF(prevPos.x() - pad, prevPos.y() - pad, 2.0 * pad, 2.0 * pad));
        }
    }
    update(dirtyRect);
}

void StrokeItem::finalizeStroke() {
    if (m_autoImprove && (m_toolType == ToolType::Pen || m_toolType == ToolType::Highlighter)) {
        if (m_points.size() >= 3) {
            prepareGeometryChange();
            m_points = StrokeSmoother::beautifyStroke(m_points, true);
        }
    }
    rebuildGeometry();
    setCacheMode(QGraphicsItem::DeviceCoordinateCache);
}

void StrokeItem::translateStroke(const QPointF &delta) {
    if (delta.isNull()) return;
    prepareGeometryChange();
    for (auto &pt : m_points) {
        pt.pos += delta;
    }
    rebuildGeometry();
}

void StrokeItem::scaleStroke(qreal factor, const QPointF &pivot) {
    if (factor <= 0.05 || factor > 20.0 || std::abs(factor - 1.0) < 1e-4) return;
    prepareGeometryChange();
    for (auto &pt : m_points) {
        pt.pos = pivot + (pt.pos - pivot) * factor;
    }
    m_baseWidth = qBound(0.5, m_baseWidth * factor, 40.0);
    rebuildGeometry();
}

void StrokeItem::rebuildGeometry() {
    prepareGeometryChange();
    m_shapePath = QPainterPath();
    m_smoothedPoints.clear();

    if (m_points.isEmpty()) {
        m_boundingRect = QRectF();
        m_ribbon.clear();
        return;
    }

    if (m_toolType == ToolType::Pen || m_toolType == ToolType::Highlighter) {
        const qreal widthMultiplier = (m_toolType == ToolType::Highlighter) ? 3.2 : 1.0;

        if (m_points.size() <= 2) {
            m_smoothedPoints = m_points;
        } else {
            m_smoothedPoints = StrokeSmoother::interpolateCatmullRom(m_points, 8);
        }

        m_ribbon = StrokeSmoother::generateRibbonPolygon(m_smoothedPoints, m_baseWidth * widthMultiplier);

        qreal minX = m_smoothedPoints[0].pos.x(), maxX = m_smoothedPoints[0].pos.x();
        qreal minY = m_smoothedPoints[0].pos.y(), maxY = m_smoothedPoints[0].pos.y();
        for (const auto &p : m_smoothedPoints) {
            if (p.pos.x() < minX) minX = p.pos.x();
            if (p.pos.x() > maxX) maxX = p.pos.x();
            if (p.pos.y() < minY) minY = p.pos.y();
            if (p.pos.y() > maxY) maxY = p.pos.y();
        }
        const qreal pad = m_baseWidth * widthMultiplier + 8.0;
        m_boundingRect = QRectF(minX - pad, minY - pad, (maxX - minX) + 2.0 * pad, (maxY - minY) + 2.0 * pad);

    } else if (m_toolType == ToolType::ShapeLine) {
        QPointF p1 = m_points.first().pos;
        QPointF p2 = m_points.last().pos;
        m_boundingRect = QRectF(p1, p2).normalized().adjusted(-m_baseWidth * 2, -m_baseWidth * 2, m_baseWidth * 2, m_baseWidth * 2);

    } else if (m_toolType == ToolType::ShapeRect) {
        QPointF p1 = m_points.first().pos;
        QPointF p2 = m_points.last().pos;
        QRectF rect = QRectF(p1, p2).normalized();
        m_boundingRect = rect.adjusted(-m_baseWidth * 2, -m_baseWidth * 2, m_baseWidth * 2, m_baseWidth * 2);

    } else if (m_toolType == ToolType::ShapeEllipse) {
        QPointF p1 = m_points.first().pos;
        QPointF p2 = m_points.last().pos;
        QRectF rect = QRectF(p1, p2).normalized();
        m_boundingRect = rect.adjusted(-m_baseWidth * 2, -m_baseWidth * 2, m_baseWidth * 2, m_baseWidth * 2);

    } else if (m_toolType == ToolType::ShapeAxes2D || m_toolType == ToolType::ShapeAxes3D) {
        QPointF p1 = m_points.first().pos;
        QPointF p2 = m_points.last().pos;
        QRectF rect = QRectF(p1, p2).normalized();
        m_boundingRect = rect.adjusted(-m_baseWidth * 3 - 15, -m_baseWidth * 3 - 15, m_baseWidth * 3 + 15, m_baseWidth * 3 + 15);
    }

    setCacheMode(QGraphicsItem::DeviceCoordinateCache);
    update();
}

void StrokeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    if (m_points.isEmpty()) return;

    painter->setRenderHint(QPainter::Antialiasing, true);

    if (m_toolType == ToolType::Highlighter) {
        painter->save();
        painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
        QColor hlColor = m_color;
        hlColor.setAlpha(95);
        const qreal widthMultiplier = 3.2;
        QPen hlPen(hlColor, m_baseWidth * widthMultiplier, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(hlPen);
        painter->setBrush(Qt::NoBrush);

        const auto &pts = m_smoothedPoints.isEmpty() ? m_points : m_smoothedPoints;
        if (pts.size() == 1) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(hlColor);
            qreal r = (m_baseWidth * widthMultiplier) * 0.5;
            painter->drawEllipse(pts[0].pos, r, r);
        } else {
            QPolygonF poly;
            poly.reserve(pts.size());
            for (const auto &p : pts) {
                poly << p.pos;
            }
            painter->drawPolyline(poly);
        }
        painter->restore();
    } else if (m_toolType == ToolType::Pen) {
        const auto &pts = m_smoothedPoints.isEmpty() ? m_points : m_smoothedPoints;
        if (pts.size() == 1) {
            qreal pr = qBound(0.05, pts[0].pressure, 1.0);
            qreal r = qMax(0.6, m_baseWidth * (0.20 + 1.25 * std::pow(pr, 1.2)) * 0.5);
            painter->setPen(Qt::NoPen);
            painter->setBrush(m_color);
            painter->drawEllipse(pts[0].pos, r, r);
        } else {
            // Check if pressure is virtually identical across the entire stroke
            bool uniformPressure = true;
            const qreal firstPressure = pts[0].pressure;
            for (const auto &p : pts) {
                if (std::abs(p.pressure - firstPressure) > 0.03) {
                    uniformPressure = false;
                    break;
                }
            }

            if (uniformPressure) {
                qreal w = qMax(0.6, m_baseWidth * (0.20 + 1.25 * std::pow(firstPressure, 1.2)));
                QPen pen(m_color, w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
                painter->setPen(pen);
                painter->setBrush(Qt::NoBrush);
                QPolygonF poly;
                poly.reserve(pts.size());
                for (const auto &p : pts) {
                    poly << p.pos;
                }
                painter->drawPolyline(poly);
            } else {
                // Expressive pressure dynamics (tablet stylus or dynamic velocity pressure)
                painter->setBrush(Qt::NoBrush);
                qreal lastW = -1.0;
                QPen segPen(m_color, m_baseWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

                for (int i = 0; i < pts.size() - 1; ++i) {
                    qreal pAvg = qBound(0.05, (pts[i].pressure + pts[i + 1].pressure) * 0.5, 1.0);
                    qreal segWidth = qMax(0.6, m_baseWidth * (0.20 + 1.25 * fastPow12(pAvg)));
                    if (std::abs(segWidth - lastW) > 0.04) {
                        segPen.setWidthF(segWidth);
                        painter->setPen(segPen);
                        lastW = segWidth;
                    }
                    painter->drawLine(pts[i].pos, pts[i + 1].pos);
                }
            }
        }
    } else {
        QPen shapePen(m_color, m_baseWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(shapePen);
        painter->setBrush(Qt::NoBrush);

        QPointF p1 = m_points.first().pos;
        QPointF p2 = m_points.last().pos;

        if (m_toolType == ToolType::ShapeLine) {
            painter->drawLine(p1, p2);
        } else if (m_toolType == ToolType::ShapeRect) {
            painter->drawRect(QRectF(p1, p2).normalized());
        } else if (m_toolType == ToolType::ShapeEllipse) {
            painter->drawEllipse(QRectF(p1, p2).normalized());
        } else if (m_toolType == ToolType::ShapeAxes2D) {
            QRectF rect = QRectF(p1, p2).normalized();
            QPointF origin(rect.left() + rect.width() * 0.3, rect.bottom() - rect.height() * 0.3);

            // X-Axis
            painter->drawLine(origin, QPointF(rect.right(), origin.y()));
            painter->drawLine(QPointF(rect.right(), origin.y()), QPointF(rect.right() - 8, origin.y() - 5));
            painter->drawLine(QPointF(rect.right(), origin.y()), QPointF(rect.right() - 8, origin.y() + 5));

            // Y-Axis
            painter->drawLine(origin, QPointF(origin.x(), rect.top()));
            painter->drawLine(QPointF(origin.x(), rect.top()), QPointF(origin.x() - 5, rect.top() + 8));
            painter->drawLine(QPointF(origin.x(), rect.top()), QPointF(origin.x() + 5, rect.top() + 8));

            // Scale tick marks
            qreal tickDist = 20.0;
            for (qreal x = origin.x() + tickDist; x < rect.right() - 12; x += tickDist) {
                painter->drawLine(QPointF(x, origin.y() - 3), QPointF(x, origin.y() + 3));
            }
            for (qreal y = origin.y() - tickDist; y > rect.top() + 12; y -= tickDist) {
                painter->drawLine(QPointF(origin.x() - 3, y), QPointF(origin.x() + 3, y));
            }

            QFont font("Fira Code", 8, QFont::Bold);
            painter->setFont(font);
            painter->drawText(QPointF(rect.right() - 6, origin.y() - 6), "x");
            painter->drawText(QPointF(origin.x() + 8, rect.top() + 12), "y");

        } else if (m_toolType == ToolType::ShapeAxes3D) {
            QRectF rect = QRectF(p1, p2).normalized();
            QPointF center = rect.center();
            qreal r = qMin(rect.width(), rect.height()) * 0.45;

            // Z-axis (straight up)
            QPointF pZ = center - QPointF(0, r);
            painter->drawLine(center, pZ);
            painter->drawLine(pZ, pZ + QPointF(-4, 6));
            painter->drawLine(pZ, pZ + QPointF(4, 6));

            // X-axis (down-left at 210 degrees)
            qreal angleX = 210.0 * M_PI / 180.0;
            QPointF pX = center + QPointF(r * std::cos(angleX), -r * std::sin(angleX));
            painter->drawLine(center, pX);

            // Y-axis (down-right at 330 degrees)
            qreal angleY = 330.0 * M_PI / 180.0;
            QPointF pY = center + QPointF(r * std::cos(angleY), -r * std::sin(angleY));
            painter->drawLine(center, pY);

            QFont font("Fira Code", 8, QFont::Bold);
            painter->setFont(font);
            painter->drawText(pZ + QPointF(6, 10), "z");
            painter->drawText(pX + QPointF(-12, 10), "x");
            painter->drawText(pY + QPointF(8, 10), "y");
        }
    }
}

QByteArray StrokeItem::serialize() const {
    QByteArray uncompressed;
    QDataStream stream(&uncompressed, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);

    const quint32 magic = 0x4F4D4E49; // "OMNI"
    const quint16 version = 1;

    stream << magic << version;
    stream << static_cast<qint32>(m_toolType);
    stream << m_color.rgba();
    stream << m_baseWidth;
    stream << zValue();

    stream << static_cast<qint32>(m_points.size());
    for (const auto &pt : m_points) {
        stream << pt.pos.x() << pt.pos.y() << pt.pressure << pt.tiltX << pt.tiltY << pt.timestamp;
    }

    return qCompress(uncompressed, 9);
}

StrokeItem *StrokeItem::deserialize(const QByteArray &data) {
    QByteArray uncompressed = qUncompress(data);
    if (uncompressed.isEmpty()) {
        return nullptr;
    }

    QDataStream stream(&uncompressed, QIODevice::ReadOnly);
    stream.setVersion(QDataStream::Qt_6_0);

    quint32 magic;
    quint16 version;
    stream >> magic >> version;

    if (magic != 0x4F4D4E49 || version != 1) {
        return nullptr;
    }

    qint32 rawToolType;
    QRgb colorRgba;
    qreal baseWidth;
    qreal zVal;
    qint32 count;

    stream >> rawToolType >> colorRgba >> baseWidth >> zVal >> count;

    auto *item = new StrokeItem(static_cast<ToolType>(rawToolType), QColor::fromRgba(colorRgba), baseWidth);
    item->setZValue(zVal);

    QVector<StrokePoint> points;
    points.reserve(count);

    for (int i = 0; i < count; ++i) {
        double px, py, pr, tx, ty;
        qint64 ts;
        stream >> px >> py >> pr >> tx >> ty >> ts;
        points.append(StrokePoint(QPointF(px, py), pr, tx, ty, ts));
    }

    item->setPoints(points);
    return item;
}

} // namespace OmniNote
