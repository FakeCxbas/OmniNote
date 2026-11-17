#include "HomeHubView.hpp"
#include "AppIcons.hpp"
#include <QMessageBox>
#include <QHeaderView>

namespace OmniNote {

HomeHubView::HomeHubView(SqliteStorage &storage, QWidget *parent)
    : QWidget(parent), m_storage(storage) {
    setupUI();
    refreshData();
}

void HomeHubView::setupUI() {
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Main background scroll area for responsiveness
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet("QScrollArea { background-color: #14161f; border: none; }");

    auto *container = new QWidget();
    container->setStyleSheet("background-color: #14161f;");
    auto *containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(36, 28, 36, 36);
    containerLayout->setSpacing(24);

    // Maximum content width wrapper
    auto *contentWrapper = new QWidget(container);
    contentWrapper->setMaximumWidth(1160);
    auto *mainLayout = new QVBoxLayout(contentWrapper);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(22);

    containerLayout->addWidget(contentWrapper, 0, Qt::AlignHCenter);
    scrollArea->setWidget(container);
    rootLayout->addWidget(scrollArea);

    // ==================== 1. Header Banner ====================
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(16);

    auto *iconLabel = new QLabel(contentWrapper);
    iconLabel->setPixmap(AppIcons::getPixmap(AppIcons::IconType::Home, QColor("#7aa2f7"), 38));
    headerLayout->addWidget(iconLabel);

    auto *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(3);

    auto *titleLabel = new QLabel("Centro de Inicio — OmniNote STEM", contentWrapper);
    titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #c0caf5; letter-spacing: 0.5px;");
    titleLayout->addWidget(titleLabel);

    auto *subtitleLabel = new QLabel("Crea hojas rápidas independientes, explora tus borradores o accede a tus cuadernos de materias.", contentWrapper);
    subtitleLabel->setStyleSheet("font-size: 12px; color: #7aa2f7;");
    titleLayout->addWidget(subtitleLabel);

    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();

    auto *returnBtn = new QPushButton("Volver al Lienzo  →", contentWrapper);
    returnBtn->setCursor(Qt::PointingHandCursor);
    returnBtn->setStyleSheet(
        "QPushButton { background-color: #24283b; color: #7aa2f7; border: 1px solid #3d59a1; border-radius: 8px; padding: 10px 18px; font-size: 12.5px; font-weight: bold; }"
        "QPushButton:hover { background-color: #3d59a1; color: #ffffff; border-color: #7aa2f7; }"
        "QPushButton:pressed { background-color: #2e3c64; }"
    );
    connect(returnBtn, &QPushButton::clicked, this, &HomeHubView::returnToCanvasRequested);
    headerLayout->addWidget(returnBtn);

    mainLayout->addLayout(headerLayout);

    // ==================== 2. Hero Action Cards ====================
    auto *cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(16);

    // Card 1: + Nueva Hoja Rapida
    auto *card1 = new QFrame(contentWrapper);
    card1->setStyleSheet(
        "QFrame { background-color: #1a1e2f; border: 1px solid #7aa2f7; border-radius: 10px; }"
    );
    auto *c1Layout = new QVBoxLayout(card1);
    c1Layout->setContentsMargins(18, 18, 18, 18);
    c1Layout->setSpacing(12);

    auto *c1Header = new QHBoxLayout();
    auto *c1Icon = new QLabel(card1);
    c1Icon->setStyleSheet("border: none;");
    c1Icon->setPixmap(AppIcons::getPixmap(AppIcons::IconType::Plus, QColor("#7aa2f7"), 24));
    c1Header->addWidget(c1Icon);
    auto *c1Title = new QLabel("Nueva Hoja Rápida", card1);
    c1Title->setStyleSheet("font-size: 15px; font-weight: bold; color: #7aa2f7; border: none;");
    c1Header->addWidget(c1Title);
    c1Header->addStretch();
    c1Layout->addLayout(c1Header);

    auto *c1Desc = new QLabel("Lienzo en blanco instantáneo con cuadrícula Canva 5mm. Sin materias ni pasos previos.", card1);
    c1Desc->setWordWrap(true);
    c1Desc->setStyleSheet("font-size: 11.5px; color: #a9b1d6; border: none; line-height: 1.4;");
    c1Layout->addWidget(c1Desc, 1);

    auto *newSheetBtn = new QPushButton("Crear Hoja en Blanco", card1);
    newSheetBtn->setCursor(Qt::PointingHandCursor);
    newSheetBtn->setIcon(AppIcons::get(AppIcons::IconType::Plus, QColor("#15161e"), 16));
    newSheetBtn->setStyleSheet(
        "QPushButton { background-color: #7aa2f7; color: #15161e; border: none; border-radius: 6px; padding: 9px 16px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #89b4fa; }"
        "QPushButton:pressed { background-color: #628cee; }"
    );
    connect(newSheetBtn, &QPushButton::clicked, this, &HomeHubView::onNewQuickSheetClicked);
    c1Layout->addWidget(newSheetBtn);
    cardsLayout->addWidget(card1, 1);

    // Card 2: Hojas Sueltas
    auto *card2 = new QFrame(contentWrapper);
    card2->setStyleSheet(
        "QFrame { background-color: #1f2335; border: 1px solid #3b4261; border-radius: 10px; }"
    );
    auto *c2Layout = new QVBoxLayout(card2);
    c2Layout->setContentsMargins(18, 18, 18, 18);
    c2Layout->setSpacing(12);

    auto *c2Header = new QHBoxLayout();
    auto *c2Icon = new QLabel(card2);
    c2Icon->setStyleSheet("border: none;");
    c2Icon->setPixmap(AppIcons::getPixmap(AppIcons::IconType::Page, QColor("#bb9af7"), 24));
    c2Header->addWidget(c2Icon);
    auto *c2Title = new QLabel("Hojas Sueltas", card2);
    c2Title->setStyleSheet("font-size: 15px; font-weight: bold; color: #bb9af7; border: none;");
    c2Header->addWidget(c2Title);
    c2Header->addStretch();
    m_standaloneCountLabel = new QLabel("0 hojas", card2);
    m_standaloneCountLabel->setStyleSheet("font-size: 10.5px; color: #7aa2f7; background: #24283b; padding: 3px 8px; border-radius: 4px; border: 1px solid #3b4261;");
    c2Header->addWidget(m_standaloneCountLabel);
    c2Layout->addLayout(c2Header);

    auto *c2Desc = new QLabel("Explora y retoma tus hojas de ejercicios y borradores independientes guardados.", card2);
    c2Desc->setWordWrap(true);
    c2Desc->setStyleSheet("font-size: 11.5px; color: #a9b1d6; border: none; line-height: 1.4;");
    c2Layout->addWidget(c2Desc, 1);

    auto *viewSheetsBtn = new QPushButton("Ver Hojas Sueltas", card2);
    viewSheetsBtn->setCursor(Qt::PointingHandCursor);
    viewSheetsBtn->setIcon(AppIcons::get(AppIcons::IconType::Page, QColor("#bb9af7"), 16));
    viewSheetsBtn->setStyleSheet(
        "QPushButton { background-color: #24283b; color: #c0caf5; border: 1px solid #3b4261; border-radius: 6px; padding: 9px 16px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #2e3c64; border-color: #bb9af7; color: #ffffff; }"
        "QPushButton:pressed { background-color: #1f2335; }"
    );
    connect(viewSheetsBtn, &QPushButton::clicked, this, &HomeHubView::onOpenSelectedStandaloneSheet);
    c2Layout->addWidget(viewSheetsBtn);
    cardsLayout->addWidget(card2, 1);

    // Card 3: Mis Cuadernos
    auto *card3 = new QFrame(contentWrapper);
    card3->setStyleSheet(
        "QFrame { background-color: #1f2335; border: 1px solid #3b4261; border-radius: 10px; }"
    );
    auto *c3Layout = new QVBoxLayout(card3);
    c3Layout->setContentsMargins(18, 18, 18, 18);
    c3Layout->setSpacing(12);

    auto *c3Header = new QHBoxLayout();
    auto *c3Icon = new QLabel(card3);
    c3Icon->setStyleSheet("border: none;");
    c3Icon->setPixmap(AppIcons::getPixmap(AppIcons::IconType::Notebook, QColor("#9ece6a"), 24));
    c3Header->addWidget(c3Icon);
    auto *c3Title = new QLabel("Mis Cuadernos", card3);
    c3Title->setStyleSheet("font-size: 15px; font-weight: bold; color: #9ece6a; border: none;");
    c3Header->addWidget(c3Title);
    c3Header->addStretch();
    m_notebooksCountLabel = new QLabel("0 cuadernos", card3);
    m_notebooksCountLabel->setStyleSheet("font-size: 10.5px; color: #9ece6a; background: #24283b; padding: 3px 8px; border-radius: 4px; border: 1px solid #3b4261;");
    c3Header->addWidget(m_notebooksCountLabel);
    c3Layout->addLayout(c3Header);

    auto *c3Desc = new QLabel("Biblioteca estructurada de materias académicas, cuadernos de clase y tareas.", card3);
    c3Desc->setWordWrap(true);
    c3Desc->setStyleSheet("font-size: 11.5px; color: #a9b1d6; border: none; line-height: 1.4;");
    c3Layout->addWidget(c3Desc, 1);

    auto *viewNotebooksBtn = new QPushButton("Abrir Biblioteca", card3);
    viewNotebooksBtn->setCursor(Qt::PointingHandCursor);
    viewNotebooksBtn->setIcon(AppIcons::get(AppIcons::IconType::Notebook, QColor("#9ece6a"), 16));
    viewNotebooksBtn->setStyleSheet(
        "QPushButton { background-color: #24283b; color: #c0caf5; border: 1px solid #3b4261; border-radius: 6px; padding: 9px 16px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #2e3c64; border-color: #9ece6a; color: #ffffff; }"
        "QPushButton:pressed { background-color: #1f2335; }"
    );
    connect(viewNotebooksBtn, &QPushButton::clicked, this, &HomeHubView::onOpenNotebooksClicked);
    c3Layout->addWidget(viewNotebooksBtn);
    cardsLayout->addWidget(card3, 1);

    mainLayout->addLayout(cardsLayout);

    // ==================== 3. Two Columns: Hojas Sueltas & Biblioteca ====================
    auto *columnsLayout = new QHBoxLayout();
    columnsLayout->setSpacing(20);

    // Left Column: Hojas Sueltas Recientes
    auto *leftCol = new QFrame(contentWrapper);
    leftCol->setStyleSheet(
        "QFrame { background-color: #1f2335; border: 1px solid #2f354f; border-radius: 10px; }"
    );
    auto *leftLayout = new QVBoxLayout(leftCol);
    leftLayout->setContentsMargins(18, 16, 18, 18);
    leftLayout->setSpacing(12);

    auto *leftHeader = new QHBoxLayout();
    auto *lhTitle = new QLabel("Hojas Sueltas Guardadas", leftCol);
    lhTitle->setStyleSheet("font-size: 13.5px; font-weight: bold; color: #c0caf5; border: none;");
    leftHeader->addWidget(lhTitle);
    leftHeader->addStretch();

    m_openSheetBtn = new QPushButton("Abrir Hoja", leftCol);
    m_openSheetBtn->setEnabled(false);
    m_openSheetBtn->setCursor(Qt::PointingHandCursor);
    m_openSheetBtn->setStyleSheet(
        "QPushButton { background-color: #3d59a1; color: #ffffff; border: none; border-radius: 5px; padding: 5px 12px; font-size: 11px; font-weight: bold; }"
        "QPushButton:hover { background-color: #4d6eb8; }"
        "QPushButton:disabled { background-color: #24283b; color: #565f89; }"
    );
    connect(m_openSheetBtn, &QPushButton::clicked, this, &HomeHubView::onOpenSelectedStandaloneSheet);
    leftHeader->addWidget(m_openSheetBtn);

    m_deleteSheetBtn = new QPushButton("Eliminar", leftCol);
    m_deleteSheetBtn->setEnabled(false);
    m_deleteSheetBtn->setCursor(Qt::PointingHandCursor);
    m_deleteSheetBtn->setStyleSheet(
        "QPushButton { background-color: #24283b; color: #f7768e; border: 1px solid #442b3b; border-radius: 5px; padding: 5px 10px; font-size: 11px; font-weight: bold; }"
        "QPushButton:hover { background-color: #442b3b; color: #ffffff; }"
        "QPushButton:disabled { background-color: #1f2335; color: #565f89; border-color: #2f354f; }"
    );
    connect(m_deleteSheetBtn, &QPushButton::clicked, this, &HomeHubView::onDeleteSelectedStandaloneSheet);
    leftHeader->addWidget(m_deleteSheetBtn);

    leftLayout->addLayout(leftHeader);

    m_sheetsList = new QListWidget(leftCol);
    m_sheetsList->setMinimumHeight(320);
    m_sheetsList->setStyleSheet(
        "QListWidget { background-color: #1a1b26; border: 1px solid #2f354f; border-radius: 6px; padding: 4px; color: #c0caf5; font-size: 12px; }"
        "QListWidget::item { padding: 9px 10px; border-radius: 4px; margin: 2px 0; }"
        "QListWidget::item:hover { background-color: #24283b; }"
        "QListWidget::item:selected { background-color: #3d59a1; color: #ffffff; font-weight: bold; }"
    );
    connect(m_sheetsList, &QListWidget::itemDoubleClicked, this, &HomeHubView::onStandaloneItemDoubleClicked);
    connect(m_sheetsList, &QListWidget::currentRowChanged, this, [this](int row) {
        bool hasSelection = (row >= 0);
        m_openSheetBtn->setEnabled(hasSelection);
        m_deleteSheetBtn->setEnabled(hasSelection);
    });
    leftLayout->addWidget(m_sheetsList, 1);
    columnsLayout->addWidget(leftCol, 6);

    // Right Column: Biblioteca de Cuadernos
    auto *rightCol = new QFrame(contentWrapper);
    rightCol->setStyleSheet(
        "QFrame { background-color: #1f2335; border: 1px solid #2f354f; border-radius: 10px; }"
    );
    auto *rightLayout = new QVBoxLayout(rightCol);
    rightLayout->setContentsMargins(18, 16, 18, 18);
    rightLayout->setSpacing(12);

    auto *rightHeader = new QHBoxLayout();
    auto *rhTitle = new QLabel("Materias y Cuadernos", rightCol);
    rhTitle->setStyleSheet("font-size: 13.5px; font-weight: bold; color: #c0caf5; border: none;");
    rightHeader->addWidget(rhTitle);
    rightHeader->addStretch();

    auto *openBibBtn = new QPushButton("Ir a Cuadernos", rightCol);
    openBibBtn->setCursor(Qt::PointingHandCursor);
    openBibBtn->setStyleSheet(
        "QPushButton { background-color: #24283b; color: #9ece6a; border: 1px solid #2e4d3a; border-radius: 5px; padding: 5px 12px; font-size: 11px; font-weight: bold; }"
        "QPushButton:hover { background-color: #2e4d3a; color: #ffffff; }"
    );
    connect(openBibBtn, &QPushButton::clicked, this, &HomeHubView::onOpenNotebooksClicked);
    rightHeader->addWidget(openBibBtn);

    rightLayout->addLayout(rightHeader);

    m_notebooksTree = new QTreeWidget(rightCol);
    m_notebooksTree->setHeaderHidden(true);
    m_notebooksTree->setAnimated(true);
    m_notebooksTree->setIndentation(18);
    m_notebooksTree->setMinimumHeight(320);
    m_notebooksTree->setStyleSheet(
        "QTreeWidget { background-color: #1a1b26; border: 1px solid #2f354f; border-radius: 6px; padding: 4px; color: #c0caf5; font-size: 12px; }"
        "QTreeWidget::item { padding: 7px 6px; border-radius: 4px; }"
        "QTreeWidget::item:hover { background-color: #24283b; }"
        "QTreeWidget::item:selected { background-color: #3d59a1; color: #ffffff; }"
    );
    connect(m_notebooksTree, &QTreeWidget::itemClicked, this, &HomeHubView::onNotebookItemClicked);
    rightLayout->addWidget(m_notebooksTree, 1);
    columnsLayout->addWidget(rightCol, 5);

    mainLayout->addLayout(columnsLayout);
}

void HomeHubView::refreshData() {
    // 1. Refresh Standalone Sheets
    m_sheetsList->clear();
    auto standalonePages = m_storage.getStandalonePages();
    m_standaloneCountLabel->setText(QString("%1 hoja%2").arg(standalonePages.size()).arg(standalonePages.size() == 1 ? "" : "s"));

    for (const auto &p : standalonePages) {
        QString label = p.title.trimmed().isEmpty() ? QString("Hoja Suelta %1").arg(p.pageNumber) : p.title.trimmed();
        auto *item = new QListWidgetItem(AppIcons::get(AppIcons::IconType::Page, QColor("#bb9af7"), 16), label, m_sheetsList);
        item->setData(Qt::UserRole, p.id);
        item->setToolTip(QString("ID: %1 • %2").arg(p.id).arg(label));
    }

    if (m_sheetsList->count() > 0) {
        m_sheetsList->setCurrentRow(0);
    } else {
        m_openSheetBtn->setEnabled(false);
        m_deleteSheetBtn->setEnabled(false);
    }

    // 2. Refresh Notebooks Tree
    m_notebooksTree->clear();
    auto subjects = m_storage.getSubjects(false);
    int totalNotebooks = 0;

    for (const auto &s : subjects) {
        auto *subItem = new QTreeWidgetItem(m_notebooksTree);
        subItem->setIcon(0, AppIcons::get(AppIcons::IconType::Folder, QColor("#7aa2f7"), 16));
        subItem->setText(0, s.name);
        subItem->setData(0, Qt::UserRole, s.id);
        subItem->setData(0, Qt::UserRole + 1, "subject");

        auto notebooks = m_storage.getNotebooks(s.id);
        totalNotebooks += notebooks.size();
        for (const auto &nb : notebooks) {
            auto *nbItem = new QTreeWidgetItem(subItem);
            nbItem->setIcon(0, AppIcons::get(AppIcons::IconType::Notebook, QColor("#9ece6a"), 16));
            nbItem->setText(0, nb.title);
            nbItem->setData(0, Qt::UserRole, nb.id);
            nbItem->setData(0, Qt::UserRole + 1, "notebook");
            nbItem->setData(0, Qt::UserRole + 2, s.id);
        }
        subItem->setExpanded(true);
    }

    m_notebooksCountLabel->setText(QString("%1 cuaderno%2").arg(totalNotebooks).arg(totalNotebooks == 1 ? "" : "s"));
}

void HomeHubView::onNewQuickSheetClicked() {
    emit newQuickSheetRequested();
}

void HomeHubView::onOpenNotebooksClicked() {
    emit openNotebooksRequested();
}

void HomeHubView::onOpenSelectedStandaloneSheet() {
    auto *item = m_sheetsList->currentItem();
    if (!item) {
        // If no sheet selected, create or open standalone mode
        emit newQuickSheetRequested();
        return;
    }
    qint64 pageId = item->data(Qt::UserRole).toLongLong();
    emit openStandalonePageRequested(pageId);
}

void HomeHubView::onStandaloneItemDoubleClicked(QListWidgetItem *item) {
    if (!item) return;
    qint64 pageId = item->data(Qt::UserRole).toLongLong();
    emit openStandalonePageRequested(pageId);
}

void HomeHubView::onDeleteSelectedStandaloneSheet() {
    auto *item = m_sheetsList->currentItem();
    if (!item) return;

    qint64 pageId = item->data(Qt::UserRole).toLongLong();
    QString title = item->text();

    if (QMessageBox::question(this, "Eliminar Hoja Suelta",
                              QString("¿Deseas eliminar '%1'?\nEsta acción no se puede deshacer.").arg(title),
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        m_storage.deletePage(pageId);
        refreshData();
    }
}

void HomeHubView::onNotebookItemClicked(QTreeWidgetItem *item, int /*column*/) {
    if (!item) return;
    QString type = item->data(0, Qt::UserRole + 1).toString();
    if (type == "notebook") {
        qint64 notebookId = item->data(0, Qt::UserRole).toLongLong();
        qint64 subjectId = item->data(0, Qt::UserRole + 2).toLongLong();
        emit openNotebookRequested(subjectId, notebookId);
    }
}

} // namespace OmniNote
