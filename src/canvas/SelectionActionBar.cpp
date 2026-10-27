#include "SelectionActionBar.hpp"
#include "../app/AppIcons.hpp"
#include <QGraphicsDropShadowEffect>

namespace OmniNote {

SelectionActionBar::SelectionActionBar(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFixedHeight(34);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 2, 8, 2);
    layout->setSpacing(5);

    setStyleSheet(
        "QWidget#SelectionActionBarContainer {"
        "   background-color: rgba(26, 30, 48, 240);"
        "   border: 1px solid #7aa2f7;"
        "   border-radius: 16px;"
        "}"
        "QPushButton {"
        "   background-color: #24283b;"
        "   color: #c0caf5;"
        "   border: 1px solid #3b4261;"
        "   border-radius: 12px;"
        "   padding: 3px 9px;"
        "   font-size: 8.5pt;"
        "   font-weight: 500;"
        "}"
        "QPushButton:hover {"
        "   background-color: #2f3549;"
        "   border-color: #7dcfff;"
        "   color: #ffffff;"
        "}"
        "QPushButton#DeleteBtn:hover {"
        "   background-color: rgba(247, 118, 142, 60);"
        "   border-color: #f7768e;"
        "   color: #f7768e;"
        "}"
    );

    auto *container = new QWidget(this);
    container->setObjectName("SelectionActionBarContainer");
    auto *cLayout = new QHBoxLayout(container);
    cLayout->setContentsMargins(6, 2, 6, 2);
    cLayout->setSpacing(4);

    m_colorBtn = new QPushButton("Color", container);
    m_colorBtn->setIcon(AppIcons::get(AppIcons::IconType::Palette, QColor("#c0caf5"), 14));
    m_colorBtn->setToolTip("Aplicar el color actual a los trazos seleccionados");
    connect(m_colorBtn, &QPushButton::clicked, this, &SelectionActionBar::recolorRequested);
    cLayout->addWidget(m_colorBtn);

    m_dupBtn = new QPushButton("Duplicar", container);
    m_dupBtn->setIcon(AppIcons::get(AppIcons::IconType::Duplicate, QColor("#c0caf5"), 14));
    m_dupBtn->setToolTip("Duplicar trazos seleccionados (Ctrl+D)");
    connect(m_dupBtn, &QPushButton::clicked, this, &SelectionActionBar::duplicateRequested);
    cLayout->addWidget(m_dupBtn);

    m_delBtn = new QPushButton("Borrar", container);
    m_delBtn->setObjectName("DeleteBtn");
    m_delBtn->setIcon(AppIcons::get(AppIcons::IconType::Trash, QColor("#f7768e"), 14));
    m_delBtn->setToolTip("Eliminar trazos seleccionados (Supr)");
    connect(m_delBtn, &QPushButton::clicked, this, &SelectionActionBar::deleteRequested);
    cLayout->addWidget(m_delBtn);

    m_closeBtn = new QPushButton(container);
    m_closeBtn->setIcon(AppIcons::get(AppIcons::IconType::Close, QColor("#c0caf5"), 12));
    m_closeBtn->setToolTip("Deseleccionar");
    m_closeBtn->setFixedWidth(24);
    connect(m_closeBtn, &QPushButton::clicked, this, &SelectionActionBar::clearRequested);
    cLayout->addWidget(m_closeBtn);

    layout->addWidget(container);

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(16);
    shadow->setColor(QColor(0, 0, 0, 160));
    shadow->setOffset(0, 4);
    setGraphicsEffect(shadow);

    adjustSize();
    hide();
}

void SelectionActionBar::setCurrentColor(const QColor &color) {
    m_color = color;
}

} // namespace OmniNote
