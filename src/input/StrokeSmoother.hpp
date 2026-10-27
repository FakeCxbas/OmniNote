#pragma once

#include <QPointF>
#include <QVector>
#include <QPolygonF>
#include <QPainterPath>
#include <QtGlobal>
#include <cmath>

namespace OmniNote {

struct StrokePoint {
    QPointF pos;
    qreal pressure{0.5};
    qreal tiltX{0.0};
    qreal tiltY{0.0};
    qint64 timestamp{0};

    StrokePoint() = default;
    StrokePoint(const QPointF &p, qreal pr = 0.5, qreal tx = 0.0, qreal ty = 0.0, qint64 ts = 0)
        : pos(p), pressure(pr), tiltX(tx), tiltY(ty), timestamp(ts) {}
};

class StrokeSmoother {
public:
    StrokeSmoother();
    ~StrokeSmoother() = default;

    // Real-time jitter filter for incoming points
    void reset();
    StrokePoint filterPoint(const StrokePoint &rawPoint);

    // Mathematical Catmull-Rom spline interpolation
    static QVector<StrokePoint> interpolateCatmullRom(const QVector<StrokePoint> &controlPoints, int subdivisionsPerSegment = 8);

    // Vector ribbon polygon generation with variable width and rounded caps
    static QPolygonF generateRibbonPolygon(const QVector<StrokePoint> &points, qreal baseWidth, qreal minPressureRatio = 0.2);

    // Stroke splitting for partial/split eraser
    static QVector<QVector<StrokePoint>> splitStroke(const QVector<StrokePoint> &points, const QPointF &eraserPos, qreal eraserRadius);

    // Spatial low-pass stabilizer for hand tremor suppression
    static QVector<StrokePoint> stabilizePoints(const QVector<StrokePoint> &points, int passes = 1);

    // Stroke Beautification & Tremor Elimination (Canva-style Auto-Improvement)
    static QVector<StrokePoint> beautifyStroke(const QVector<StrokePoint> &points, bool detectShapes = true);
    static QVector<StrokePoint> simplifyRDP(const QVector<StrokePoint> &points, qreal epsilon = 1.1);
    static bool trySnapToGeometricShape(const QVector<StrokePoint> &points, QVector<StrokePoint> &outSnapped);

    // Configuration for filter
    void setAlpha(qreal alpha) { m_alpha = qBound(0.05, alpha, 1.0); }
    qreal alpha() const { return m_alpha; }

private:
    bool m_hasPrevious{false};
    QPointF m_filteredPos;
    qreal m_filteredPressure{0.5};
    qreal m_alpha{0.68}; // Filter responsiveness weight
};

} // namespace OmniNote
