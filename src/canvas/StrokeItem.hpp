#pragma once

#include <QGraphicsItem>
#include <QColor>
#include <QVector>
#include <QPolygonF>
#include <QPainterPath>
#include "../input/StrokeSmoother.hpp"

namespace OmniNote {

enum class ToolType {
    Pen = 0,
    Highlighter = 1,
    Eraser = 2,
    Lasso = 3,
    ShapeLine = 4,
    ShapeRect = 5,
    ShapeEllipse = 6,
    ShapeAxes2D = 7,
    ShapeAxes3D = 8,
    Text = 9
};

class StrokeItem : public QGraphicsItem {
public:
    enum { Type = UserType + 10 };
    int type() const override { return Type; }

    StrokeItem(ToolType toolType = ToolType::Pen,
               const QColor &color = QColor("#c0caf5"),
               qreal baseWidth = 2.5,
               QGraphicsItem *parent = nullptr);
    ~StrokeItem() override = default;

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    ToolType toolType() const { return m_toolType; }
    void setToolType(ToolType t);

    QColor color() const { return m_color; }
    void setColor(const QColor &c);

    qreal baseWidth() const { return m_baseWidth; }
    void setBaseWidth(qreal w);

    const QVector<StrokePoint> &points() const { return m_points; }
    void setPoints(const QVector<StrokePoint> &pts);
    void addPoint(const StrokePoint &pt);
    void addPointFast(const StrokePoint &pt);
    void finalizeStroke();
    void setAutoImprove(bool enable) { m_autoImprove = enable; }
    bool autoImprove() const { return m_autoImprove; }

    void translateStroke(const QPointF &delta);
    void scaleStroke(qreal factor, const QPointF &pivot);

    const QPolygonF &ribbonPolygon() const { return m_ribbon; }

    bool isHighlighter() const { return m_toolType == ToolType::Highlighter; }

    QByteArray serialize() const;
    static StrokeItem *deserialize(const QByteArray &data);

private:
    void rebuildGeometry();

    ToolType m_toolType;
    QColor m_color;
    qreal m_baseWidth;
    QVector<StrokePoint> m_points;
    QVector<StrokePoint> m_smoothedPoints;
    QPolygonF m_ribbon;
    QPainterPath m_shapePath;
    QRectF m_boundingRect;
    bool m_autoImprove{true};
};

} // namespace OmniNote
