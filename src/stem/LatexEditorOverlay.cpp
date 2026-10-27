#include "LatexEditorOverlay.hpp"
#include "LatexRenderer.hpp"
#include "FormulaBlockItem.hpp"
#include "../app/AppIcons.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollArea>
#include <QFontDatabase>

namespace OmniNote {

LatexEditorOverlay::LatexEditorOverlay(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::StrongFocus);
    hide();

    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(60);
    connect(m_debounceTimer, &QTimer::timeout, this, &LatexEditorOverlay::updateLivePreview);

    setupUi();
}

void LatexEditorOverlay::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setAlignment(Qt::AlignCenter);

    m_dialogCard = new QWidget(this);
    m_dialogCard->setFixedSize(620, 520);
    m_dialogCard->setStyleSheet(
        "QWidget#DialogCard {"
        "   background-color: #16161e;"
        "   border: 1.5px solid #3b4261;"
        "   border-radius: 12px;"
        "}"
        "QLabel { color: #c0caf5; font-size: 11px; font-weight: bold; }"
        "QPlainTextEdit {"
        "   background-color: #1a1b26;"
        "   color: #c0caf5;"
        "   border: 1.5px solid #414868;"
        "   border-radius: 6px;"
        "   font-family: 'JetBrains Mono', 'Fira Code', 'DejaVu Sans Mono', monospace;"
        "   font-size: 13px;"
        "   padding: 8px;"
        "   selection-background-color: #3d59a1;"
        "}"
        "QPlainTextEdit:focus { border: 1.5px solid #7aa2f7; }"
        "QPushButton.TplBtn {"
        "   background-color: #24283b;"
        "   color: #c0caf5;"
        "   border: 1px solid #3b4261;"
        "   border-radius: 4px;"
        "   padding: 4px 8px;"
        "   font-size: 10px;"
        "   font-weight: 500;"
        "}"
        "QPushButton.TplBtn:hover {"
        "   background-color: #3d59a1;"
        "   border-color: #7aa2f7;"
        "   color: #ffffff;"
        "}"
        "QPushButton#CancelBtn {"
        "   background-color: #24283b;"
        "   color: #c0caf5;"
        "   border: 1px solid #3b4261;"
        "   border-radius: 6px;"
        "   padding: 7px 16px;"
        "   font-weight: bold;"
        "}"
        "QPushButton#CancelBtn:hover { background-color: #2f3549; color: #ffffff; }"
        "QPushButton#SaveBtn {"
        "   background-color: #7aa2f7;"
        "   color: #15161e;"
        "   border: none;"
        "   border-radius: 6px;"
        "   padding: 7px 20px;"
        "   font-weight: bold;"
        "   font-size: 12px;"
        "}"
        "QPushButton#SaveBtn:hover { background-color: #7dcfff; }"
        "QPushButton#CloseBtn {"
        "   background-color: transparent;"
        "   color: #7aa2f7;"
        "   border: none;"
        "   font-size: 14px;"
        "   font-weight: bold;"
        "}"
        "QPushButton#CloseBtn:hover { color: #f7768e; }"
    );
    m_dialogCard->setObjectName("DialogCard");

    auto *cardLayout = new QVBoxLayout(m_dialogCard);
    cardLayout->setContentsMargins(18, 14, 18, 16);
    cardLayout->setSpacing(8);

    // Header bar
    auto *headerLayout = new QHBoxLayout();
    auto *titleIcon = new QLabel(m_dialogCard);
    titleIcon->setPixmap(AppIcons::getPixmap(AppIcons::IconType::Formula, QColor("#7aa2f7"), 18));
    headerLayout->addWidget(titleIcon);

    auto *titleIconLabel = new QLabel("Editor de Fórmulas LaTeX", m_dialogCard);
    titleIconLabel->setStyleSheet("color: #7aa2f7; font-size: 13px; font-weight: bold; margin-left: 4px;");
    headerLayout->addWidget(titleIconLabel);
    headerLayout->addStretch();

    auto *closeBtn = new QPushButton(m_dialogCard);
    closeBtn->setObjectName("CloseBtn");
    closeBtn->setIcon(AppIcons::get(AppIcons::IconType::Close, QColor("#7aa2f7"), 16));
    closeBtn->setFixedSize(26, 26);
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, this, [this]() {
        hide();
        emit cancelled();
    });
    headerLayout->addWidget(closeBtn);
    cardLayout->addLayout(headerLayout);

    // Code label
    auto *codeLabel = new QLabel("CÓDIGO LATEX:", m_dialogCard);
    cardLayout->addWidget(codeLabel);

    // Plain text editor
    m_latexEditor = new QPlainTextEdit(m_dialogCard);
    m_latexEditor->setPlaceholderText("Escribe código LaTeX aquí... ej: \\int_{0}^{\\infty} e^{-x^2} dx = \\frac{\\sqrt{\\pi}}{2}");
    m_latexEditor->setFixedHeight(85);
    connect(m_latexEditor, &QPlainTextEdit::textChanged, this, [this]() {
        m_debounceTimer->start();
    });
    cardLayout->addWidget(m_latexEditor);

    // Templates row
    auto *tplLayout = new QHBoxLayout();
    tplLayout->setSpacing(5);

    auto addTpl = [&](const QString &label, const QString &code) {
        auto *btn = new QPushButton(label, m_dialogCard);
        btn->setProperty("class", "TplBtn");
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, [this, code]() {
            m_latexEditor->setPlainText(code);
            m_latexEditor->setFocus();
        });
        tplLayout->addWidget(btn);
    };

    addTpl("∫ Integral", "\\int_{0}^{\\infty} e^{-x^2} dx = \\frac{\\sqrt{\\pi}}{2}");
    addTpl("x = Cuadrática", "x = \\frac{-b \\pm \\sqrt{b^2 - 4ac}}{2a}");
    addTpl("∑ Suma", "\\sum_{k=1}^{n} k^2 = \\frac{n(n+1)(2n+1)}{6}");
    addTpl("a/b Fracción", "\\frac{a}{b}");
    addTpl("√ Raíz", "\\sqrt{x^2 + y^2}");
    addTpl("lim Límite", "\\lim_{x \\to 0} \\frac{\\sin x}{x} = 1");
    addTpl("∂ Derivada", "\\frac{\\partial f}{\\partial x}");
    tplLayout->addStretch();
    cardLayout->addLayout(tplLayout);

    // Color choices row
    auto *colorLayout = new QHBoxLayout();
    auto *colorTitle = new QLabel("COLOR DE FÓRMULA:", m_dialogCard);
    colorLayout->addWidget(colorTitle);

    const QVector<QPair<QString, QString>> colors = {
        {"Blanco", "#ffffff"},
        {"Celeste", "#7dcfff"},
        {"Azul", "#7aa2f7"},
        {"Amarillo", "#e0af68"},
        {"Verde", "#9ece6a"},
        {"Rosa", "#f7768e"}
    };

    for (const auto &pair : colors) {
        auto *cBtn = new QPushButton(m_dialogCard);
        cBtn->setFixedSize(22, 22);
        cBtn->setCursor(Qt::PointingHandCursor);
        cBtn->setToolTip(pair.first);
        cBtn->setStyleSheet(QString(
            "QPushButton { background-color: %1; border-radius: 11px; border: 1.5px solid #414868; }"
            "QPushButton:hover { border: 2px solid #ffffff; }"
        ).arg(pair.second));
        connect(cBtn, &QPushButton::clicked, this, [this, c = QColor(pair.second)]() {
            m_selectedColor = c;
            updateLivePreview();
        });
        colorLayout->addWidget(cBtn);
    }
    colorLayout->addStretch();
    cardLayout->addLayout(colorLayout);

    // Preview area label
    auto *prevTitle = new QLabel("VISTA PREVIA EN VIVO (ALTA DEFINICIÓN):", m_dialogCard);
    cardLayout->addWidget(prevTitle);

    // Preview box
    auto *previewBox = new QWidget(m_dialogCard);
    previewBox->setFixedHeight(120);
    previewBox->setStyleSheet("background-color: #0f111a; border: 1.5px solid #24283b; border-radius: 8px;");
    auto *pLayout = new QVBoxLayout(previewBox);
    pLayout->setContentsMargins(10, 8, 10, 8);
    pLayout->setAlignment(Qt::AlignCenter);

    m_previewLabel = new QLabel(previewBox);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    pLayout->addWidget(m_previewLabel);

    m_statusLabel = new QLabel(previewBox);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("color: #bb9af7; font-size: 10px; font-weight: normal;");
    m_statusLabel->hide();
    pLayout->addWidget(m_statusLabel);

    cardLayout->addWidget(previewBox);

    // Bottom action buttons
    auto *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);

    auto *hintLabel = new QLabel("Presiona Ctrl+Enter para guardar, Esc para cancelar", m_dialogCard);
    hintLabel->setStyleSheet("color: #565f89; font-size: 9.5px; font-weight: normal;");
    btnLayout->addWidget(hintLabel);
    btnLayout->addStretch();

    auto *cancelBtn = new QPushButton("Cancelar", m_dialogCard);
    cancelBtn->setObjectName("CancelBtn");
    cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(cancelBtn, &QPushButton::clicked, this, [this]() {
        hide();
        emit cancelled();
    });
    btnLayout->addWidget(cancelBtn);

    auto *saveBtn = new QPushButton("Guardar Fórmula", m_dialogCard);
    saveBtn->setObjectName("SaveBtn");
    saveBtn->setIcon(AppIcons::get(AppIcons::IconType::Check, QColor("#15161e"), 16));
    saveBtn->setCursor(Qt::PointingHandCursor);
    connect(saveBtn, &QPushButton::clicked, this, [this]() {
        QString text = m_latexEditor->toPlainText().trimmed();
        if (m_currentItem) {
            emit formulaSaved(m_currentItem, text, m_selectedColor);
        } else {
            emit newFormulaRequested(text, m_selectedColor);
        }
        hide();
    });
    btnLayout->addWidget(saveBtn);

    cardLayout->addLayout(btnLayout);
    mainLayout->addWidget(m_dialogCard);
}

