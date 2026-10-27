#pragma once

#include <QGraphicsItem>
#include <QPainter>
#include <QPainterPath>
#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include "MathExpressionParser.hpp"

namespace OmniNote {

class FunctionPlotItem : public QGraphicsItem {
public:
    enum { Type = UserType + 21 };
    int type() const override { return Type; }

    FunctionPlotItem(const QString &expression = "sin(x)/x",
                     double xMin = -10.0, double xMax = 10.0,
                     double yMin = -3.0, double yMax = 3.0,
                     QGraphicsItem *parent = nullptr);
    ~FunctionPlotItem() override = default;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    // Getters and setters
    QString expression() const { return m_expression; }
    void setExpression(const QString &expr);

    void setDomain(double xMin, double xMax);
    void setRange(double yMin, double yMax);
    void setPlotSize(qreal w, qreal h);

    double xMin() const { return m_xMin; }
    double xMax() const { return m_xMax; }
    double yMin() const { return m_yMin; }
    double yMax() const { return m_yMax; }

    QString serialize() const;
    void deserialize(const QString &data);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    void updatePlotPath();
    QPointF mathToScreen(double x, double y) const;
    double calculateTickStep(double minVal, double maxVal, int targetTicks) const;

    QString m_expression;
    MathExpressionParser m_parser;

    double m_xMin{-10.0};
    double m_xMax{10.0};
    double m_yMin{-3.0};
    double m_yMax{3.0};

    qreal m_width{440.0};
    qreal m_height{300.0};
    const qreal m_headerHeight{32.0};

    QVector<QPainterPath> m_plotPaths;

    bool m_resizing{false};
    QPointF m_resizeStartPos;
    QSizeF m_resizeStartSize;
};

} // namespace OmniNote
