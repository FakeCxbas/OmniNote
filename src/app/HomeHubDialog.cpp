#include "HomeHubDialog.hpp"
#include "AppIcons.hpp"
#include <QSettings>
#include <QMessageBox>

namespace OmniNote {

HomeHubDialog::HomeHubDialog(SqliteStorage &storage, QWidget *parent)
    : QDialog(parent), m_storage(storage) {
    setWindowTitle("Centro de Inicio — OmniNote STEM");
    setMinimumSize(780, 560);
    resize(820, 580);
    setupUI();
    loadData();
}

void HomeHubDialog::setupUI() {
    setStyleSheet(
        "QDialog {"
        "   background-color: #16161e;"
        "   color: #c0caf5;"
        "   font-family: 'Inter', 'Segoe UI', sans-serif;"
        "}"
        "QFrame#Card {"
        "   background-color: #1a1b26;"
        "   border: 1.5px solid #24283b;"
        "   border-radius: 10px;"
        "   padding: 12px;"
        "}"
        "QFrame#HighlightCard {"
        "   background-color: #1e2238;"
        "   border: 1.5px solid #3d59a1;"
        "   border-radius: 10px;"
        "   padding: 12px;"
        "}"
        "QPushButton#ActionBtn {"
        "   background-color: #7aa2f7;"
        "   color: #15161e;"
        "   font-weight: bold;"
        "   border-radius: 6px;"
        "   padding: 9px 16px;"
        "   font-size: 12px;"
        "}"
        "QPushButton#ActionBtn:hover { background-color: #7dcfff; }"
        "QPushButton#SecondaryBtn {"
        "   background-color: #24283b;"
        "   color: #c0caf5;"
        "   border: 1px solid #3b4261;"
        "   border-radius: 6px;"
        "   padding: 8px 14px;"
        "   font-size: 11.5px;"
        "}"
        "QPushButton#SecondaryBtn:hover { background-color: #2f3549; color: #ffffff; }"
        "QPushButton#DangerBtn {"
        "   background-color: #3b242f;"
        "   color: #f7768e;"
        "   border: 1px solid #782e3b;"
        "   border-radius: 6px;"
        "   padding: 7px 12px;"
        "   font-size: 11px;"
        "}"
        "QPushButton#DangerBtn:hover { background-color: #f7768e; color: #15161e; }"
        "QListWidget {"
        "   background-color: #1a1b26;"
        "   border: 1px solid #24283b;"
        "   border-radius: 8px;"
        "   padding: 4px;"
        "   color: #c0caf5;"
        "   font-size: 11.5px;"
        "}"
        "QListWidget::item {"
        "   padding: 8px 10px;"
        "   border-radius: 5px;"
        "   margin-bottom: 2px;"
        "}"
        "QListWidget::item:hover { background-color: #24283b; }"
        "QListWidget::item:selected { background-color: #2e3c64; color: #ffffff; }"
        "QCheckBox {"
        "   color: #9aa5ce;"
        "   font-size: 11px;"
        "}"
        "QCheckBox::indicator {"
        "   width: 14px;"
        "   height: 14px;"
        "   border: 1px solid #3b4261;"
        "   border-radius: 3px;"
        "   background-color: #1a1b26;"
        "}"
        "QCheckBox::indicator:checked {"
        "   background-color: #7aa2f7;"
        "   border-color: #7aa2f7;"
        "}"
    );

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 20, 24, 20);
    mainLayout->setSpacing(16);

    // 1. Header
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(12);

    auto *iconLabel = new QLabel(this);
    iconLabel->setPixmap(AppIcons::getPixmap(AppIcons::IconType::Home, QColor("#7aa2f7"), 30));
    headerLayout->addWidget(iconLabel);

    auto *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(2);
    auto *titleLabel = new QLabel("Centro de Inicio — OmniNote STEM", this);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #7aa2f7; letter-spacing: 0.5px;");
    titleLayout->addWidget(titleLabel);

    auto *subTitleLabel = new QLabel("Crea una sola hoja en blanco al instante o accede a tu biblioteca organizada de cuadernos", this);
    subTitleLabel->setStyleSheet("font-size: 11.5px; color: #565f89;");
    titleLayout->addWidget(subTitleLabel);

    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    // 2. Action Cards Grid
    auto *cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(14);

