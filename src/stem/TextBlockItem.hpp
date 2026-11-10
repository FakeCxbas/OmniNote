#pragma once

#include <QGraphicsProxyWidget>
#include <QTextEdit>
#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QJsonObject>
#include <QJsonDocument>
#include <QEnterEvent>

namespace OmniNote {

class TextEditorWidget : public QTextEdit {
    Q_OBJECT

public:
    explicit TextEditorWidget(QWidget *parent = nullptr);

signals:
    void contentChanged();
    void focusChanged(bool focused);

protected:
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
};

class TextBlockContainerWidget : public QWidget {
    Q_OBJECT

public:
    enum StylePreset {
        TitleH1 = 0,     // 28pt, bold (2 rows)
        SubtitleH2 = 1,  // 20pt, bold (1.5 rows)
        HeadingH3 = 2,   // 16pt, bold (1 row)
        BodyText = 3     // 14pt, regular (1 row)
    };

    enum ResizeHandle {
        NoHandle = 0,
        CornerHandle,
        RightHandle,
        BottomHandle
    };

    explicit TextBlockContainerWidget(QWidget *parent = nullptr);

    QString text() const;
    void setText(const QString &text);

    StylePreset stylePreset() const { return m_stylePreset; }
    void setStylePreset(StylePreset preset);

    QColor textColor() const { return m_textColor; }
    void setTextColor(const QColor &color);

    QSize containerSize() const { return size(); }
    void setContainerSize(const QSize &s);

    void focusEditorAndSelectAll();
    void clearEditorFocus();
    bool isEditorFocused() const { return m_isFocused; }

    void setInteractiveMode(bool enable);
    bool isInteractiveMode() const { return m_isInteractiveMode; }

    QString serialize() const;
    void deserialize(const QString &jsonStr);

signals:
    void textChanged();
    void moveDeltaRequested(const QPointF &delta);
    void interactionStateChanged();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void applyTextStyle();
    ResizeHandle hitTestResizeHandle(const QPoint &pos) const;

    TextEditorWidget *m_editor{nullptr};

    StylePreset m_stylePreset{BodyText};
    QColor m_textColor{QColor("#111827")};

    ResizeHandle m_activeResizeHandle{NoHandle};
    ResizeHandle m_hoverResizeHandle{NoHandle};
    bool m_draggingItem{false};
    bool m_isHovered{false};
    bool m_isFocused{false};
    bool m_isInteractiveMode{false};

    QPoint m_dragStartPos;
    QSize m_dragStartSize;
};

class TextBlockItem : public QGraphicsProxyWidget {
    Q_OBJECT

public:
    enum { Type = UserType + 22 };
    int type() const override { return Type; }

    explicit TextBlockItem(const QPointF &initialPos = QPointF(100, 100), QGraphicsItem *parent = nullptr);
    ~TextBlockItem() override;

    QString text() const;
    void setText(const QString &text);
    void focusEditorAndSelectAll();

    QColor textColor() const;
    void setTextColor(const QColor &color);

    TextBlockContainerWidget::StylePreset stylePreset() const;
    void setStylePreset(TextBlockContainerWidget::StylePreset preset);

    void setInteractiveMode(bool enable);
    bool isInteractiveMode() const { return m_isInteractiveMode; }
    void updateHeaderVisibility();

    QString serialize() const;
    void deserialize(const QString &data);

signals:
    void itemDeleted(TextBlockItem *item);
    void blockModified();

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    void setupFloatingHeader();

    TextBlockContainerWidget *m_containerWidget{nullptr};
    QWidget *m_headerWidget{nullptr};
    QComboBox *m_styleCombo{nullptr};
    QPushButton *m_colorBtn{nullptr};
    QPushButton *m_closeBtn{nullptr};

    bool m_isInteractiveMode{false};
};

} // namespace OmniNote
