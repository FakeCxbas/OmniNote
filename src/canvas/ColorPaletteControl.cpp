#include "ColorPaletteControl.hpp"
#include "../app/AppIcons.hpp"
#include <QHBoxLayout>
#include <QPainter>
#include <QPaintEvent>
#include <QFont>
#include <QtMath>

namespace OmniNote {

// ============================================================================
// ColorSwatchButton
// ============================================================================

ColorSwatchButton::ColorSwatchButton(const QColor &color, const QString &tooltip, bool isCustom, QWidget *parent)
    : QAbstractButton(parent), m_color(color), m_isCustom(isCustom) {
    setCheckable(true);
    setFixedSize(22, 22);
    setToolTip(tooltip);
    setCursor(Qt::PointingHandCursor);
}

void ColorSwatchButton::setColor(const QColor &c) {
    m_color = c;
    update();
}

void ColorSwatchButton::enterEvent(QEnterEvent *event) {
    m_hovered = true;
    update();
    QAbstractButton::enterEvent(event);
}

void ColorSwatchButton::leaveEvent(QEvent *event) {
    m_hovered = false;
    update();
    QAbstractButton::leaveEvent(event);
}

void ColorSwatchButton::paintEvent(QPaintEvent * /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const qreal cx = width() / 2.0;
    const qreal cy = height() / 2.0;

    if (isChecked()) {
        // 1. Prominent Outer Glowing Selection Ring
        bool isLightOrBlue = (m_color == QColor("#7aa2f7") || m_color == QColor("#7dcfff") || m_color == QColor("#ffffff"));
        QColor ringColor = isLightOrBlue ? QColor("#7dcfff") : QColor("#7aa2f7");

        p.setPen(QPen(ringColor, 1.8));
        p.setBrush(QColor("#1f2335"));
        p.drawEllipse(QPointF(cx, cy), 9.5, 9.5);

        // 2. High-contrast separator ring
        p.setPen(QPen(QColor("#14161c"), 1.2));
        p.setBrush(m_color);
        p.drawEllipse(QPointF(cx, cy), 7.0, 7.0);

        // 3. Central selection indicator dot
        qreal lum = 0.299 * m_color.redF() + 0.587 * m_color.greenF() + 0.114 * m_color.blueF();
        QColor dotColor = (lum > 0.55) ? QColor("#14161c") : QColor("#ffffff");
        p.setPen(Qt::NoPen);
        p.setBrush(dotColor);
        p.drawEllipse(QPointF(cx, cy), 2.0, 2.0);

    } else {
        // Unchecked state
        if (m_hovered) {
            p.setPen(QPen(QColor("#7aa2f7"), 1.2));
            p.setBrush(QColor("#24283b"));
            p.drawEllipse(QPointF(cx, cy), 8.5, 8.5);
        } else {
            p.setPen(QPen(QColor(59, 66, 97, 180), 1.0));
            p.setBrush(QColor(25, 29, 44, 140));
            p.drawEllipse(QPointF(cx, cy), 8.0, 8.0);
        }

        // Inner color disc
        p.setPen(QPen(QColor(36, 40, 59), 0.8));
        p.setBrush(m_color);
        p.drawEllipse(QPointF(cx, cy), 5.5, 5.5);
    }

    if (m_isCustom) {
        QPixmap palettePx = AppIcons::getPixmap(AppIcons::IconType::Palette, isChecked() ? QColor("#ffffff") : QColor("#c0caf5"), 12);
        p.drawPixmap(rect().center().x() - 6, rect().center().y() - 6, palettePx);
    }
}

// ============================================================================
// ColorPaletteControl
// ============================================================================

ColorPaletteControl::ColorPaletteControl(QWidget *parent) : QWidget(parent) {
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(2, 0, 2, 0);
    layout->setSpacing(3);

    m_buttonGroup = new QButtonGroup(this);
    m_buttonGroup->setExclusive(true);

    const QVector<QPair<QString, QColor>> presets = {
        {"Blanco (#ffffff)", QColor("#ffffff")},
        {"Lavanda (#c0caf5)", QColor("#c0caf5")},
        {"Azul (#7aa2f7)", QColor("#7aa2f7")},
        {"Verde (#9ece6a)", QColor("#9ece6a")},
        {"Naranja (#ff9e64)", QColor("#ff9e64")},
        {"Violeta (#bb9af7)", QColor("#bb9af7")},
        {"Ámbar (#e0af68)", QColor("#e0af68")},
        {"Cian (#7dcfff)", QColor("#7dcfff")},
        {"Coral (#f7768e)", QColor("#f7768e")}
    };

    int id = 0;
    for (const auto &item : presets) {
        auto *btn = new ColorSwatchButton(item.second, item.first, false, this);
        layout->addWidget(btn);
        m_buttonGroup->addButton(btn, id++);
        m_presetButtons.append(btn);

        connect(btn, &QAbstractButton::clicked, this, [this, btn]() {
            m_currentColor = btn->color();
            emit colorSelected(m_currentColor);
        });
    }

    // Custom Color Button (🎨)
    m_customButton = new ColorSwatchButton(m_customColor, "Color personalizado...", true, this);
    layout->addWidget(m_customButton);
    m_buttonGroup->addButton(m_customButton, id++);

    connect(m_customButton, &QAbstractButton::clicked, this, [this]() {
        emit customColorRequested();
    });

    // Default: select White (#ffffff)
    if (!m_presetButtons.isEmpty()) {
        m_presetButtons.first()->setChecked(true);
    }
}

void ColorPaletteControl::setCurrentColor(const QColor &color) {
    m_currentColor = color;
    bool foundPreset = false;

    for (auto *btn : m_presetButtons) {
        if (btn->color().rgb() == color.rgb()) {
            btn->setChecked(true);
            foundPreset = true;
            break;
        }
    }

    if (!foundPreset && m_customButton) {
        m_customColor = color;
        m_customButton->setColor(color);
        m_customButton->setChecked(true);
    }
}

void ColorPaletteControl::setCustomColor(const QColor &color) {
    if (!color.isValid()) return;
    m_customColor = color;
    m_currentColor = color;
    if (m_customButton) {
        m_customButton->setColor(color);
        m_customButton->setChecked(true);
    }
}

} // namespace OmniNote
