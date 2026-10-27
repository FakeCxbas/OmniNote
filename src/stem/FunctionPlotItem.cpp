#include "FunctionPlotItem.hpp"
#include "../app/AppIcons.hpp"
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsScene>
#include <QCursor>
#include <cmath>
#include <algorithm>

namespace OmniNote {

FunctionPlotItem::FunctionPlotItem(const QString &expression, double xMin, double xMax, double yMin, double yMax, QGraphicsItem *parent)
    : QGraphicsItem(parent), m_expression(expression), m_xMin(xMin), m_xMax(xMax), m_yMin(yMin), m_yMax(yMax) {
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    setCacheMode(QGraphicsItem::DeviceCoordinateCache);

    m_parser.parse(m_expression.toStdString());
    updatePlotPath();
}

QRectF FunctionPlotItem::boundingRect() const {
    return QRectF(0, 0, m_width, m_height);
}

void FunctionPlotItem::setExpression(const QString &expr) {
    m_expression = expr;
    m_parser.parse(m_expression.toStdString());
    updatePlotPath();
    update();
}

void FunctionPlotItem::setDomain(double xMin, double xMax) {
    if (xMin >= xMax) return;
    m_xMin = xMin;
    m_xMax = xMax;
    updatePlotPath();
    update();
}

void FunctionPlotItem::setRange(double yMin, double yMax) {
    if (yMin >= yMax) return;
    m_yMin = yMin;
    m_yMax = yMax;
    updatePlotPath();
    update();
}

void FunctionPlotItem::setPlotSize(qreal w, qreal h) {
    m_width = qMax(260.0, w);
    m_height = qMax(180.0, h);
    updatePlotPath();
    update();
}

QPointF FunctionPlotItem::mathToScreen(double x, double y) const {
    qreal plotW = m_width - 40.0;
    qreal plotH = m_height - m_headerHeight - 30.0;

    qreal sx = 25.0 + ((x - m_xMin) / (m_xMax - m_xMin)) * plotW;
    qreal sy = m_headerHeight + 10.0 + ((m_yMax - y) / (m_yMax - m_yMin)) * plotH;

    return QPointF(sx, sy);
}

double FunctionPlotItem::calculateTickStep(double minVal, double maxVal, int targetTicks) const {
    double span = maxVal - minVal;
    if (span <= 0.0) return 1.0;
    double rawStep = span / targetTicks;
    double mag = std::pow(10.0, std::floor(std::log10(rawStep)));
    double residual = rawStep / mag;

    if (residual < 1.5) return 1.0 * mag;
    if (residual < 3.0) return 2.0 * mag;
    if (residual < 7.0) return 5.0 * mag;
    return 10.0 * mag;
}

void FunctionPlotItem::updatePlotPath() {
    m_plotPaths.clear();

    if (!m_parser.isValid()) {
        return;
    }

    const int sampleCount = 600;
    const double step = (m_xMax - m_xMin) / static_cast<double>(sampleCount);
    const double maxJump = (m_yMax - m_yMin) * 0.8;

    QPainterPath currentSegment;
    bool segmentActive = false;
    double prevY = 0.0;

    for (int i = 0; i <= sampleCount; ++i) {
        double x = m_xMin + i * step;
        double y = m_parser.evaluate(x);

        if (std::isnan(y) || std::isinf(y)) {
            if (segmentActive) {
                m_plotPaths.append(currentSegment);
                currentSegment = QPainterPath();
                segmentActive = false;
            }
            continue;
        }

        // Asymptote discontinuity check (e.g. 1/x, tan(x))
        if (segmentActive) {
            double diff = std::abs(y - prevY);
            if (diff > maxJump && (y * prevY < 0.0)) {
                m_plotPaths.append(currentSegment);
                currentSegment = QPainterPath();
                segmentActive = false;
            }
        }

        QPointF pt = mathToScreen(x, y);

        if (!segmentActive) {
            currentSegment.moveTo(pt);
            segmentActive = true;
        } else {
            currentSegment.lineTo(pt);
        }

        prevY = y;
    }

    if (segmentActive && !currentSegment.isEmpty()) {
        m_plotPaths.append(currentSegment);
    }
}

void FunctionPlotItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    painter->setRenderHint(QPainter::Antialiasing, true);

