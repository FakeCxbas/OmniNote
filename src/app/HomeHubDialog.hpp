#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include "../storage/SqliteStorage.hpp"

namespace OmniNote {

class HomeHubDialog : public QDialog {
    Q_OBJECT

public:
    enum class HubAction {
        Dismiss,
        NewQuickSheet,
        OpenStandalonePage,
        OpenNotebooksLibrary
    };

    explicit HomeHubDialog(SqliteStorage &storage, QWidget *parent = nullptr);
    ~HomeHubDialog() override = default;

    HubAction selectedAction() const { return m_selectedAction; }
    qint64 selectedPageId() const { return m_selectedPageId; }
    bool showOnStartup() const;

signals:
    void newQuickSheetRequested();
    void openStandalonePageRequested(qint64 pageId);
    void openNotebooksRequested();

private slots:
    void onNewQuickSheetClicked();
    void onOpenNotebooksClicked();
    void onOpenSelectedStandaloneSheet();
    void onDeleteSelectedStandaloneSheet();
    void onStandaloneItemDoubleClicked(QListWidgetItem *item);
    void onStartupCheckToggled(bool checked);

private:
    void setupUI();
    void loadData();

    SqliteStorage &m_storage;
    HubAction m_selectedAction{HubAction::Dismiss};
    qint64 m_selectedPageId{-1};

    QLabel *m_standaloneCountLabel{nullptr};
    QLabel *m_notebooksCountLabel{nullptr};
    QListWidget *m_sheetsList{nullptr};
    QPushButton *m_openSheetBtn{nullptr};
    QPushButton *m_deleteSheetBtn{nullptr};
    QCheckBox *m_startupCheck{nullptr};
};

} // namespace OmniNote