    // Card 1: + Nueva Hoja Rapida
    auto *card1 = new QFrame(this);
    card1->setObjectName("HighlightCard");
    auto *c1Layout = new QVBoxLayout(card1);
    c1Layout->setContentsMargins(14, 14, 14, 14);
    c1Layout->setSpacing(10);

    auto *c1Header = new QHBoxLayout();
    auto *c1Icon = new QLabel(card1);
    c1Icon->setPixmap(AppIcons::getPixmap(AppIcons::IconType::Plus, QColor("#7aa2f7"), 22));
    c1Header->addWidget(c1Icon);
    auto *c1Title = new QLabel("Nueva Hoja Rápida", card1);
    c1Title->setStyleSheet("font-size: 13.5px; font-weight: bold; color: #7aa2f7;");
    c1Header->addWidget(c1Title);
    c1Header->addStretch();
    c1Layout->addLayout(c1Header);

    auto *c1Desc = new QLabel("Lienzo en blanco instantáneo sin requerir materias ni cuadernos. Ideal para apuntes de hoy o cálculos.", card1);
    c1Desc->setWordWrap(true);
    c1Desc->setStyleSheet("font-size: 11px; color: #a9b1d6; line-height: 1.4;");
    c1Layout->addWidget(c1Desc, 1);

    auto *newSheetBtn = new QPushButton("  Crear Hoja en Blanco", card1);
    newSheetBtn->setObjectName("ActionBtn");
    newSheetBtn->setIcon(AppIcons::get(AppIcons::IconType::Plus, QColor("#15161e"), 16));
    newSheetBtn->setCursor(Qt::PointingHandCursor);
    connect(newSheetBtn, &QPushButton::clicked, this, &HomeHubDialog::onNewQuickSheetClicked);
    c1Layout->addWidget(newSheetBtn);
    cardsLayout->addWidget(card1, 1);

    // Card 2: Hojas Sueltas
    auto *card2 = new QFrame(this);
    card2->setObjectName("Card");
    auto *c2Layout = new QVBoxLayout(card2);
    c2Layout->setContentsMargins(14, 14, 14, 14);
    c2Layout->setSpacing(10);

    auto *c2Header = new QHBoxLayout();
    auto *c2Icon = new QLabel(card2);
    c2Icon->setPixmap(AppIcons::getPixmap(AppIcons::IconType::Page, QColor("#bb9af7"), 22));
    c2Header->addWidget(c2Icon);
    auto *c2Title = new QLabel("Hojas Sueltas", card2);
    c2Title->setStyleSheet("font-size: 13.5px; font-weight: bold; color: #bb9af7;");
    c2Header->addWidget(c2Title);
    c2Header->addStretch();
    m_standaloneCountLabel = new QLabel("0 hojas", card2);
    m_standaloneCountLabel->setStyleSheet("font-size: 10px; color: #7aa2f7; background: #24283b; padding: 2px 6px; border-radius: 4px;");
    c2Header->addWidget(m_standaloneCountLabel);
    c2Layout->addLayout(c2Header);

    auto *c2Desc = new QLabel("Accede al historial de hojas de notas y ejercicios independientes que has creado previamente.", card2);
    c2Desc->setWordWrap(true);
    c2Desc->setStyleSheet("font-size: 11px; color: #a9b1d6; line-height: 1.4;");
    c2Layout->addWidget(c2Desc, 1);

    auto *viewSheetsBtn = new QPushButton("Ver Hojas Sueltas", card2);
    viewSheetsBtn->setObjectName("SecondaryBtn");
    viewSheetsBtn->setCursor(Qt::PointingHandCursor);
    connect(viewSheetsBtn, &QPushButton::clicked, this, [this]() {
        if (m_sheetsList && m_sheetsList->count() > 0) {
            m_sheetsList->setFocus();
            if (m_sheetsList->currentRow() < 0) {
                m_sheetsList->setCurrentRow(0);
            }
        }
    });
    c2Layout->addWidget(viewSheetsBtn);
    cardsLayout->addWidget(card2, 1);

