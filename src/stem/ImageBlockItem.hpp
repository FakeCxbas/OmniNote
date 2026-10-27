#pragma once

#include <QGraphicsObject>
#include <QImage>
#include <QPixmap>
#include <QRectF>
#include <QPainter>
#include <QJsonObject>
#include <QJsonDocument>
#include <QByteArray>
#include <QBuffer>

namespace OmniNote {

class ImageBlockItem : public QGraphicsObject {
    Q_OBJECT

public:
    enum { Type = UserType + 23 };
    int type() const override { return Type; }

    enum ResizeHandle {
        NoHandle,
        TopLeft,
        TopRight,
        BottomRight,
        BottomLeft
    };

    explicit ImageBlockItem(const QImage &image = QImage(), const QPointF &pos = QPointF(100, 100), QGraphicsItem *parent = nullptr);
    ~ImageBlockItem() override = default;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    QImage image() const { return m_image; }
    void setImage(const QImage &image);

    qreal width() const { return m_rect.width(); }
    qreal height() const { return m_rect.height(); }
    void setSize(qreal w, qreal h);

    QString serialize() const;
    void deserialize(const QString &data);

    bool isControlHit(const QPointF &localPos) const {
        return closeButtonRect().contains(localPos) || (hitTestHandle(localPos) != NoHandle);
    }
    ResizeHandle hitTestHandle(const QPointF &pos) const;
    QRectF closeButtonRect() const;

signals:
    void blockModified();
    void itemDeleted(ImageBlockItem *item);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    void updatePixmap();

    QImage m_image;
    QPixmap m_cachedPixmap;
    QRectF m_rect{0, 0, 300, 200};

    bool m_isHovered{false};
    bool m_isDragging{false};
    ResizeHandle m_activeResizeHandle{NoHandle};
    ResizeHandle m_hoverResizeHandle{NoHandle};

    QPointF m_dragStartMousePos;
    QPointF m_dragStartItemPos;
    QRectF m_dragStartRect;
};

} // namespace OmniNote
