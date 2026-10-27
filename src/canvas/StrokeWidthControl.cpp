#include "StrokeWidthControl.hpp"
#include <cmath>

namespace OmniNote {

// ============================================================================
// StrokeWidthPreview
// ============================================================================

StrokeWidthPreview::StrokeWidthPreview(QWidget *parent) : QWidget(parent) {
    setFixedSize(26, 26);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setToolTip("Vista previa del grosor y color del trazo");
}

void StrokeWidthPreview::setWidth(double w) {
    m_width = w;
    update();
}

void StrokeWidthPreview::setColor(const QColor &c) {
    m_color = c;
    update();
}

void StrokeWidthPreview::paintEvent(QPaintEvent * /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal cx = width() / 2.0;
    const qreal cy = height() / 2.0;

    // Subtle outer guide boundary ring
    painter.setPen(QPen(QColor("#3b4261"), 1.0));
    painter.setBrush(QColor(22, 26, 40, 160));
    painter.drawEllipse(QPointF(cx, cy), 11.5, 11.5);

    // Dynamic inner stroke circle
    // Diameter is clamped between 2px and 20px for preview clarity
    qreal r = qBound(1.0, m_width * 0.5, 9.5);
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_color);
    painter.drawEllipse(QPointF(cx, cy), r, r);
}

// ============================================================================
// StrokeWidthControl
// ============================================================================

StrokeWidthControl::StrokeWidthControl(QWidget *parent) : QWidget(parent) {
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(2, 0, 4, 0);
    layout->setSpacing(5);

    auto *label = new QLabel("Grosor:", this);
    label->setStyleSheet("color: #c0caf5; font-size: 8.5pt; font-weight: 500;");
    layout->addWidget(label);

    const QString btnStyle =
        "QPushButton {"
        "   background-color: #24283b;"
        "   color: #c0caf5;"
        "   border: 1px solid #3b4261;"
        "   border-radius: 4px;"
        "   font-size: 9pt;"
        "   font-weight: bold;"
        "   min-width: 22px;"
        "   max-width: 22px;"
        "   height: 22px;"
        "   padding: 0px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #2f3549;"
        "   border-color: #7aa2f7;"
        "   color: #ffffff;"
        "}"
        "QPushButton:checked {"
        "   background-color: #3d59a1;"
        "   border-color: #7dcfff;"
        "   color: #ffffff;"
        "}";

    // Preset 1: Fino (1.0 px)
    m_btnFino = new QPushButton("•", this);
    m_btnFino->setCheckable(true);
    m_btnFino->setStyleSheet(btnStyle);
    m_btnFino->setToolTip("Fino (1.0 px) — Ideal para detalles y subíndices");
    connect(m_btnFino, &QPushButton::clicked, this, [this]() { setWidth(1.0); });
    layout->addWidget(m_btnFino);

    // Preset 2: Normal (2.5 px)
    m_btnNormal = new QPushButton("●", this);
    m_btnNormal->setCheckable(true);
    m_btnNormal->setStyleSheet(btnStyle);
    m_btnNormal->setToolTip("Normal (2.5 px) — Escritura regular de apuntes");
    connect(m_btnNormal, &QPushButton::clicked, this, [this]() { setWidth(2.5); });
    layout->addWidget(m_btnNormal);

    // Preset 3: Grueso (5.0 px)
    m_btnGrueso = new QPushButton("⬤", this);
    m_btnGrueso->setCheckable(true);
    m_btnGrueso->setStyleSheet(btnStyle);
    m_btnGrueso->setToolTip("Grueso (5.0 px) — Títulos y diagramas");
    connect(m_btnGrueso, &QPushButton::clicked, this, [this]() { setWidth(5.0); });
    layout->addWidget(m_btnGrueso);

    // Preset 4: Marcador (10.0 px)
    m_btnMarcador = new QPushButton("▉", this);
    m_btnMarcador->setCheckable(true);
    m_btnMarcador->setStyleSheet(btnStyle);
    m_btnMarcador->setToolTip("Marcador (10.0 px) — Títulos grandes y énfasis");
    connect(m_btnMarcador, &QPushButton::clicked, this, [this]() { setWidth(10.0); });
    layout->addWidget(m_btnMarcador);

    // Fluid Interactive Slider (1 to 60 -> 0.5 px to 30.0 px)
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(1, 60);
    m_slider->setValue(5); // 2.5 px
    m_slider->setFixedWidth(70);
    m_slider->setToolTip("Desliza para ajustar grosor o usa la rueda del ratón");
    m_slider->setStyleSheet(
        "QSlider::groove:horizontal {"
        "   height: 4px;"
        "   background: #24283b;"
        "   border: 1px solid #3b4261;"
        "   border-radius: 2px;"
        "}"
        "QSlider::sub-page:horizontal {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3d59a1, stop:1 #7aa2f7);"
        "   border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "   background: #c0caf5;"
        "   border: 1.5px solid #7aa2f7;"
        "   width: 13px;"
        "   margin-top: -5px;"
        "   margin-bottom: -5px;"
        "   border-radius: 6px;"
        "}"
        "QSlider::handle:horizontal:hover {"
        "   background: #ffffff;"
        "   border-color: #7dcfff;"
        "}"
    );
    layout->addWidget(m_slider);

    // Value badge
    m_valLabel = new QLabel("2.5 px", this);
    m_valLabel->setFixedWidth(46);
    m_valLabel->setAlignment(Qt::AlignCenter);
    m_valLabel->setStyleSheet(
        "QLabel {"
        "   background-color: #1f2335;"
        "   color: #7dcfff;"
        "   border: 1px solid #3b4261;"
        "   border-radius: 3px;"
        "   font-size: 8pt;"
        "   font-weight: bold;"
        "   padding: 2px 0px;"
        "}"
    );
    layout->addWidget(m_valLabel);

    // Live preview disc
    m_preview = new StrokeWidthPreview(this);
    layout->addWidget(m_preview);

    connect(m_slider, &QSlider::valueChanged, this, [this](int val) {
        double w = val * 0.5;
        m_valLabel->setText(QString::number(w, 'f', 1) + " px");
        m_preview->setWidth(w);
        updatePresetStates(w);
        emit widthChanged(w);
    });

    updatePresetStates(2.5);
}

