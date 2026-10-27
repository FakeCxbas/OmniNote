#pragma once

#include <QMainWindow>
#include <QTreeWidget>
#include <QListWidget>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QProgressBar>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QActionGroup>
#include <QSplitter>
#include <QCloseEvent>
#include <QTimer>
#include <memory>
#include "../canvas/VectorCanvasView.hpp"
#include "../canvas/VectorCanvasScene.hpp"
#include "../canvas/StrokeWidthControl.hpp"
#include "../canvas/ColorPaletteControl.hpp"
#include "../pdf/PdfManager.hpp"
#include "../storage/SqliteStorage.hpp"
#include "../sync/GoogleDriveSync.hpp"

namespace OmniNote {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    // Tool selection slots
    void onToolTriggered(QAction *action);
    void onShapeTypeChanged(int index);
    void onGridTypeChanged(int index);
    void onStrokeWidthChanged(double width);
    void onColorSelected(const QColor &color);
    void onCustomColorDialog();

    // STEM actions
    void onInsertTextBlock();
    void onInsertCodeBlock();
    void onPlotFunctionDialog();
    void onInsertImageFromFile();
    void onPasteFromClipboard();
    void onInsertFormulaBlock();
    void onToggleVirtualRuler();
    void insertImage(const QImage &img, const QPointF &pos = QPointF());

    // PDF Import & Export
    void onImportPdf();
    void onExportPdf();

    // Storage and Notebook Management
    void onAddSubject();
    void onAddNotebook();
    void onAddPage();
    void onDeleteItem();
    void onDeleteCurrentPage();
    void onRestoreLastDeletedPage();
    void onSubjectTreeItemClicked(QTreeWidgetItem *item, int column);
    void onPageListSelectionChanged(int row);
    void onSaveCurrentPage();
    void onPeriodicAutoSave();

    // Status bar updates
    void updateCoordinateDisplay(const QPointF &pos);
    void updatePressureDisplay(qreal pressure);
    void updateZoomDisplay(qreal zoom);
    void updatePointerTipDisplay(bool isEraser);

    void toggleSidebar();
    void onInsertTextBlockAt(const QPointF &pos);
    void showCloudSyncDialog();
    void showShortcutsGuide();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupUI();
    void setupToolbars();
    void setupSidebar();
    void setupStatusBar();
    void setupShortcuts();
    void loadInitialData();
    void switchPage(qint64 pageId);
    void markModified();
    void ensureActivePageForSave();
    void syncToLocalCloudFolder(bool notifyUser = false);
    std::unique_ptr<VectorCanvasScene> createSceneForPage(const PageRecord &p);
    QVector<GoogleDriveSyncService::SyncFileEntry> prepareNotebookExportFiles(const QString &targetDir);

    // Core Canvas & Scene
    VectorCanvasScene *m_currentScene{nullptr};
    VectorCanvasView *m_canvasView{nullptr};

    // Subsystems
    PdfManager m_pdfManager;
    SqliteStorage m_storage;
    QTimer *m_autoSaveTimer{nullptr};
    bool m_hasUnsavedChanges{false};
    bool m_isSaving{false};

    // Active Database IDs
    qint64 m_currentSubjectId{-1};
    qint64 m_currentNotebookId{-1};
    qint64 m_currentPageId{-1};

    // Sidebar Widgets
    QSplitter *m_mainSplitter{nullptr};
    QWidget *m_sidebarContainer{nullptr};
    QTreeWidget *m_subjectsTree{nullptr};
    QListWidget *m_pageList{nullptr};
    QPushButton *m_restorePageBtn{nullptr};

    struct DeletedPageBackup {
        qint64 originalNotebookId{-1};
        int originalPageNumber{1};
        PageRecord pageRecord;
        QVector<StrokeRecord> strokes;
        QVector<StemBlockRecord> blocks;
    };
    QVector<DeletedPageBackup> m_pageTrash;

    // Toolbar Widgets
    QAction *m_undoAction{nullptr};
    QAction *m_redoAction{nullptr};
    QActionGroup *m_toolActionGroup{nullptr};
    QAction *m_penAction{nullptr};
    QAction *m_highlighterAction{nullptr};
    QAction *m_eraserAction{nullptr};
    QAction *m_lassoAction{nullptr};
    QAction *m_shapeAction{nullptr};
    QAction *m_textBlockAction{nullptr};
    QAction *m_codeBlockAction{nullptr};
    QAction *m_plotAction{nullptr};
    QAction *m_imageAction{nullptr};
    QAction *m_formulaAction{nullptr};
    QAction *m_rulerAction{nullptr};
    QAction *m_cloudSyncAction{nullptr};
    QAction *m_helpAction{nullptr};
    class VirtualRulerItem *m_virtualRuler{nullptr};
    GoogleDriveSyncService *m_driveSyncService{nullptr};

    QComboBox *m_shapeCombo{nullptr};
    QComboBox *m_gridCombo{nullptr};
    StrokeWidthControl *m_widthControl{nullptr};
    ColorPaletteControl *m_colorPalette{nullptr};

    // Status Bar Widgets
    QLabel *m_coordLabel{nullptr};
    QLabel *m_zoomLabel{nullptr};
    QLabel *m_tipLabel{nullptr};
    QLabel *m_saveStatusLabel{nullptr};
    QProgressBar *m_pressureBar{nullptr};
};

} // namespace OmniNote
