#pragma once

#include <QGraphicsObject>
#include <QRectF>
#include <QPainter>
#include <QPointF>

namespace OmniNote {

class VirtualRulerItem : public QGraphicsObject {
    Q_OBJECT

public:
    enum { Type = UserType + 25 };
    int type() const override { return Type; }

    explicit VirtualRulerItem(const QPointF &pos = QPointF(200, 200), QGraphicsItem *parent = nullptr);
    ~VirtualRulerItem() override = default;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    qreal rulerLength() const { return m_length; }
    void setRulerLength(qreal len);

    qreal rulerAngle() const { return m_angleDegrees; }
    void setRulerAngle(qreal degrees);

    // Snapping mathematical projection:
    // Returns true if scenePt is within threshold pixels from the top straight drawing edge [0, length]
    bool snapPoint(const QPointF &scenePt, QPointF &snappedPt, qreal threshold = 22.0) const;

    bool isDrawingEdgeHit(const QPointF &localPos) const;
    bool isControlHit(const QPointF &localPos) const;

signals:
    void rulerMoved();
    void closeRequested();

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    QRectF closeButtonRect() const;
    QRectF dialRect() const;
    qreal normalizeAngle(qreal deg) const;
    qreal snapAngle(qreal deg) const;

    qreal m_length{600.0};
    qreal m_height{80.0};
    qreal m_angleDegrees{0.0};

    bool m_isHovered{false};
    bool m_isDraggingBody{false};
    bool m_isRotatingDial{false};

    QPointF m_dragStartMouseScenePos;
    QPointF m_dragStartItemPos;
    qreal m_dialStartMouseAngle{0.0};
    qreal m_dialStartRulerAngle{0.0};
};

} // namespace OmniNote