double StrokeWidthControl::currentWidth() const {
    return m_slider ? (m_slider->value() * 0.5) : 2.5;
}

void StrokeWidthControl::setWidth(double w) {
    int val = qBound(1, static_cast<int>(std::round(w * 2.0)), 60);
    if (m_slider && m_slider->value() != val) {
        m_slider->setValue(val);
    } else if (m_slider) {
        double currentW = val * 0.5;
        m_valLabel->setText(QString::number(currentW, 'f', 1) + " px");
        m_preview->setWidth(currentW);
        updatePresetStates(currentW);
        emit widthChanged(currentW);
    }
}

void StrokeWidthControl::setColor(const QColor &c) {
    if (m_preview) {
        m_preview->setColor(c);
    }
}

void StrokeWidthControl::stepUp() {
    if (m_slider) {
        m_slider->setValue(qMin(60, m_slider->value() + 1));
    }
}

void StrokeWidthControl::stepDown() {
    if (m_slider) {
        m_slider->setValue(qMax(1, m_slider->value() - 1));
    }
}

void StrokeWidthControl::wheelEvent(QWheelEvent *event) {
    int numDegrees = event->angleDelta().y() / 8;
    int numSteps = numDegrees / 15;
    if (numSteps != 0 && m_slider) {
        int newVal = qBound(1, m_slider->value() + numSteps, 60);
        m_slider->setValue(newVal);
    }
    event->accept();
}

void StrokeWidthControl::updatePresetStates(double w) {
    auto match = [w](double target) {
        return std::abs(w - target) < 0.15;
    };

    if (m_btnFino) m_btnFino->setChecked(match(1.0));
    if (m_btnNormal) m_btnNormal->setChecked(match(2.5));
    if (m_btnGrueso) m_btnGrueso->setChecked(match(5.0));
    if (m_btnMarcador) m_btnMarcador->setChecked(match(10.0));
}

} // namespace OmniNote
