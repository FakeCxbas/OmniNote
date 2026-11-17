#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QScrollArea>
#include "../storage/SqliteStorage.hpp"

namespace OmniNote {

class HomeHubView : public QWidget {
    Q_OBJECT

public:
    explicit HomeHubView(SqliteStorage &storage, QWidget *parent = nullptr);
    ~HomeHubView() override = default;

    void refreshData();

signals:
    void newQuickSheetRequested();
    void openStandalonePageRequested(qint64 pageId);
    void openNotebooksRequested();
    void openNotebookRequested(qint64 subjectId, qint64 notebookId);
    void returnToCanvasRequested();

private slots:
    void onNewQuickSheetClicked();
    void onOpenNotebooksClicked();
    void onOpenSelectedStandaloneSheet();
    void onDeleteSelectedStandaloneSheet();
    void onStandaloneItemDoubleClicked(QListWidgetItem *item);
    void onNotebookItemClicked(QTreeWidgetItem *item, int column);

private:
    void setupUI();

    SqliteStorage &m_storage;

    QLabel *m_standaloneCountLabel{nullptr};
    QLabel *m_notebooksCountLabel{nullptr};
    QListWidget *m_sheetsList{nullptr};
    QTreeWidget *m_notebooksTree{nullptr};
    QPushButton *m_openSheetBtn{nullptr};
    QPushButton *m_deleteSheetBtn{nullptr};
};

} // namespace OmniNote
