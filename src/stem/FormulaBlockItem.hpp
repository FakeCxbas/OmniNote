#pragma once

#include <QGraphicsObject>
#include <QRectF>
#include <QPainter>
#include <QPixmap>
#include <QColor>
#include <QJsonObject>
#include <QJsonDocument>

namespace OmniNote {

class FormulaBlockItem : public QGraphicsObject {
    Q_OBJECT

public:
    enum { Type = UserType + 24 };
    int type() const override { return Type; }

    explicit FormulaBlockItem(const QString &latex = QString(), const QPointF &pos = QPointF(150, 150), const QColor &color = QColor("#ffffff"), QGraphicsItem *parent = nullptr);
    ~FormulaBlockItem() override = default;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    QString latex() const { return m_latex; }
    QColor color() const { return m_color; }
    void setLatex(const QString &latex, const QColor &color = QColor("#ffffff"));
    void setColor(const QColor &color);

    QString serialize() const;
    void deserialize(const QString &data);

    bool isControlHit(const QPointF &localPos) const;
    void openEditorDialog();

signals:
    void blockModified();
    void itemDeleted(FormulaBlockItem *item);
    void editRequested(FormulaBlockItem *item);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    QRectF closeButtonRect() const;
    QRectF editButtonRect() const;
    QRectF copyButtonRect() const;
    QRectF resizeGripRect() const;
    void updateRenderedPixmap();

    QString m_latex{"\\int_{0}^{\\infty} e^{-x^2} dx = \\frac{\\sqrt{\\pi}}{2}"};
    QColor m_color{QColor("#ffffff")};
    QRectF m_rect{0, 0, 240, 80};
    QPixmap m_renderedPixmap;

    bool m_isHovered{false};
    bool m_isDragging{false};
    bool m_isResizing{false};

    QPointF m_dragStartMousePos;
    QPointF m_dragStartItemPos;
    QRectF m_dragStartRect;
};

} // namespace OmniNote