void LatexEditorOverlay::openForFormula(FormulaBlockItem *item) {
    m_currentItem = item;
    if (item) {
        m_latexEditor->setPlainText(item->latex());
        m_selectedColor = item->color();
    } else {
        if (m_latexEditor->toPlainText().trimmed().isEmpty()) {
            m_latexEditor->setPlainText("\\int_{0}^{\\infty} e^{-x^2} dx = \\frac{\\sqrt{\\pi}}{2}");
        }
        m_selectedColor = QColor("#ffffff");
    }

    if (parentWidget()) {
        setGeometry(0, 0, parentWidget()->width(), parentWidget()->height());
    }

    show();
    raise();
    m_latexEditor->setFocus();
    m_latexEditor->selectAll();
    updateLivePreview();
}

void LatexEditorOverlay::updateLivePreview() {
    QString latex = m_latexEditor->toPlainText().trimmed();
    if (latex.isEmpty()) {
        m_previewLabel->clear();
        m_statusLabel->setText("Ingresa una fórmula LaTeX...");
        m_statusLabel->show();
        return;
    }

    QPixmap px = LatexRenderer::instance().render(latex, m_selectedColor, 180);
    if (!px.isNull()) {
        // Scale down preview if too large to fit box
        if (px.width() > 560 || px.height() > 100) {
            px = px.scaled(560, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        m_previewLabel->setPixmap(px);
        m_statusLabel->hide();
    } else {
        m_previewLabel->clear();
        m_statusLabel->setText("Renderizando vista previa...");
        m_statusLabel->show();
    }
}

void LatexEditorOverlay::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    // Dark semi-transparent backdrop over entire canvas view
    painter.fillRect(rect(), QColor(10, 12, 18, 175));
}

void LatexEditorOverlay::resizeEvent(QResizeEvent *) {
    if (parentWidget()) {
        setGeometry(0, 0, parentWidget()->width(), parentWidget()->height());
    }
}

void LatexEditorOverlay::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        hide();
        emit cancelled();
        event->accept();
        return;
    }

    if ((event->modifiers() & Qt::ControlModifier) &&
        (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        QString text = m_latexEditor->toPlainText().trimmed();
        if (m_currentItem) {
            emit formulaSaved(m_currentItem, text, m_selectedColor);
        } else {
            emit newFormulaRequested(text, m_selectedColor);
        }
        hide();
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

void LatexEditorOverlay::mousePressEvent(QMouseEvent *event) {
    // If clicking outside the dialog card, do not dismiss accidentally or absorb event
    if (!m_dialogCard->geometry().contains(event->pos())) {
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

} // namespace OmniNote