    // Card 3: Mis Cuadernos
    auto *card3 = new QFrame(this);
    card3->setObjectName("Card");
    auto *c3Layout = new QVBoxLayout(card3);
    c3Layout->setContentsMargins(14, 14, 14, 14);
    c3Layout->setSpacing(10);

    auto *c3Header = new QHBoxLayout();
    auto *c3Icon = new QLabel(card3);
    c3Icon->setPixmap(AppIcons::getPixmap(AppIcons::IconType::Notebook, QColor("#9ece6a"), 22));
    c3Header->addWidget(c3Icon);
    auto *c3Title = new QLabel("Mis Cuadernos", card3);
    c3Title->setStyleSheet("font-size: 13.5px; font-weight: bold; color: #9ece6a;");
    c3Header->addWidget(c3Title);
    c3Header->addStretch();
    m_notebooksCountLabel = new QLabel("0 cuadernos", card3);
    m_notebooksCountLabel->setStyleSheet("font-size: 10px; color: #9ece6a; background: #24283b; padding: 2px 6px; border-radius: 4px;");
    c3Header->addWidget(m_notebooksCountLabel);
    c3Layout->addLayout(c3Header);

    auto *c3Desc = new QLabel("Explora y gestiona tus materias académicas completas con sus cuadernos estructurados por páginas.", card3);
    c3Desc->setWordWrap(true);
    c3Desc->setStyleSheet("font-size: 11px; color: #a9b1d6; line-height: 1.4;");
    c3Layout->addWidget(c3Desc, 1);

    auto *openNotebooksBtn = new QPushButton("Explorar Cuadernos", card3);
    openNotebooksBtn->setObjectName("SecondaryBtn");
    openNotebooksBtn->setCursor(Qt::PointingHandCursor);
    connect(openNotebooksBtn, &QPushButton::clicked, this, &HomeHubDialog::onOpenNotebooksClicked);
    c3Layout->addWidget(openNotebooksBtn);
    cardsLayout->addWidget(card3, 1);

    mainLayout->addLayout(cardsLayout);

    // 3. Standalone Sheets List Section
    auto *listHeaderLayout = new QHBoxLayout();
    auto *listLabel = new QLabel("TUS HOJAS SUELTAS INDEPENDIENTES", this);
    listLabel->setStyleSheet("color: #7aa2f7; font-size: 10.5px; font-weight: bold; letter-spacing: 1px;");
    listHeaderLayout->addWidget(listLabel);
    listHeaderLayout->addStretch();

    m_openSheetBtn = new QPushButton("Abrir Hoja", this);
    m_openSheetBtn->setObjectName("SecondaryBtn");
    m_openSheetBtn->setEnabled(false);
    m_openSheetBtn->setCursor(Qt::PointingHandCursor);
    connect(m_openSheetBtn, &QPushButton::clicked, this, &HomeHubDialog::onOpenSelectedStandaloneSheet);
    listHeaderLayout->addWidget(m_openSheetBtn);

    m_deleteSheetBtn = new QPushButton("Eliminar Hoja", this);
    m_deleteSheetBtn->setObjectName("DangerBtn");
    m_deleteSheetBtn->setEnabled(false);
    m_deleteSheetBtn->setCursor(Qt::PointingHandCursor);
    connect(m_deleteSheetBtn, &QPushButton::clicked, this, &HomeHubDialog::onDeleteSelectedStandaloneSheet);
    listHeaderLayout->addWidget(m_deleteSheetBtn);

    mainLayout->addLayout(listHeaderLayout);

    m_sheetsList = new QListWidget(this);
    connect(m_sheetsList, &QListWidget::itemDoubleClicked, this, &HomeHubDialog::onStandaloneItemDoubleClicked);
    connect(m_sheetsList, &QListWidget::currentRowChanged, this, [this](int row) {
        bool hasSelection = (row >= 0);
        m_openSheetBtn->setEnabled(hasSelection);
        m_deleteSheetBtn->setEnabled(hasSelection);
    });
    mainLayout->addWidget(m_sheetsList, 1);

