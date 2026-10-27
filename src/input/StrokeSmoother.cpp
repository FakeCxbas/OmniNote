#include "StrokeSmoother.hpp"
#include <QtMath>
#include <algorithm>

namespace OmniNote {

StrokeSmoother::StrokeSmoother()
    : m_hasPrevious(false), m_filteredPos(0.0, 0.0), m_filteredPressure(0.5), m_alpha(0.68) {
}

void StrokeSmoother::reset() {
    m_hasPrevious = false;
    m_filteredPos = QPointF(0.0, 0.0);
    m_filteredPressure = 0.5;
}

StrokePoint StrokeSmoother::filterPoint(const StrokePoint &rawPoint) {
    if (!m_hasPrevious) {
        m_hasPrevious = true;
        m_filteredPos = rawPoint.pos;
        m_filteredPressure = rawPoint.pressure;
        return rawPoint;
    }

    // Displacement from previous filtered coordinate
    qreal dx = rawPoint.pos.x() - m_filteredPos.x();
    qreal dy = rawPoint.pos.y() - m_filteredPos.y();
    qreal dist = std::hypot(dx, dy);

    // Adaptive alpha: strong smoothing on slow/subtle hand tremor, fast following on sweeping moves
    // Enhanced responsiveness on small movements to preserve fine contours of exponents and small numbers
    qreal adaptiveAlpha = m_alpha;
    if (dist < 4.0) {
        adaptiveAlpha = qBound(0.65, m_alpha * (0.85 + 0.15 * (dist / 4.0)), 0.88);
    } else if (dist > 15.0) {
        // Fast cursive sweeps: zero-drag responsiveness
        adaptiveAlpha = qMin(1.0, m_alpha + 0.20);
    }

    m_filteredPos.setX(m_filteredPos.x() + adaptiveAlpha * (rawPoint.pos.x() - m_filteredPos.x()));
    m_filteredPos.setY(m_filteredPos.y() + adaptiveAlpha * (rawPoint.pos.y() - m_filteredPos.y()));
    m_filteredPressure += adaptiveAlpha * (rawPoint.pressure - m_filteredPressure);

    return StrokePoint(m_filteredPos, m_filteredPressure, rawPoint.tiltX, rawPoint.tiltY, rawPoint.timestamp);
}

QVector<StrokePoint> StrokeSmoother::stabilizePoints(const QVector<StrokePoint> &pts, int passes) {
    const int n = pts.size();
    if (n <= 3) {
        return pts;
    }

    QVector<StrokePoint> cur = pts;
    for (int p = 0; p < passes; ++p) {
        QVector<StrokePoint> next = cur;
        for (int i = 1; i < n - 1; ++i) {
            if (i == 1 || i == n - 2) {
                // 3-point Gaussian kernel for boundary transitions
                next[i].pos = 0.25 * cur[i - 1].pos + 0.5 * cur[i].pos + 0.25 * cur[i + 1].pos;
                next[i].pressure = 0.25 * cur[i - 1].pressure + 0.5 * cur[i].pressure + 0.25 * cur[i + 1].pressure;
            } else {
                // 5-point binomial kernel [1, 4, 6, 4, 1] / 16 (ideal spatial low-pass filter)
                next[i].pos = (1.0 * cur[i - 2].pos + 4.0 * cur[i - 1].pos + 6.0 * cur[i].pos + 4.0 * cur[i + 1].pos + 1.0 * cur[i + 2].pos) / 16.0;
                next[i].pressure = (1.0 * cur[i - 2].pressure + 4.0 * cur[i - 1].pressure + 6.0 * cur[i].pressure + 4.0 * cur[i + 1].pressure + 1.0 * cur[i + 2].pressure) / 16.0;
            }
        }
        cur = next;
    }
    return cur;
}

QVector<StrokePoint> StrokeSmoother::interpolateCatmullRom(const QVector<StrokePoint> &pts, int subdivisions) {
    const int n = pts.size();
    if (n < 2) {
        return pts;
    }

    // Apply 5-point spatial stabilizer to suppress hand tremor while preserving true handwriting geometry
    const QVector<StrokePoint> smoothPts = (n >= 4) ? stabilizePoints(pts, 1) : pts;

    QVector<StrokePoint> result;
    result.reserve((n - 1) * qMax(subdivisions, 6) + 1);

    for (int i = 0; i < n - 1; ++i) {
        const QPointF &p1 = smoothPts[i].pos;
        const QPointF &p2 = smoothPts[i + 1].pos;

        // Ghost endpoint extrapolation along the natural segment tangent
        const QPointF p0 = (i > 0) ? smoothPts[i - 1].pos : (p1 + (p1 - p2));
        const QPointF p3 = (i + 2 < n) ? smoothPts[i + 2].pos : (p2 + (p2 - p1));

        auto getT = [](qreal t, const QPointF &a, const QPointF &b) {
            qreal d = std::hypot(b.x() - a.x(), b.y() - a.y());
            return t + qMax(1e-4, std::sqrt(d)); // Centripetal parameterization (alpha = 0.5)
        };

        const qreal t0 = 0.0;
        const qreal t1 = getT(t0, p0, p1);
        const qreal t2 = getT(t1, p1, p2);
        const qreal t3 = getT(t2, p2, p3);

        const qreal segLen = std::hypot(p2.x() - p1.x(), p2.y() - p1.y());
        // Adaptive step calculation: ensure maximum distance between points is <= 2.0 pixels
        // for buttery smooth antialiased curves without visible polygonal facets
        int steps = qMax(subdivisions, static_cast<int>(std::ceil(segLen / 2.0)));
        steps = qBound(subdivisions, steps, 28);

        for (int s = 0; s < steps; ++s) {
            const qreal u = static_cast<qreal>(s) / static_cast<qreal>(steps);
            const qreal t = t1 + u * (t2 - t1);

            const QPointF a1 = (t1 - t) / (t1 - t0) * p0 + (t - t0) / (t1 - t0) * p1;
            const QPointF a2 = (t2 - t) / (t2 - t1) * p1 + (t - t1) / (t2 - t1) * p2;
            const QPointF a3 = (t3 - t) / (t3 - t2) * p2 + (t - t2) / (t3 - t2) * p3;

            const QPointF b1 = (t2 - t) / (t2 - t0) * a1 + (t - t0) / (t2 - t0) * a2;
            const QPointF b2 = (t3 - t) / (t3 - t1) * a2 + (t - t1) / (t3 - t1) * a3;

            const QPointF c = (t2 - t) / (t2 - t1) * b1 + (t - t1) / (t2 - t1) * b2;

            const qreal pr = qBound(0.0, (1.0 - u) * smoothPts[i].pressure + u * smoothPts[i + 1].pressure, 1.0);
            const qreal tx = (1.0 - u) * smoothPts[i].tiltX + u * smoothPts[i + 1].tiltX;
            const qreal ty = (1.0 - u) * smoothPts[i].tiltY + u * smoothPts[i + 1].tiltY;
            const qint64 ts = smoothPts[i].timestamp + static_cast<qint64>(u * (smoothPts[i + 1].timestamp - smoothPts[i].timestamp));

            result.append(StrokePoint(c, pr, tx, ty, ts));
        }
    }
    result.append(smoothPts.last());
    return result;
}

QPolygonF StrokeSmoother::generateRibbonPolygon(const QVector<StrokePoint> &points, qreal baseWidth, qreal minPressureRatio) {
    if (points.isEmpty()) {
        return QPolygonF();
    }

    if (points.size() == 1) {
        // Draw round dot for single tap
        const auto &pt = points.first();
        const qreal radius = (baseWidth * (minPressureRatio + (1.0 - minPressureRatio) * pt.pressure)) * 0.5;
        QPolygonF circlePoly;
        const int steps = 16;
        for (int i = 0; i < steps; ++i) {
            qreal angle = (2.0 * M_PI * i) / steps;
            circlePoly << QPointF(pt.pos.x() + radius * std::cos(angle), pt.pos.y() + radius * std::sin(angle));
        }
        return circlePoly;
    }

    const int n = points.size();
    QVector<QPointF> leftEdge;
    QVector<QPointF> rightEdge;
    leftEdge.reserve(n);
    rightEdge.reserve(n);

    QVector<QPointF> normals(n);
    QVector<qreal> halfWidths(n);

    for (int i = 0; i < n; ++i) {
        QPointF tangent;
        if (i == 0) {
            tangent = points[1].pos - points[0].pos;
        } else if (i == n - 1) {
            tangent = points[n - 1].pos - points[n - 2].pos;
        } else {
            tangent = points[i + 1].pos - points[i - 1].pos;
        }

        qreal len = std::hypot(tangent.x(), tangent.y());
        if (len < 1e-5) {
            normals[i] = (i > 0) ? normals[i - 1] : QPointF(0.0, 1.0);
        } else {
            // Perpendicular normal: (-dy, dx) normalized
            normals[i] = QPointF(-tangent.y() / len, tangent.x() / len);
        }

        qreal p = qBound(0.0, points[i].pressure, 1.0);
        halfWidths[i] = qMax(0.5, (baseWidth * (minPressureRatio + (1.0 - minPressureRatio) * p)) * 0.5);
    }

    for (int i = 0; i < n; ++i) {
        leftEdge.append(points[i].pos + normals[i] * halfWidths[i]);
        rightEdge.append(points[i].pos - normals[i] * halfWidths[i]);
    }

    QPolygonF ribbon;
    ribbon.reserve(n * 2 + 32);

    // Start cap (semicircle around points[0])
    const int capSteps = 8;
    const QPointF startNormal = normals[0];
    const QPointF startTangent(-startNormal.y(), startNormal.x());
    const qreal r0 = halfWidths[0];

    for (int s = 0; s <= capSteps; ++s) {
        qreal angle = M_PI * 0.5 + (M_PI * s) / capSteps;
        QPointF offset = (startNormal * std::sin(angle) + startTangent * std::cos(angle)) * r0;
        ribbon << (points[0].pos + offset);
    }

    // Left edge moving forward
    for (int i = 0; i < n; ++i) {
        ribbon << leftEdge[i];
    }

    // End cap (semicircle around points[n - 1])
    const QPointF endNormal = normals[n - 1];
    const QPointF endTangent(-endNormal.y(), endNormal.x());
    const qreal rn = halfWidths[n - 1];

    for (int s = 0; s <= capSteps; ++s) {
        qreal angle = -M_PI * 0.5 + (M_PI * s) / capSteps;
        QPointF offset = (endNormal * std::sin(angle) + endTangent * std::cos(angle)) * rn;
        ribbon << (points[n - 1].pos + offset);
    }

    // Right edge moving backwards
    for (int i = n - 1; i >= 0; --i) {
        ribbon << rightEdge[i];
    }

    return ribbon;
}

QVector<QVector<StrokePoint>> StrokeSmoother::splitStroke(const QVector<StrokePoint> &points,
                                                          const QPointF &eraserPos,
                                                          qreal eraserRadius) {
    QVector<QVector<StrokePoint>> segments;
    QVector<StrokePoint> currentSegment;
    const qreal radiusSq = eraserRadius * eraserRadius;

    for (const auto &pt : points) {
        qreal dx = pt.pos.x() - eraserPos.x();
        qreal dy = pt.pos.y() - eraserPos.y();
        bool insideEraser = (dx * dx + dy * dy) <= radiusSq;

        if (!insideEraser) {
            currentSegment.append(pt);
        } else {
            if (currentSegment.size() >= 2) {
                segments.append(currentSegment);
            }
            currentSegment.clear();
        }
    }

    if (currentSegment.size() >= 2) {
        segments.append(currentSegment);
    }

    return segments;
}

static qreal distanceToSegment(const QPointF &p, const QPointF &a, const QPointF &b) {
    qreal dx = b.x() - a.x();
    qreal dy = b.y() - a.y();
    qreal lenSq = dx * dx + dy * dy;
    if (lenSq < 1e-6) {
        return std::hypot(p.x() - a.x(), p.y() - a.y());
    }
    qreal t = qBound(0.0, ((p.x() - a.x()) * dx + (p.y() - a.y()) * dy) / lenSq, 1.0);
    QPointF proj(a.x() + t * dx, a.y() + t * dy);
    return std::hypot(p.x() - proj.x(), p.y() - proj.y());
}

static void rdpRecursive(const QVector<StrokePoint> &pts, int start, int end, qreal epsilon, QVector<bool> &keep) {
    if (end <= start + 1) return;
    qreal maxDist = 0.0;
    int index = start;
    const QPointF &a = pts[start].pos;
    const QPointF &b = pts[end].pos;

    for (int i = start + 1; i < end; ++i) {
        qreal d = distanceToSegment(pts[i].pos, a, b);
        if (d > maxDist) {
            maxDist = d;
            index = i;
        }
    }

    if (maxDist > epsilon) {
        keep[index] = true;
        rdpRecursive(pts, start, index, epsilon, keep);
        rdpRecursive(pts, index, end, epsilon, keep);
    }
}

QVector<StrokePoint> StrokeSmoother::simplifyRDP(const QVector<StrokePoint> &pts, qreal epsilon) {
    if (pts.size() <= 2) return pts;
    QVector<bool> keep(pts.size(), false);
    keep.first() = true;
    keep.last() = true;
    rdpRecursive(pts, 0, pts.size() - 1, epsilon, keep);

    QVector<StrokePoint> result;
    result.reserve(pts.size());
    for (int i = 0; i < pts.size(); ++i) {
        if (keep[i]) result.append(pts[i]);
    }
    return result;
}

bool StrokeSmoother::trySnapToGeometricShape(const QVector<StrokePoint> &pts, QVector<StrokePoint> &outSnapped) {
    const int n = pts.size();
    if (n < 4) return false;

    // Calculate bounding box, arc length, and chord length
    qreal minX = pts[0].pos.x(), maxX = pts[0].pos.x();
    qreal minY = pts[0].pos.y(), maxY = pts[0].pos.y();
    qreal arcLength = 0.0;
    for (int i = 0; i < n; ++i) {
        if (pts[i].pos.x() < minX) minX = pts[i].pos.x();
        if (pts[i].pos.x() > maxX) maxX = pts[i].pos.x();
        if (pts[i].pos.y() < minY) minY = pts[i].pos.y();
        if (pts[i].pos.y() > maxY) maxY = pts[i].pos.y();
        if (i > 0) {
            arcLength += std::hypot(pts[i].pos.x() - pts[i-1].pos.x(), pts[i].pos.y() - pts[i-1].pos.y());
        }
    }
    const qreal bbW = maxX - minX;
    const qreal bbH = maxY - minY;
    const qreal diag = std::hypot(bbW, bbH);
    const qreal chordLength = std::hypot(pts.last().pos.x() - pts.first().pos.x(), pts.last().pos.y() - pts.first().pos.y());

    // Small handwriting guard: never snap small strokes (exponents, subscripts, dots, tiny numbers)
    if (diag < 40.0 || arcLength < 45.0) {
        return false;
    }

    if (arcLength < 1.0) return false;
    const qreal straightness = chordLength / arcLength;

    // 1. Straight Line Snapping (Underlines, division/fraction bars, axes)
    // Needs sufficient length (>= 50px), high straightness (>= 0.94), and low max deviation
    if (chordLength >= 50.0 && straightness >= 0.94) {
        qreal maxDev = 0.0;
        const QPointF &pStart = pts.first().pos;
        const QPointF &pEnd = pts.last().pos;
        for (int i = 1; i < n - 1; ++i) {
            qreal dev = distanceToSegment(pts[i].pos, pStart, pEnd);
            if (dev > maxDev) maxDev = dev;
        }

        if (maxDev <= 12.0 && (maxDev / chordLength) <= 0.06) {
            int subs = qMax(4, qMin(32, static_cast<int>(chordLength / 10.0)));
            outSnapped.clear();
            outSnapped.reserve(subs + 1);
            for (int i = 0; i <= subs; ++i) {
                qreal t = static_cast<qreal>(i) / subs;
                QPointF pos = pStart + t * (pEnd - pStart);
                qreal pressure = pts.first().pressure + t * (pts.last().pressure - pts.first().pressure);
                outSnapped.append(StrokePoint(pos, pressure, 0.0, 0.0, pts.first().timestamp));
            }
            return true;
        }
    }

    // 2. Circle / Ellipse Snapping
    // Require substantial bounding size (>= 45px) so small handwritten symbols ('0', '8', 'o', '6') are never snapped
    qreal endsGap = std::hypot(pts.last().pos.x() - pts.first().pos.x(), pts.last().pos.y() - pts.first().pos.y());
    if (n >= 16 && bbW >= 45.0 && bbH >= 45.0 && endsGap <= 0.25 * diag) {
        qreal sumX = 0.0, sumY = 0.0;
        for (const auto &p : pts) {
            sumX += p.pos.x();
            sumY += p.pos.y();
        }
        QPointF center(sumX / n, sumY / n);
        qreal a = bbW * 0.5;
        qreal b = bbH * 0.5;
        if (a > 10.0 && b > 10.0) {
            qreal totalErr = 0.0;
            for (const auto &p : pts) {
                qreal nx = (p.pos.x() - center.x()) / a;
                qreal ny = (p.pos.y() - center.y()) / b;
                totalErr += std::abs(std::hypot(nx, ny) - 1.0);
            }
            qreal avgErr = totalErr / n;
            if (avgErr <= 0.15) {
                int steps = 48;
                outSnapped.clear();
                outSnapped.reserve(steps + 1);
                qreal avgPressure = 0.5;
                for (const auto &p : pts) avgPressure += p.pressure;
                avgPressure /= (n + 1);

                for (int i = 0; i <= steps; ++i) {
                    qreal angle = 2.0 * M_PI * (static_cast<qreal>(i) / steps);
                    QPointF pos(center.x() + a * std::cos(angle), center.y() + b * std::sin(angle));
                    outSnapped.append(StrokePoint(pos, avgPressure, 0.0, 0.0, pts.first().timestamp));
                }
                return true;
            }
        }
    }

    return false;
}

QVector<StrokePoint> StrokeSmoother::beautifyStroke(const QVector<StrokePoint> &pts, bool detectShapes) {
    if (pts.size() <= 2) return pts;

    // Calculate dimensions
    qreal minX = pts[0].pos.x(), maxX = pts[0].pos.x();
    qreal minY = pts[0].pos.y(), maxY = pts[0].pos.y();
    qreal arcLength = 0.0;
    for (int i = 0; i < pts.size(); ++i) {
        if (pts[i].pos.x() < minX) minX = pts[i].pos.x();
        if (pts[i].pos.x() > maxX) maxX = pts[i].pos.x();
        if (pts[i].pos.y() < minY) minY = pts[i].pos.y();
        if (pts[i].pos.y() > maxY) maxY = pts[i].pos.y();
        if (i > 0) {
            arcLength += std::hypot(pts[i].pos.x() - pts[i-1].pos.x(), pts[i].pos.y() - pts[i-1].pos.y());
        }
    }
    const qreal bbW = maxX - minX;
    const qreal bbH = maxY - minY;
    const qreal diag = std::hypot(bbW, bbH);

    // Step 1: Geometric shape snapping (Canva QuickShape) - only for large intentional shapes
    if (detectShapes && diag >= 40.0 && arcLength >= 45.0) {
        QVector<StrokePoint> snapped;
        if (trySnapToGeometricShape(pts, snapped)) {
            return snapped;
        }
    }

    // Step 2: Scale-adaptive RDP simplification
    // For small strokes (exponents x^2, x^3, subscripts, dots), use micro-epsilon (0.15px - 0.32px)
    // so tight curves and loops are never flattened or lost.
    // For normal strokes, use 0.55px (toned down from 1.1px) to preserve natural handwriting.
    qreal epsilon = 0.55;
    if (diag < 25.0 || arcLength < 35.0) {
        epsilon = qBound(0.15, diag * 0.015, 0.32);
    } else if (diag < 60.0) {
        epsilon = qBound(0.32, diag * 0.012, 0.55);
    }

    QVector<StrokePoint> simplified = simplifyRDP(pts, epsilon);
    if (simplified.size() <= 3) {
        return simplified;
    }

    const int m = simplified.size();

    // Step 3: Salient Corner Detection (preserve sharp corners like in 'x', 'z', '7', square roots)
    QVector<bool> isCorner(m, false);
    isCorner.first() = true;
    isCorner.last() = true;

    for (int i = 1; i < m - 1; ++i) {
        QPointF v1 = simplified[i].pos - simplified[i - 1].pos;
        QPointF v2 = simplified[i + 1].pos - simplified[i].pos;
        qreal len1 = std::hypot(v1.x(), v1.y());
        qreal len2 = std::hypot(v2.x(), v2.y());
        if (len1 > 1e-4 && len2 > 1e-4) {
            qreal dot = (v1.x() * v2.x() + v1.y() * v2.y()) / (len1 * len2);
            // Angle turn > ~55 degrees -> intentional sharp vertex
            if (dot < 0.58) {
                isCorner[i] = true;
            }
        }
    }

    // Step 4: Corner-preserving gentle binomial filter
    // For small strokes, do only 1 very gentle pass (weights: 0.12, 0.76, 0.12) to avoid shrinkage
    // For normal strokes, do 1 balanced pass (weights: 0.20, 0.60, 0.20)
    qreal sideWeight = (diag < 30.0 || arcLength < 40.0) ? 0.12 : 0.20;
    qreal centerWeight = 1.0 - 2.0 * sideWeight;

    QVector<StrokePoint> cur = simplified;
    QVector<StrokePoint> next = cur;
    for (int i = 1; i < m - 1; ++i) {
        if (isCorner[i]) {
            next[i] = cur[i];
        } else {
            next[i].pos = sideWeight * cur[i - 1].pos + centerWeight * cur[i].pos + sideWeight * cur[i + 1].pos;
            next[i].pressure = sideWeight * cur[i - 1].pressure + centerWeight * cur[i].pressure + sideWeight * cur[i + 1].pressure;
        }
    }

    return next;
}

} // namespace OmniNote
