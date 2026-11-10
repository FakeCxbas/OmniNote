#include "StrokeItem.hpp"
#include <QPainter>
#include <QPainterPathStroker>
#include <QDataStream>
#include <QIODevice>
#include <QtMath>
#include <array>

namespace OmniNote {

static inline qreal fastCanvaPressureFactor(qreal p) {
    static const auto lut = []() {
        std::array<qreal, 256> table;
        for (int i = 0; i < 256; ++i) {
            qreal normalizedP = static_cast<qreal>(i) / 255.0;
            // Canva pen dynamics: stable, uniform ink body with gentle organic modulation
            // p=0.0 -> 0.85, p=0.5 -> 0.98 (~1.00), p=1.0 -> 1.15
            table[i] = 0.85 + 0.30 * std::pow(normalizedP, 1.2);
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

    const qreal widthMultiplier = (m_toolType == ToolType::Highlighter) ? 3.2 : 1.35;
    const qreal pad = m_baseWidth * widthMultiplier + 8.0;
    QRectF ptRect(pt.pos.x() - pad, pt.pos.y() - pad, 2.0 * pad, 2.0 * pad);

    QRectF dirtyRect = ptRect;
    if (m_boundingRect.isEmpty()) {
        m_boundingRect = ptRect;
    } else {
        m_boundingRect = m_boundingRect.united(ptRect);
        if (m_points.size() >= 2) {
            const QPointF &p1 = m_points[m_points.size() - 2].pos;
            dirtyRect = dirtyRect.united(QRectF(p1.x() - pad, p1.y() - pad, 2.0 * pad, 2.0 * pad));
        }
        if (m_points.size() >= 3) {
            const QPointF &p2 = m_points[m_points.size() - 3].pos;
            dirtyRect = dirtyRect.united(QRectF(p2.x() - pad, p2.y() - pad, 2.0 * pad, 2.0 * pad));
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
    m_cachedSpans.clear();
    m_cachedSinglePath = QPainterPath();
    m_cachedSingleWidth = 0.0;
    m_cachedHighlighterPoly.clear();

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
        const qreal pad = m_baseWidth * widthMultiplier * 1.35 + 8.0;
        m_boundingRect = QRectF(minX - pad, minY - pad, (maxX - minX) + 2.0 * pad, (maxY - minY) + 2.0 * pad);

        // Precompute cached paths for lightning-fast zero-allocation painting
        if (m_toolType == ToolType::Highlighter) {
            m_cachedHighlighterPoly.reserve(m_smoothedPoints.size());
            for (const auto &p : m_smoothedPoints) {
                m_cachedHighlighterPoly << p.pos;
            }
        } else if (m_toolType == ToolType::Pen) {
            const auto &pts = m_smoothedPoints;
            const int n = pts.size();
            if (n > 2) {
                qreal minP = 1.0, maxP = 0.0, sumP = 0.0;
                for (const auto &p : pts) {
                    if (p.pressure < minP) minP = p.pressure;
                    if (p.pressure > maxP) maxP = p.pressure;
                    sumP += p.pressure;
                }
                qreal avgP = sumP / static_cast<qreal>(n);

                qreal totalLength = 0.0;
                QVector<qreal> cumDist;
                cumDist.reserve(n);
                cumDist.append(0.0);
                for (int i = 0; i < n - 1; ++i) {
                    totalLength += std::hypot(pts[i + 1].pos.x() - pts[i].pos.x(),
                                              pts[i + 1].pos.y() - pts[i].pos.y());
                    cumDist.append(totalLength);
                }
                const qreal taperWindow = qMin(8.0, totalLength * 0.20);

                if ((maxP - minP <= 0.10) && (totalLength <= 14.0 || taperWindow <= 0.5)) {
                    m_cachedSinglePath.moveTo(pts[0].pos);
                    for (int i = 1; i < n - 1; ++i) {
                        QPointF mid = (pts[i].pos + pts[i + 1].pos) * 0.5;
                        m_cachedSinglePath.quadTo(pts[i].pos, mid);
                    }
                    m_cachedSinglePath.lineTo(pts.last().pos);
                    m_cachedSingleWidth = qMax(0.6, m_baseWidth * fastCanvaPressureFactor(avgP));
                } else {
                    m_cachedSpans.reserve(n);
                    QPointF mid0 = (pts[0].pos + pts[1].pos) * 0.5;
                    qreal p0 = pts[0].pressure;
                    qreal taper0 = (totalLength > 3.0 && taperWindow > 0.5) ? 0.88 : 1.0;
                    qreal w0 = qMax(0.6, m_baseWidth * fastCanvaPressureFactor(p0) * taper0);

                    QPainterPath path0;
                    path0.moveTo(pts[0].pos);
                    path0.lineTo(mid0);
                    m_cachedSpans.append({path0, w0});

                    QPointF prevMid = mid0;
                    for (int i = 1; i < n - 1; ++i) {
                        QPointF nextMid = (pts[i].pos + pts[i + 1].pos) * 0.5;
                        qreal segDist = cumDist[i];
                        qreal taper = 1.0;
                        if (totalLength > 3.0 && taperWindow > 0.5) {
                            if (segDist < taperWindow) {
                                qreal u = segDist / taperWindow;
                                taper = 0.88 + 0.12 * (3.0 * u * u - 2.0 * u * u * u);
                            } else if (totalLength - segDist < taperWindow) {
                                qreal u = (totalLength - segDist) / taperWindow;
                                taper = 0.88 + 0.12 * (3.0 * u * u - 2.0 * u * u * u);
                            }
                        }
                        qreal segW = qMax(0.6, m_baseWidth * fastCanvaPressureFactor(pts[i].pressure) * taper);
                        QPainterPath quadSpan;
                        quadSpan.moveTo(prevMid);
                        quadSpan.quadTo(pts[i].pos, nextMid);
                        m_cachedSpans.append({quadSpan, segW});
                        prevMid = nextMid;
                    }

                    qreal pn = pts[n - 1].pressure;
                    qreal tapern = (totalLength > 3.0 && taperWindow > 0.5) ? 0.88 : 1.0;
                    qreal wn = qMax(0.6, m_baseWidth * fastCanvaPressureFactor(pn) * tapern);
                    QPainterPath pathN;
                    pathN.moveTo(prevMid);
                    pathN.lineTo(pts[n - 1].pos);
                    m_cachedSpans.append({pathN, wn});
                }
            }
        }

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

        if (!m_cachedHighlighterPoly.isEmpty()) {
            painter->drawPolyline(m_cachedHighlighterPoly);
        } else {
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
        }
        painter->restore();
    } else if (m_toolType == ToolType::Pen) {
        if (!m_cachedSpans.isEmpty()) {
            painter->setBrush(Qt::NoBrush);
            QPen pen(m_color, 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            for (const auto &span : m_cachedSpans) {
                pen.setWidthF(span.width);
                painter->setPen(pen);
                painter->strokePath(span.path, pen);
            }
            return;
        } else if (!m_cachedSinglePath.isEmpty()) {
            painter->setBrush(Qt::NoBrush);
            QPen pen(m_color, m_cachedSingleWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter->setPen(pen);
            painter->strokePath(m_cachedSinglePath, pen);
            return;
        }

        const auto &pts = m_smoothedPoints.isEmpty() ? m_points : m_smoothedPoints;
        const int n = pts.size();
        if (n == 0) return;

        if (n == 1) {
            qreal pr = qBound(0.0, pts[0].pressure, 1.0);
            qreal r = qMax(0.6, m_baseWidth * fastCanvaPressureFactor(pr) * 0.5);
            painter->setPen(Qt::NoPen);
            painter->setBrush(m_color);
            painter->drawEllipse(pts[0].pos, r, r);
        } else if (n == 2) {
            qreal pAvg = (pts[0].pressure + pts[1].pressure) * 0.5;
            qreal w = qMax(0.6, m_baseWidth * fastCanvaPressureFactor(pAvg));
            QPen pen(m_color, w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawLine(pts[0].pos, pts[1].pos);
        } else {
            // Live drawing fallback during active stroke input
            qreal minP = 1.0, maxP = 0.0, sumP = 0.0;
            for (const auto &p : pts) {
                if (p.pressure < minP) minP = p.pressure;
                if (p.pressure > maxP) maxP = p.pressure;
                sumP += p.pressure;
            }
            qreal avgP = sumP / static_cast<qreal>(n);

            // Cumulative distance along the stroke for natural stroke entry/exit tapering
            qreal totalLength = 0.0;
            QVector<qreal> cumDist;
            cumDist.reserve(n);
            cumDist.append(0.0);
            for (int i = 0; i < n - 1; ++i) {
                totalLength += std::hypot(pts[i + 1].pos.x() - pts[i].pos.x(),
                                          pts[i + 1].pos.y() - pts[i].pos.y());
                cumDist.append(totalLength);
            }
            const qreal taperWindow = qMin(8.0, totalLength * 0.20);

            if ((maxP - minP <= 0.10) && (totalLength <= 14.0 || taperWindow <= 0.5)) {
                // Highly uniform or short stroke: single continuous Bezier path
                QPainterPath path;
                path.moveTo(pts[0].pos);
                for (int i = 1; i < n - 1; ++i) {
                    QPointF mid = (pts[i].pos + pts[i + 1].pos) * 0.5;
                    path.quadTo(pts[i].pos, mid);
                }
                path.lineTo(pts.last().pos);

                qreal w = qMax(0.6, m_baseWidth * fastCanvaPressureFactor(avgP));
                QPen pen(m_color, w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
                painter->setPen(pen);
                painter->setBrush(Qt::NoBrush);
                painter->strokePath(path, pen);
            } else {
                // Dynamic pressure and natural tapering: smooth quadratic Bezier spans
                painter->setBrush(Qt::NoBrush);
                QPen segPen(m_color, m_baseWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

                // Start segment: from pts[0] to first midpoint
                QPointF mid0 = (pts[0].pos + pts[1].pos) * 0.5;
                qreal p0 = pts[0].pressure;
                qreal taper0 = (totalLength > 3.0 && taperWindow > 0.5) ? 0.88 : 1.0;
                segPen.setWidthF(qMax(0.6, m_baseWidth * fastCanvaPressureFactor(p0) * taper0));
                painter->setPen(segPen);
                painter->drawLine(pts[0].pos, mid0);

                // Intermediate quadratic Bezier spans between consecutive midpoints
                QPointF prevMid = mid0;
                for (int i = 1; i < n - 1; ++i) {
                    QPointF nextMid = (pts[i].pos + pts[i + 1].pos) * 0.5;
                    qreal segDist = cumDist[i];
                    qreal taper = 1.0;
                    if (totalLength > 3.0 && taperWindow > 0.5) {
                        if (segDist < taperWindow) {
                            qreal u = segDist / taperWindow;
                            taper = 0.88 + 0.12 * (3.0 * u * u - 2.0 * u * u * u);
                        } else if (totalLength - segDist < taperWindow) {
                            qreal u = (totalLength - segDist) / taperWindow;
                            taper = 0.88 + 0.12 * (3.0 * u * u - 2.0 * u * u * u);
                        }
                    }
                    qreal segW = qMax(0.6, m_baseWidth * fastCanvaPressureFactor(pts[i].pressure) * taper);
                    segPen.setWidthF(segW);
                    painter->setPen(segPen);

                    QPainterPath quadSpan;
                    quadSpan.moveTo(prevMid);
                    quadSpan.quadTo(pts[i].pos, nextMid);
                    painter->strokePath(quadSpan, segPen);

                    prevMid = nextMid;
                }

                // Final segment: from last midpoint to pts[n - 1]
                qreal pn = pts[n - 1].pressure;
                qreal tapern = (totalLength > 3.0 && taperWindow > 0.5) ? 0.88 : 1.0;
                segPen.setWidthF(qMax(0.6, m_baseWidth * fastCanvaPressureFactor(pn) * tapern));
                painter->setPen(segPen);
                painter->drawLine(prevMid, pts[n - 1].pos);
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

void StrokeItem::setGroupId(qint64 id) {
    m_groupId = id;
    setData(UserRoleGroupId, id > 0 ? QVariant(id) : QVariant());
}

QByteArray StrokeItem::serialize() const {
    QByteArray uncompressed;
    QDataStream stream(&uncompressed, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);

    const quint32 magic = 0x4F4D4E49; // "OMNI"
    const quint16 version = 2;

    stream << magic << version;
    stream << static_cast<qint32>(m_toolType);
    stream << m_color.rgba();
    stream << m_baseWidth;
    stream << zValue();
    stream << m_groupId;

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

    if (magic != 0x4F4D4E49 || (version != 1 && version != 2)) {
        return nullptr;
    }

    qint32 rawToolType;
    QRgb colorRgba;
    qreal baseWidth;
    qreal zVal;
    qint64 groupId = 0;
    qint32 count;

    if (version == 1) {
        stream >> rawToolType >> colorRgba >> baseWidth >> zVal >> count;
    } else { // version 2
        stream >> rawToolType >> colorRgba >> baseWidth >> zVal >> groupId >> count;
    }

    auto *item = new StrokeItem(static_cast<ToolType>(rawToolType), QColor::fromRgba(colorRgba), baseWidth);
    item->setZValue(zVal);
    item->setGroupId(groupId);

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