    // 4. Footer
    auto *footerLayout = new QHBoxLayout();
    QSettings settings("OmniNote", "OmniNoteSTEM");
    bool showOnStartup = settings.value("show_home_hub_on_startup", true).toBool();

    m_startupCheck = new QCheckBox("Mostrar esta pantalla al iniciar OmniNote", this);
    m_startupCheck->setChecked(showOnStartup);
    connect(m_startupCheck, &QCheckBox::toggled, this, &HomeHubDialog::onStartupCheckToggled);
    footerLayout->addWidget(m_startupCheck);

    footerLayout->addStretch();

    auto *closeBtn = new QPushButton("Continuar al Lienzo", this);
    closeBtn->setObjectName("SecondaryBtn");
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, this, [this]() {
        m_selectedAction = HubAction::Dismiss;
        accept();
    });
    footerLayout->addWidget(closeBtn);

    mainLayout->addLayout(footerLayout);
}

void HomeHubDialog::loadData() {
    m_sheetsList->clear();

    auto standalonePages = m_storage.getStandalonePages();
    m_standaloneCountLabel->setText(QString("%1 hoja%2").arg(standalonePages.size()).arg(standalonePages.size() == 1 ? "" : "s"));

    for (const auto &p : standalonePages) {
        QString label = p.title.trimmed().isEmpty() ? QString("Hoja Suelta %1").arg(p.pageNumber) : p.title.trimmed();
        auto *item = new QListWidgetItem(AppIcons::get(AppIcons::IconType::Page, QColor("#bb9af7"), 16), label, m_sheetsList);
        item->setData(Qt::UserRole, p.id);
        item->setToolTip(QString("ID: %1 • %2").arg(p.id).arg(label));
    }

    auto subjects = m_storage.getSubjects(false);
    int totalNotebooks = 0;
    for (const auto &s : subjects) {
        totalNotebooks += m_storage.getNotebooks(s.id).size();
    }
    m_notebooksCountLabel->setText(QString("%1 cuaderno%2").arg(totalNotebooks).arg(totalNotebooks == 1 ? "" : "s"));

    if (m_sheetsList->count() > 0) {
        m_sheetsList->setCurrentRow(0);
    }
}

void HomeHubDialog::onNewQuickSheetClicked() {
    m_selectedAction = HubAction::NewQuickSheet;
    emit newQuickSheetRequested();
    accept();
}

void HomeHubDialog::onOpenNotebooksClicked() {
    m_selectedAction = HubAction::OpenNotebooksLibrary;
    emit openNotebooksRequested();
    accept();
}

void HomeHubDialog::onOpenSelectedStandaloneSheet() {
    auto *item = m_sheetsList->currentItem();
    if (!item) return;

    m_selectedPageId = item->data(Qt::UserRole).toLongLong();
    m_selectedAction = HubAction::OpenStandalonePage;
    emit openStandalonePageRequested(m_selectedPageId);
    accept();
}

void HomeHubDialog::onStandaloneItemDoubleClicked(QListWidgetItem *item) {
    if (!item) return;
    m_selectedPageId = item->data(Qt::UserRole).toLongLong();
    m_selectedAction = HubAction::OpenStandalonePage;
    emit openStandalonePageRequested(m_selectedPageId);
    accept();
}

void HomeHubDialog::onDeleteSelectedStandaloneSheet() {
    auto *item = m_sheetsList->currentItem();
    if (!item) return;

    qint64 pageId = item->data(Qt::UserRole).toLongLong();
    QString title = item->text();

    if (QMessageBox::question(this, "Eliminar Hoja Suelta",
                              QString("¿Deseas eliminar '%1'?").arg(title),
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        m_storage.deletePage(pageId);
        loadData();
    }
}

void HomeHubDialog::onStartupCheckToggled(bool checked) {
    QSettings settings("OmniNote", "OmniNoteSTEM");
    settings.setValue("show_home_hub_on_startup", checked);
}

bool HomeHubDialog::showOnStartup() const {
    return m_startupCheck ? m_startupCheck->isChecked() : true;
}

} // namespace OmniNote
