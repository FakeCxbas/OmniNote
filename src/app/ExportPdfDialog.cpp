#include "ExportPdfDialog.hpp"
#include "AppIcons.hpp"
#include <QSet>
#include <algorithm>

namespace OmniNote {

static QSet<int> parseRangeString(const QString &text, int maxPages) {
    QSet<int> result;
    QStringList parts = text.split(',', Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        QString p = part.trimmed();
        if (p.contains('-')) {
            QStringList bounds = p.split('-');
            if (bounds.size() == 2) {
                bool ok1, ok2;
                int start = bounds[0].trimmed().toInt(&ok1);
                int end = bounds[1].trimmed().toInt(&ok2);
                if (ok1 && ok2) {
                    if (start > end) std::swap(start, end);
                    for (int i = start; i <= end; ++i) {
                        if (i >= 1 && i <= maxPages) result.insert(i);
                    }
                }
            }
        } else {
            bool ok;
            int page = p.toInt(&ok);
            if (ok && page >= 1 && page <= maxPages) {
                result.insert(page);
            }
        }
    }
    return result;
}

ExportPdfDialog::ExportPdfDialog(const QVector<PageRecord> &availablePages,
                                 qint64 currentPageId,
                                 const QString &notebookTitle,
                                 QWidget *parent)
    : QDialog(parent),
      m_pages(availablePages),
      m_currentPageId(currentPageId),
      m_notebookTitle(notebookTitle) {
    setWindowTitle("Exportar a PDF — Estilo Canva");
    setMinimumWidth(520);
    setWindowIcon(AppIcons::get(AppIcons::IconType::ExportPdf, QColor("#e0af68"), 24));
    setStyleSheet(
        "QDialog { background-color: #1a1b26; color: #c0caf5; font-family: 'Inter', 'Segoe UI', sans-serif; }"
        "QFrame#Card { background-color: #1f2335; border: 1px solid #2f354f; border-radius: 8px; }"
        "QRadioButton { font-size: 13px; color: #c0caf5; spacing: 8px; }"
        "QRadioButton::indicator { width: 16px; height: 16px; border-radius: 8px; border: 2px solid #565f89; background: #1a1b26; }"
        "QRadioButton::indicator:checked { border-color: #7aa2f7; background: #7aa2f7; }"
        "QLineEdit { background-color: #14161f; color: #c0caf5; border: 1px solid #3b4261; border-radius: 6px; padding: 6px 10px; font-size: 12px; }"
        "QLineEdit:focus { border-color: #7aa2f7; }"
        "QListWidget { background-color: #14161f; color: #c0caf5; border: 1px solid #2f354f; border-radius: 6px; padding: 4px; font-size: 12px; }"
        "QListWidget::item { padding: 4px; }"
        "QListWidget::item:hover { background-color: #24283b; border-radius: 4px; }"
        "QComboBox { background-color: #1f2335; color: #c0caf5; border: 1px solid #3b4261; border-radius: 6px; padding: 6px 10px; font-size: 12px; }"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox QAbstractItemView { background-color: #1f2335; color: #c0caf5; selection-background-color: #3d59a1; }"
    );

    setupUI();
}

void ExportPdfDialog::setupUI() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(22, 20, 22, 20);
    mainLayout->setSpacing(16);

    // 1. Header
    auto *headerLayout = new QHBoxLayout();
    auto *iconLabel = new QLabel(this);
    iconLabel->setPixmap(AppIcons::getPixmap(AppIcons::IconType::ExportPdf, QColor("#e0af68"), 32));
    headerLayout->addWidget(iconLabel);

    auto *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(2);
    auto *titleLabel = new QLabel("Exportar Documento a PDF", this);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #c0caf5;");
    titleLayout->addWidget(titleLabel);

    auto *descLabel = new QLabel("Descarga tus apuntes en alta resolución vectorial con calidad de impresión.", this);
    descLabel->setStyleSheet("font-size: 11px; color: #7aa2f7;");
    titleLayout->addWidget(descLabel);
    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    // 2. Card: Selección de Páginas (Estilo Canva)
    auto *scopeCard = new QFrame(this);
    scopeCard->setObjectName("Card");
    auto *scopeLayout = new QVBoxLayout(scopeCard);
    scopeLayout->setContentsMargins(16, 14, 16, 16);
    scopeLayout->setSpacing(12);

    auto *scopeTitle = new QLabel("SELECCIÓN DE PÁGINAS", scopeCard);
    scopeTitle->setStyleSheet("font-size: 10.5px; font-weight: bold; color: #7aa2f7; letter-spacing: 0.5px;");
    scopeLayout->addWidget(scopeTitle);

    // Find current page info
    int currentNum = 1;
    QString currentTitle;
    for (const auto &p : m_pages) {
        if (p.id == m_currentPageId) {
            currentNum = p.pageNumber;
            currentTitle = p.title.trimmed();
            break;
        }
    }
    if (currentTitle.isEmpty()) {
        currentTitle = QString("Página %1").arg(currentNum);
    }

    // Radio 1: Página actual
    m_currentPageRadio = new QRadioButton(QString("Página actual (Pág. %1 • %2)").arg(currentNum).arg(currentTitle), scopeCard);
    m_currentPageRadio->setChecked(true);
    connect(m_currentPageRadio, &QRadioButton::toggled, this, &ExportPdfDialog::onScopeChanged);
    scopeLayout->addWidget(m_currentPageRadio);

    // Radio 2: Todas las páginas
    m_allPagesRadio = new QRadioButton(QString("Todas las páginas (%1 páginas en total)").arg(m_pages.size()), scopeCard);
    connect(m_allPagesRadio, &QRadioButton::toggled, this, &ExportPdfDialog::onScopeChanged);
    scopeLayout->addWidget(m_allPagesRadio);

    // Radio 3: Varias hojas / Rango personalizado
    m_customRangeRadio = new QRadioButton("Páginas personalizadas / Varias hojas", scopeCard);
    connect(m_customRangeRadio, &QRadioButton::toggled, this, &ExportPdfDialog::onScopeChanged);
    scopeLayout->addWidget(m_customRangeRadio);

    // Custom range container
    m_customContainer = new QWidget(scopeCard);
    auto *custLayout = new QVBoxLayout(m_customContainer);
    custLayout->setContentsMargins(24, 0, 0, 0);
    custLayout->setSpacing(8);

    auto *rangeInputLayout = new QHBoxLayout();
    auto *rangeLabel = new QLabel("Rango:", m_customContainer);
    rangeLabel->setStyleSheet("color: #a9b1d6; font-size: 11.5px;");
    rangeInputLayout->addWidget(rangeLabel);

    m_rangeLineEdit = new QLineEdit(m_customContainer);
    m_rangeLineEdit->setPlaceholderText("ej. 1-3, 5, 12");
    m_rangeLineEdit->setText(QString::number(currentNum));
    connect(m_rangeLineEdit, &QLineEdit::textChanged, this, &ExportPdfDialog::onRangeTextChanged);
    rangeInputLayout->addWidget(m_rangeLineEdit, 1);
    custLayout->addLayout(rangeInputLayout);

    // Interactive checklist of pages
    m_pagesListWidget = new QListWidget(m_customContainer);
    m_pagesListWidget->setMaximumHeight(140);

    for (const auto &p : m_pages) {
        QString label = p.title.trimmed().isEmpty()
            ? QString("Página %1").arg(p.pageNumber)
            : QString("Pág. %1 • %2").arg(p.pageNumber).arg(p.title.trimmed());
        auto *item = new QListWidgetItem(label, m_pagesListWidget);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(p.id == m_currentPageId ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, p.id);
        item->setData(Qt::UserRole + 1, p.pageNumber);
    }
    connect(m_pagesListWidget, &QListWidget::itemChanged, this, &ExportPdfDialog::onPageItemChanged);
    custLayout->addWidget(m_pagesListWidget);

    auto *quickBtnsLayout = new QHBoxLayout();
    m_selectAllBtn = new QPushButton("Seleccionar todas", m_customContainer);
    m_selectAllBtn->setCursor(Qt::PointingHandCursor);
    m_selectAllBtn->setStyleSheet("background: #24283b; color: #7aa2f7; border: 1px solid #3b4261; border-radius: 4px; padding: 4px 8px; font-size: 11px;");
    connect(m_selectAllBtn, &QPushButton::clicked, this, &ExportPdfDialog::onSelectAllPages);
    quickBtnsLayout->addWidget(m_selectAllBtn);

    m_deselectAllBtn = new QPushButton("Deseleccionar todas", m_customContainer);
    m_deselectAllBtn->setCursor(Qt::PointingHandCursor);
    m_deselectAllBtn->setStyleSheet("background: #24283b; color: #a9b1d6; border: 1px solid #3b4261; border-radius: 4px; padding: 4px 8px; font-size: 11px;");
    connect(m_deselectAllBtn, &QPushButton::clicked, this, &ExportPdfDialog::onDeselectAllPages);
    quickBtnsLayout->addWidget(m_deselectAllBtn);
    quickBtnsLayout->addStretch();
    custLayout->addLayout(quickBtnsLayout);

    scopeLayout->addWidget(m_customContainer);
    m_customContainer->setVisible(false);

    mainLayout->addWidget(scopeCard);

    // 3. Card: Configuración de Salida
    auto *optCard = new QFrame(this);
    optCard->setObjectName("Card");
    auto *optLayout = new QHBoxLayout(optCard);
    optLayout->setContentsMargins(16, 12, 16, 12);
    optLayout->setSpacing(12);

    auto *dpiLabel = new QLabel("Calidad de Impresión:", optCard);
    dpiLabel->setStyleSheet("color: #c0caf5; font-size: 12px; font-weight: bold;");
    optLayout->addWidget(dpiLabel);

    m_dpiCombo = new QComboBox(optCard);
    m_dpiCombo->addItem("Alta Resolución Vectorial (300 DPI — Canva)", 300);
    m_dpiCombo->addItem("Calidad Estándar Digital (150 DPI)", 150);
    m_dpiCombo->addItem("Ultra Alta Definición (600 DPI)", 600);
    optLayout->addWidget(m_dpiCombo, 1);

    mainLayout->addWidget(optCard);

    // 4. Action Buttons
    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    auto *cancelBtn = new QPushButton("Cancelar", this);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setStyleSheet("background: #24283b; color: #c0caf5; border: 1px solid #3b4261; border-radius: 6px; padding: 8px 16px; font-size: 12px; font-weight: bold;");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    auto *exportBtn = new QPushButton("Exportar PDF", this);
    exportBtn->setCursor(Qt::PointingHandCursor);
    exportBtn->setStyleSheet("background: #7aa2f7; color: #15161e; border: none; border-radius: 6px; padding: 8px 22px; font-size: 12px; font-weight: bold;");
    connect(exportBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(exportBtn);

    mainLayout->addLayout(btnLayout);
}

void ExportPdfDialog::onScopeChanged() {
    bool isCustom = m_customRangeRadio->isChecked();
    m_customContainer->setVisible(isCustom);
    adjustSize();
}

void ExportPdfDialog::onRangeTextChanged(const QString &text) {
    if (m_updatingChecks) return;
    m_updatingChecks = true;

    QSet<int> pageNums = parseRangeString(text, m_pages.size());
    for (int i = 0; i < m_pagesListWidget->count(); ++i) {
        auto *item = m_pagesListWidget->item(i);
        int num = item->data(Qt::UserRole + 1).toInt();
        item->setCheckState(pageNums.contains(num) ? Qt::Checked : Qt::Unchecked);
    }

    m_updatingChecks = false;
}

void ExportPdfDialog::onPageItemChanged(QListWidgetItem * /*item*/) {
    if (m_updatingChecks) return;
    updateRangeTextFromChecks();
}

void ExportPdfDialog::updateRangeTextFromChecks() {
    m_updatingChecks = true;
    QStringList nums;
    for (int i = 0; i < m_pagesListWidget->count(); ++i) {
        auto *item = m_pagesListWidget->item(i);
        if (item->checkState() == Qt::Checked) {
            nums.append(QString::number(item->data(Qt::UserRole + 1).toInt()));
        }
    }
    m_rangeLineEdit->setText(nums.join(", "));
    m_updatingChecks = false;
}

void ExportPdfDialog::onSelectAllPages() {
    m_updatingChecks = true;
    for (int i = 0; i < m_pagesListWidget->count(); ++i) {
        m_pagesListWidget->item(i)->setCheckState(Qt::Checked);
    }
    m_updatingChecks = false;
    updateRangeTextFromChecks();
}

void ExportPdfDialog::onDeselectAllPages() {
    m_updatingChecks = true;
    for (int i = 0; i < m_pagesListWidget->count(); ++i) {
        m_pagesListWidget->item(i)->setCheckState(Qt::Unchecked);
    }
    m_updatingChecks = false;
    m_rangeLineEdit->clear();
}

ExportPdfDialog::ExportScope ExportPdfDialog::exportScope() const {
    if (m_currentPageRadio->isChecked()) return ExportScope::CurrentPage;
    if (m_allPagesRadio->isChecked()) return ExportScope::AllPages;
    return ExportScope::CustomRange;
}

QVector<PageRecord> ExportPdfDialog::selectedPages() const {
    QVector<PageRecord> result;

    if (m_currentPageRadio->isChecked()) {
        for (const auto &p : m_pages) {
            if (p.id == m_currentPageId) {
                result.append(p);
                break;
            }
        }
        if (result.isEmpty() && !m_pages.isEmpty()) {
            result.append(m_pages.first());
        }
        return result;
    }

    if (m_allPagesRadio->isChecked()) {
        return m_pages;
    }

    // Custom Range
    QSet<int> pageNums = parseRangeString(m_rangeLineEdit->text(), m_pages.size());
    // Also include any items checked in list
    for (int i = 0; i < m_pagesListWidget->count(); ++i) {
        auto *it = m_pagesListWidget->item(i);
        if (it->checkState() == Qt::Checked) {
            pageNums.insert(it->data(Qt::UserRole + 1).toInt());
        }
    }

    for (const auto &p : m_pages) {
        if (pageNums.contains(p.pageNumber)) {
            result.append(p);
        }
    }

    // If somehow empty, fallback to current page
    if (result.isEmpty()) {
        for (const auto &p : m_pages) {
            if (p.id == m_currentPageId) {
                result.append(p);
                break;
            }
        }
        if (result.isEmpty() && !m_pages.isEmpty()) {
            result.append(m_pages.first());
        }
    }

    return result;
}

int ExportPdfDialog::dpi() const {
    return m_dpiCombo ? m_dpiCombo->currentData().toInt() : 300;
}

QString ExportPdfDialog::suggestedFileName() const {
    if (m_currentPageRadio->isChecked()) {
        for (const auto &p : m_pages) {
            if (p.id == m_currentPageId) {
                QString t = p.title.trimmed();
                return t.isEmpty() ? QString("Hoja_%1.pdf").arg(p.pageNumber) : QString("%1.pdf").arg(t);
            }
        }
    }

    QString nb = m_notebookTitle.trimmed();
    if (!nb.isEmpty() && nb != "Hojas Sueltas") {
        return QString("%1.pdf").arg(nb);
    }

    return QStringLiteral("Cuaderno_OmniNote.pdf");
}

} // namespace OmniNote
