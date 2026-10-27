#pragma once

#include <QWidget>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QColor>
#include <QVector>
#include <QPair>

namespace OmniNote {

class ColorSwatchButton : public QAbstractButton {
    Q_OBJECT

public:
    explicit ColorSwatchButton(const QColor &color, const QString &tooltip, bool isCustom = false, QWidget *parent = nullptr);

    QColor color() const { return m_color; }
    void setColor(const QColor &c);
    bool isCustom() const { return m_isCustom; }

    QSize sizeHint() const override { return QSize(22, 22); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QColor m_color;
    bool m_isCustom{false};
    bool m_hovered{false};
};

class ColorPaletteControl : public QWidget {
    Q_OBJECT

public:
    explicit ColorPaletteControl(QWidget *parent = nullptr);

    QColor currentColor() const { return m_currentColor; }

public slots:
    void setCurrentColor(const QColor &color);
    void setCustomColor(const QColor &color);

signals:
    void colorSelected(const QColor &color);
    void customColorRequested();

private:
    QButtonGroup *m_buttonGroup{nullptr};
    QVector<ColorSwatchButton *> m_presetButtons;
    ColorSwatchButton *m_customButton{nullptr};
    QColor m_currentColor{QColor("#ffffff")};
    QColor m_customColor{QColor("#ff007f")};
};

} // namespace OmniNote