    // Card background
    QRectF cardRect(0, 0, m_width, m_height);
    painter->setPen(isSelected() ? QPen(QColor("#7aa2f7"), 2.0) : QPen(QColor("#3b4261"), 1.0));
    painter->setBrush(QColor("#16161e"));
    painter->drawRoundedRect(cardRect, 6.0, 6.0);

    // Header bar
    QRectF headerRect(0, 0, m_width, m_headerHeight);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor("#1f2335"));
    QPainterPath headerPath;
    headerPath.moveTo(0, m_headerHeight);
    headerPath.lineTo(0, 6);
    headerPath.arcTo(0, 0, 12, 12, 180, -90);
    headerPath.lineTo(m_width - 6, 0);
    headerPath.arcTo(m_width - 12, 0, 12, 12, 90, -90);
    headerPath.lineTo(m_width, m_headerHeight);
    headerPath.closeSubpath();
    painter->drawPath(headerPath);

    // Header text: f(x) = ...
    painter->setPen(QColor("#c0caf5"));
    QFont headerFont("Fira Code", 9, QFont::Bold);
    painter->setFont(headerFont);
    QString title = QString("f(x) = %1").arg(m_expression);
    painter->drawText(QRectF(10, 0, m_width - 50, m_headerHeight), Qt::AlignLeft | Qt::AlignVCenter, title);

    // Delete button in header
    QPixmap closePix = AppIcons::getPixmap(AppIcons::IconType::Close, QColor("#f7768e"), 12);
    painter->drawPixmap(m_width - 20, (m_headerHeight - 12) / 2, closePix);

    // Plot clipping area
    QRectF plotArea(20, m_headerHeight + 5, m_width - 30, m_height - m_headerHeight - 20);
    painter->save();
    painter->setClipRect(plotArea);

    // Draw Grid Lines
    double xStep = calculateTickStep(m_xMin, m_xMax, 8);
    double yStep = calculateTickStep(m_yMin, m_yMax, 6);

    painter->setPen(QPen(QColor("#24283b"), 1.0, Qt::DashLine));
    double firstX = std::ceil(m_xMin / xStep) * xStep;
    for (double gx = firstX; gx <= m_xMax; gx += xStep) {
        QPointF pTop = mathToScreen(gx, m_yMax);
        QPointF pBottom = mathToScreen(gx, m_yMin);
        painter->drawLine(pTop, pBottom);
    }

    double firstY = std::ceil(m_yMin / yStep) * yStep;
    for (double gy = firstY; gy <= m_yMax; gy += yStep) {
        QPointF pLeft = mathToScreen(m_xMin, gy);
        QPointF pRight = mathToScreen(m_xMax, gy);
        painter->drawLine(pLeft, pRight);
    }

    // Draw Cartesian Axes
    painter->setPen(QPen(QColor("#7aa2f7"), 1.5));
    // X-Axis (y = 0)
    if (m_yMin <= 0.0 && m_yMax >= 0.0) {
        QPointF pLeft = mathToScreen(m_xMin, 0.0);
        QPointF pRight = mathToScreen(m_xMax, 0.0);
        painter->drawLine(pLeft, pRight);
        // Arrowhead for +X
        painter->drawLine(pRight, pRight + QPointF(-6, -4));
        painter->drawLine(pRight, pRight + QPointF(-6, 4));
    }
    // Y-Axis (x = 0)
    if (m_xMin <= 0.0 && m_xMax >= 0.0) {
        QPointF pBottom = mathToScreen(0.0, m_yMin);
        QPointF pTop = mathToScreen(0.0, m_yMax);
        painter->drawLine(pBottom, pTop);
        // Arrowhead for +Y
        painter->drawLine(pTop, pTop + QPointF(-4, 6));
        painter->drawLine(pTop, pTop + QPointF(4, 6));
    }

    // Draw Function Curve
    painter->setPen(QPen(QColor("#bb9af7"), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    for (const auto &path : m_plotPaths) {
        painter->drawPath(path);
    }

    painter->restore();

    // Axis Labels and Ticks (rendered outside clip)
    painter->setPen(QColor("#565f89"));
    QFont tickFont("JetBrains Mono", 7);
    painter->setFont(tickFont);

    for (double gx = firstX; gx <= m_xMax; gx += xStep) {
        if (std::abs(gx) < 1e-9) continue;
        QPointF pt = mathToScreen(gx, (m_yMin <= 0.0 && m_yMax >= 0.0) ? 0.0 : m_yMin);
        painter->drawLine(QPointF(pt.x(), pt.y() - 3), QPointF(pt.x(), pt.y() + 3));
        painter->drawText(QRectF(pt.x() - 20, pt.y() + 4, 40, 14), Qt::AlignCenter, QString::number(gx, 'g', 3));
    }

    for (double gy = firstY; gy <= m_yMax; gy += yStep) {
        if (std::abs(gy) < 1e-9) continue;
        QPointF pt = mathToScreen((m_xMin <= 0.0 && m_xMax >= 0.0) ? 0.0 : m_xMin, gy);
        painter->drawLine(QPointF(pt.x() - 3, pt.y()), QPointF(pt.x() + 3, pt.y()));
        painter->drawText(QRectF(pt.x() - 36, pt.y() - 7, 32, 14), Qt::AlignRight | Qt::AlignVCenter, QString::number(gy, 'g', 3));
    }

    // Resize grip in bottom-right corner
    painter->setPen(QColor("#7aa2f7"));
    painter->drawLine(m_width - 12, m_height - 4, m_width - 4, m_height - 12);
    painter->drawLine(m_width - 8, m_height - 4, m_width - 4, m_height - 8);
    painter->drawLine(m_width - 4, m_height - 4, m_width - 4, m_height - 4);
}

void FunctionPlotItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (event->pos().x() >= m_width - 24 && event->pos().y() <= m_headerHeight) {
        if (scene()) {
            scene()->removeItem(this);
            delete this;
            return;
        }
    }

    if (event->pos().x() >= m_width - 16 && event->pos().y() >= m_height - 16) {
        m_resizing = true;
        m_resizeStartPos = event->scenePos();
        m_resizeStartSize = QSizeF(m_width, m_height);
        event->accept();
        return;
    }

    QGraphicsItem::mousePressEvent(event);
}

void FunctionPlotItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    if (m_resizing) {
        QPointF delta = event->scenePos() - m_resizeStartPos;
        setPlotSize(m_resizeStartSize.width() + delta.x(), m_resizeStartSize.height() + delta.y());
        event->accept();
        return;
    }
    QGraphicsItem::mouseMoveEvent(event);
}

void FunctionPlotItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    if (m_resizing) {
        m_resizing = false;
        event->accept();
        return;
    }
    QGraphicsItem::mouseReleaseEvent(event);
}

void FunctionPlotItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    if (event->pos().x() >= m_width - 16 && event->pos().y() >= m_height - 16) {
        setCursor(Qt::SizeFDiagCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
    QGraphicsItem::hoverMoveEvent(event);
}

QString FunctionPlotItem::serialize() const {
    QJsonObject obj;
    obj["x"] = pos().x();
    obj["y"] = pos().y();
    obj["width"] = m_width;
    obj["height"] = m_height;
    obj["expression"] = m_expression;
    obj["xMin"] = m_xMin;
    obj["xMax"] = m_xMax;
    obj["yMin"] = m_yMin;
    obj["yMax"] = m_yMax;

    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void FunctionPlotItem::deserialize(const QString &data) {
    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
    if (!doc.isObject()) return;
    QJsonObject obj = doc.object();

    if (obj.contains("x") && obj.contains("y")) {
        setPos(obj["x"].toDouble(), obj["y"].toDouble());
    }
    if (obj.contains("width") && obj.contains("height")) {
        setPlotSize(obj["width"].toDouble(), obj["height"].toDouble());
    }
    if (obj.contains("xMin") && obj.contains("xMax")) {
        m_xMin = obj["xMin"].toDouble();
        m_xMax = obj["xMax"].toDouble();
    }
    if (obj.contains("yMin") && obj.contains("yMax")) {
        m_yMin = obj["yMin"].toDouble();
        m_yMax = obj["yMax"].toDouble();
    }
    if (obj.contains("expression")) {
        setExpression(obj["expression"].toString());
    }
}

} // namespace OmniNote
