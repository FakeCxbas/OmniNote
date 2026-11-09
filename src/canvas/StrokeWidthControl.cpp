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

    // Preset 1: Fino (2.5 px)
    m_btnP25 = new QPushButton("•", this);
    m_btnP25->setCheckable(true);
    m_btnP25->setStyleSheet(btnStyle);
    m_btnP25->setToolTip("Fino (2.5 px) — Fórmulas compactas y subíndices");
    connect(m_btnP25, &QPushButton::clicked, this, [this]() { setWidth(2.5); });
    layout->addWidget(m_btnP25);

    // Preset 2: Medio-Fino (3.0 px)
    m_btnP30 = new QPushButton("●", this);
    m_btnP30->setCheckable(true);
    m_btnP30->setStyleSheet(btnStyle);
    m_btnP30->setToolTip("Medio-Fino (3.0 px) — Escritura ágil");
    connect(m_btnP30, &QPushButton::clicked, this, [this]() { setWidth(3.0); });
    layout->addWidget(m_btnP30);

    // Preset 3: Estándar Canva (3.5 px)
    m_btnP35 = new QPushButton("⬤", this);
    m_btnP35->setCheckable(true);
    m_btnP35->setStyleSheet(btnStyle);
    m_btnP35->setToolTip("Normal Canva (3.5 px) — Escritura regular de apuntes STEM");
    connect(m_btnP35, &QPushButton::clicked, this, [this]() { setWidth(3.5); });
    layout->addWidget(m_btnP35);

    // Preset 4: Énfasis (4.0 px)
    m_btnP40 = new QPushButton("⬤", this);
    m_btnP40->setCheckable(true);
    m_btnP40->setStyleSheet(btnStyle);
    m_btnP40->setToolTip("Énfasis (4.0 px) — Palabras clave y diagramas");
    connect(m_btnP40, &QPushButton::clicked, this, [this]() { setWidth(4.0); });
    layout->addWidget(m_btnP40);

    // Preset 5: Marcador (10.0 px)
    m_btnMarcador = new QPushButton("▉", this);
    m_btnMarcador->setCheckable(true);
    m_btnMarcador->setStyleSheet(btnStyle);
    m_btnMarcador->setToolTip("Marcador (10.0 px) — Títulos grandes y resaltados");
    connect(m_btnMarcador, &QPushButton::clicked, this, [this]() { setWidth(10.0); });
    layout->addWidget(m_btnMarcador);

    // Fluid Interactive Slider (1 to 60 -> 0.5 px to 30.0 px)
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(1, 60);
    m_slider->setValue(7); // 3.5 px (Canva pen handwriting standard)
    m_slider->setFixedWidth(65);
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

    const QString stepBtnStyle =
        "QPushButton {"
        "   background-color: #24283b;"
        "   color: #c0caf5;"
        "   border: 1px solid #3b4261;"
        "   border-radius: 3px;"
        "   font-size: 8.5pt;"
        "   font-weight: bold;"
        "   min-width: 18px;"
        "   max-width: 18px;"
        "   height: 20px;"
        "   padding: 0px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #2f3549;"
        "   border-color: #7aa2f7;"
        "   color: #ffffff;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #3d59a1;"
        "}";

    // Step down button (-0.5 px)
    m_btnStepDown = new QPushButton("−", this);
    m_btnStepDown->setStyleSheet(stepBtnStyle);
    m_btnStepDown->setToolTip("Reducir grosor (-0.5 px) [Atajo: []");
    connect(m_btnStepDown, &QPushButton::clicked, this, &StrokeWidthControl::stepDown);
    layout->addWidget(m_btnStepDown);

    // Value badge
    m_valLabel = new QLabel("3.5 px", this);
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

    // Step up button (+0.5 px)
    m_btnStepUp = new QPushButton("+", this);
    m_btnStepUp->setStyleSheet(stepBtnStyle);
    m_btnStepUp->setToolTip("Aumentar grosor (+0.5 px) [Atajo: ]]");
    connect(m_btnStepUp, &QPushButton::clicked, this, &StrokeWidthControl::stepUp);
    layout->addWidget(m_btnStepUp);

    // Live preview disc
    m_preview = new StrokeWidthPreview(this);
    m_preview->setWidth(3.5);
    layout->addWidget(m_preview);

    connect(m_slider, &QSlider::valueChanged, this, [this](int val) {
        double w = val * 0.5;
        m_valLabel->setText(QString::number(w, 'f', 1) + " px");
        m_preview->setWidth(w);
        updatePresetStates(w);
        emit widthChanged(w);
    });

    updatePresetStates(3.5);
}

double StrokeWidthControl::currentWidth() const {
    return m_slider ? (m_slider->value() * 0.5) : 3.5;
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

    if (m_btnP25) m_btnP25->setChecked(match(2.5));
    if (m_btnP30) m_btnP30->setChecked(match(3.0));
    if (m_btnP35) m_btnP35->setChecked(match(3.5));
    if (m_btnP40) m_btnP40->setChecked(match(4.0));
    if (m_btnMarcador) m_btnMarcador->setChecked(match(10.0));
}

} // namespace OmniNote
