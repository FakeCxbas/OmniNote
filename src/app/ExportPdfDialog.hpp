#pragma once

#include <QDialog>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QListWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QVector>
#include "../storage/SqliteStorage.hpp"

namespace OmniNote {

class ExportPdfDialog : public QDialog {
    Q_OBJECT

public:
    enum class ExportScope {
        CurrentPage,
        AllPages,
        CustomRange
    };

    explicit ExportPdfDialog(const QVector<PageRecord> &availablePages,
                             qint64 currentPageId,
                             const QString &notebookTitle,
                             QWidget *parent = nullptr);
    ~ExportPdfDialog() override = default;

    ExportScope exportScope() const;
    QVector<PageRecord> selectedPages() const;
    int dpi() const;
    QString suggestedFileName() const;

private slots:
    void onScopeChanged();
    void onRangeTextChanged(const QString &text);
    void onPageItemChanged(QListWidgetItem *item);
    void onSelectAllPages();
    void onDeselectAllPages();

private:
    void setupUI();
    void updateRangeTextFromChecks();
    void updateChecksFromRangeText();

    QVector<PageRecord> m_pages;
    qint64 m_currentPageId{-1};
    QString m_notebookTitle;

    QRadioButton *m_currentPageRadio{nullptr};
    QRadioButton *m_allPagesRadio{nullptr};
    QRadioButton *m_customRangeRadio{nullptr};

    QWidget *m_customContainer{nullptr};
    QLineEdit *m_rangeLineEdit{nullptr};
    QListWidget *m_pagesListWidget{nullptr};
    QPushButton *m_selectAllBtn{nullptr};
    QPushButton *m_deselectAllBtn{nullptr};

    QComboBox *m_dpiCombo{nullptr};
    bool m_updatingChecks{false};
};

} // namespace OmniNote
