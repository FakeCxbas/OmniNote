#include "MainWindow.hpp"
#include "ThemeManager.hpp"
#include "AppIcons.hpp"
#include "../stem/CodeBlockItem.hpp"
#include "../stem/FunctionPlotItem.hpp"
#include "../stem/TextBlockItem.hpp"
#include "../stem/ImageBlockItem.hpp"
#include "../stem/FormulaBlockItem.hpp"
#include "../canvas/VirtualRulerItem.hpp"
#include "../canvas/UndoCommands.hpp"
#include "../sync/GoogleDriveSync.hpp"
#include <QClipboard>
#include <QMimeData>
#include <QDateTime>
#include <vector>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QDialog>
#include <QLineEdit>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QColorDialog>
#include <QStandardPaths>
#include <QDir>
#include <QShortcut>
#include <QSignalBlocker>
#include <QPixmap>
#include <QPainter>
#include <QIcon>
#include <QFont>
#include <QPushButton>
#include <QApplication>
#include <QKeyEvent>
#include <QSettings>
#include <QTabWidget>
#include <QCheckBox>
#include <QDesktopServices>
#include <QUrl>

namespace OmniNote {

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("OmniNote STEM Engine — High-Performance Native Engineering Notes");
    setWindowIcon(QIcon(":/icons/app_icon.svg"));
    resize(1360, 860);
    setStyleSheet(ThemeManager::getDarkBlueTokyoNightStyle());

    m_driveSyncService = new GoogleDriveSyncService(this);

    // Silent background auto-save timer firing every 30 seconds when QUndoStack has new commands
    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setSingleShot(false);
    m_autoSaveTimer->setInterval(30000);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::onPeriodicAutoSave);
    m_autoSaveTimer->start();

    setupUI();
    setupToolbars();
    setupSidebar();
    setupStatusBar();
    setupShortcuts();

    qApp->installEventFilter(this);

    loadInitialData();

    QTimer::singleShot(100, this, [this]() {
        m_canvasView->fitToWidth();
    });
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_autoSaveTimer) {
        m_autoSaveTimer->stop();
    }

    if (m_currentScene) {
        bool hasCanvasItems = !m_currentScene->items().isEmpty();
        bool hasDirtyState = !m_currentScene->undoStack()->isClean() || m_hasUnsavedChanges;
        if (m_currentPageId > 0 || (hasCanvasItems && hasDirtyState)) {
            onSaveCurrentPage();
        }
    }

    event->accept();
}

void MainWindow::onPeriodicAutoSave() {
    if (!m_currentScene) return;

    // Only trigger background save if new commands exist in QUndoStack or canvas state was modified
    if (!m_currentScene->undoStack()->isClean() || m_hasUnsavedChanges) {
        if (m_currentPageId > 0 || !m_currentScene->items().isEmpty()) {
            onSaveCurrentPage();
        }
    }
}

void MainWindow::markModified() {
    m_hasUnsavedChanges = true;
    if (m_saveStatusLabel) {
        if (m_currentPageId > 0) {
            m_saveStatusLabel->setText("Modificado *");
        } else {
            m_saveStatusLabel->setText("Borrador *");
        }
    }
}

void MainWindow::setupUI() {
    auto *centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget");
    setCentralWidget(centralWidget);

    auto *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_mainSplitter = new QSplitter(Qt::Horizontal, centralWidget);

    // Left Sidebar Container (uses native system QStyle and QPalette)
    m_sidebarContainer = new QWidget(m_mainSplitter);
    m_sidebarContainer->setMinimumWidth(240);
    m_sidebarContainer->setMaximumWidth(380);

    // Right Canvas View Container (keeps dark #14161C page and #0C0D12 viewport background)
    m_currentScene = new VectorCanvasScene(1240, 1754, this);
    m_canvasView = new VectorCanvasView(m_currentScene, m_mainSplitter);

    m_mainSplitter->addWidget(m_sidebarContainer);
    m_mainSplitter->addWidget(m_canvasView);
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setSizes({260, 1100});

    mainLayout->addWidget(m_mainSplitter);

    // Scene & View signals
    connect(m_canvasView, &VectorCanvasView::cursorCoordinatesChanged, this, &MainWindow::updateCoordinateDisplay);
    connect(m_canvasView, &VectorCanvasView::pressureChanged, this, &MainWindow::updatePressureDisplay);
    connect(m_canvasView, &VectorCanvasView::zoomChanged, this, &MainWindow::updateZoomDisplay);
    connect(m_canvasView, &VectorCanvasView::eraserModeChanged, this, &MainWindow::updatePointerTipDisplay);
    connect(m_canvasView, &VectorCanvasView::pasteRequested, this, &MainWindow::onPasteFromClipboard);
    connect(m_currentScene, &VectorCanvasScene::contentModified, this, &MainWindow::markModified);
    connect(m_currentScene->undoStack(), &QUndoStack::indexChanged, this, &MainWindow::markModified);
    connect(m_currentScene, &VectorCanvasScene::textPlacementRequested, this, &MainWindow::onInsertTextBlockAt);
}

void MainWindow::setupSidebar() {
    auto *layout = new QVBoxLayout(m_sidebarContainer);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *headerLabel = new QLabel("OmniNote STEM", m_sidebarContainer);
    headerLabel->setStyleSheet("color: #7aa2f7; font-size: 13pt; font-weight: bold; padding: 2px 0;");
    layout->addWidget(headerLabel);

    // Subjects and Notebooks Tree
    auto *treeLabel = new QLabel("MATERIAS Y CUADERNOS", m_sidebarContainer);
    treeLabel->setStyleSheet("color: #7aa2f7; font-size: 8pt; font-weight: bold; letter-spacing: 1px; padding: 6px 0 2px 0;");
    layout->addWidget(treeLabel);

    m_subjectsTree = new QTreeWidget(m_sidebarContainer);
    m_subjectsTree->setHeaderHidden(true);
    m_subjectsTree->setAnimated(true);
    m_subjectsTree->setIndentation(16);
    layout->addWidget(m_subjectsTree, 3);

    auto *btnLayout1 = new QHBoxLayout();
    btnLayout1->setSpacing(4);
    auto *addSubjectBtn = new QPushButton("Materia", m_sidebarContainer);
    addSubjectBtn->setIcon(AppIcons::get(AppIcons::IconType::Plus, QColor("#7aa2f7"), 14));
    auto *addNotebookBtn = new QPushButton("Cuaderno", m_sidebarContainer);
    addNotebookBtn->setIcon(AppIcons::get(AppIcons::IconType::Plus, QColor("#9ece6a"), 14));
    auto *delTreeBtn = new QPushButton(m_sidebarContainer);
    delTreeBtn->setIcon(AppIcons::get(AppIcons::IconType::Trash, QColor("#f7768e"), 14));
    delTreeBtn->setFixedWidth(30);
    delTreeBtn->setToolTip("Eliminar materia o cuaderno seleccionado");
    btnLayout1->addWidget(addSubjectBtn);
    btnLayout1->addWidget(addNotebookBtn);
    btnLayout1->addWidget(delTreeBtn);
    layout->addLayout(btnLayout1);

    // Pages List
    auto *pagesLabel = new QLabel("PÁGINAS DEL CUADERNO", m_sidebarContainer);
    pagesLabel->setStyleSheet("color: #7aa2f7; font-size: 8pt; font-weight: bold; letter-spacing: 1px; padding: 6px 0 2px 0;");
    layout->addWidget(pagesLabel);

    m_pageList = new QListWidget(m_sidebarContainer);
    layout->addWidget(m_pageList, 2);

    auto *btnLayout2 = new QHBoxLayout();
    btnLayout2->setSpacing(4);
    auto *addPageBtn = new QPushButton("Página", m_sidebarContainer);
    addPageBtn->setIcon(AppIcons::get(AppIcons::IconType::Plus, QColor("#7aa2f7"), 14));
    addPageBtn->setToolTip("Crear nueva página (Ctrl+N)");
    auto *delPageBtn = new QPushButton("Eliminar", m_sidebarContainer);
    delPageBtn->setIcon(AppIcons::get(AppIcons::IconType::Trash, QColor("#f7768e"), 14));
    delPageBtn->setToolTip("Eliminar página seleccionada (se respalda en papelera)");
    m_restorePageBtn = new QPushButton("Recuperar", m_sidebarContainer);
    m_restorePageBtn->setIcon(AppIcons::get(AppIcons::IconType::Restore, QColor("#7dcfff"), 14));
    m_restorePageBtn->setToolTip("Recuperar última página eliminada (Papelera)");
    m_restorePageBtn->setEnabled(false);
    btnLayout2->addWidget(addPageBtn);
    btnLayout2->addWidget(delPageBtn);
    btnLayout2->addWidget(m_restorePageBtn);
    layout->addLayout(btnLayout2);

    connect(addSubjectBtn, &QPushButton::clicked, this, &MainWindow::onAddSubject);
    connect(addNotebookBtn, &QPushButton::clicked, this, &MainWindow::onAddNotebook);
    connect(delTreeBtn, &QPushButton::clicked, this, &MainWindow::onDeleteItem);
    connect(addPageBtn, &QPushButton::clicked, this, &MainWindow::onAddPage);
    connect(delPageBtn, &QPushButton::clicked, this, &MainWindow::onDeleteCurrentPage);
    connect(m_restorePageBtn, &QPushButton::clicked, this, &MainWindow::onRestoreLastDeletedPage);

    connect(m_subjectsTree, &QTreeWidget::itemClicked, this, &MainWindow::onSubjectTreeItemClicked);
    connect(m_pageList, &QListWidget::currentRowChanged, this, &MainWindow::onPageListSelectionChanged);
}

void MainWindow::setupToolbars() {
    // ==================== Toolbar 1: Drawing & STEM Tools ====================
    auto *toolsBar = addToolBar("Herramientas STEM");
    toolsBar->setMovable(false);
    toolsBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolsBar->setIconSize(QSize(22, 22));

    auto *toggleSidebarAction = toolsBar->addAction(AppIcons::get(AppIcons::IconType::Panel, QColor("#7aa2f7"), 22), "Panel");
    toggleSidebarAction->setShortcut(QKeySequence("F4"));
    connect(toggleSidebarAction, &QAction::triggered, this, &MainWindow::toggleSidebar);

    toolsBar->addSeparator();

    m_undoAction = toolsBar->addAction(AppIcons::get(AppIcons::IconType::Undo, QColor("#c0caf5"), 22), "Deshacer");
    m_undoAction->setToolTip("Deshacer último cambio (Ctrl+Z)");
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_undoAction, &QAction::triggered, this, [this]() {
        if (m_currentScene) {
            m_currentScene->undoStack()->undo();
            m_currentScene->update();
        }
    });

    m_redoAction = toolsBar->addAction(AppIcons::get(AppIcons::IconType::Redo, QColor("#c0caf5"), 22), "Rehacer");
    m_redoAction->setToolTip("Rehacer cambio deshecho (Ctrl+Y o Ctrl+Shift+Z)");
    m_redoAction->setShortcuts({QKeySequence::Redo, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z)});
    m_redoAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_redoAction, &QAction::triggered, this, [this]() {
        if (m_currentScene) {
            m_currentScene->undoStack()->redo();
            m_currentScene->update();
        }
    });

    toolsBar->addSeparator();

    m_toolActionGroup = new QActionGroup(this);
    m_toolActionGroup->setExclusive(true);

    m_penAction = toolsBar->addAction(AppIcons::get(AppIcons::IconType::Pen, QColor("#7aa2f7"), 22), "Lápiz (P)");
    m_penAction->setCheckable(true);
    m_penAction->setChecked(true);
    m_penAction->setShortcut(QKeySequence(Qt::Key_P));
    m_penAction->setData(static_cast<int>(ToolType::Pen));
    m_toolActionGroup->addAction(m_penAction);

    m_highlighterAction = toolsBar->addAction(AppIcons::get(AppIcons::IconType::Highlighter, QColor("#e0af68"), 22), "Resaltador (H)");
    m_highlighterAction->setCheckable(true);
    m_highlighterAction->setShortcut(QKeySequence(Qt::Key_H));
    m_highlighterAction->setData(static_cast<int>(ToolType::Highlighter));
    m_toolActionGroup->addAction(m_highlighterAction);

    m_eraserAction = toolsBar->addAction(AppIcons::get(AppIcons::IconType::Eraser, QColor("#f7768e"), 22), "Borrador (E)");
    m_eraserAction->setCheckable(true);
    m_eraserAction->setShortcut(QKeySequence(Qt::Key_E));
    m_eraserAction->setData(static_cast<int>(ToolType::Eraser));
    m_toolActionGroup->addAction(m_eraserAction);

    m_lassoAction = toolsBar->addAction(AppIcons::get(AppIcons::IconType::Lasso, QColor("#bb9af7"), 22), "Lazo (L)");
    m_lassoAction->setCheckable(true);
    m_lassoAction->setShortcut(QKeySequence(Qt::Key_L));
    m_lassoAction->setData(static_cast<int>(ToolType::Lasso));
    m_toolActionGroup->addAction(m_lassoAction);

    m_shapeAction = toolsBar->addAction(AppIcons::get(AppIcons::IconType::Shapes, QColor("#7dcfff"), 22), "Formas (S)");
    m_shapeAction->setCheckable(true);
    m_shapeAction->setShortcut(QKeySequence(Qt::Key_S));
    m_shapeAction->setData(static_cast<int>(ToolType::ShapeLine));
    m_toolActionGroup->addAction(m_shapeAction);

    m_shapeCombo = new QComboBox(toolsBar);
    m_shapeCombo->setMaximumWidth(110);
    m_shapeCombo->addItem(AppIcons::get(AppIcons::IconType::StraightLine, QColor("#7dcfff"), 16), "Línea", static_cast<int>(ToolType::ShapeLine));
    m_shapeCombo->addItem(AppIcons::get(AppIcons::IconType::Axes2D, QColor("#7dcfff"), 16), "Ejes 2D", static_cast<int>(ToolType::ShapeAxes2D));
    m_shapeCombo->addItem(AppIcons::get(AppIcons::IconType::Axes3D, QColor("#7dcfff"), 16), "Ejes 3D", static_cast<int>(ToolType::ShapeAxes3D));
    m_shapeCombo->addItem(AppIcons::get(AppIcons::IconType::RectShape, QColor("#7dcfff"), 16), "Rectángulo", static_cast<int>(ToolType::ShapeRect));
    m_shapeCombo->addItem(AppIcons::get(AppIcons::IconType::CircleShape, QColor("#7dcfff"), 16), "Círculo", static_cast<int>(ToolType::ShapeEllipse));
    connect(m_shapeCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onShapeTypeChanged);
    toolsBar->addWidget(m_shapeCombo);

    connect(m_toolActionGroup, &QActionGroup::triggered, this, &MainWindow::onToolTriggered);

    toolsBar->addSeparator();

    m_textBlockAction = toolsBar->addAction(AppIcons::get(AppIcons::IconType::Text, QColor("#7aa2f7"), 22), "Texto (T)");
    m_textBlockAction->setCheckable(true);
    m_textBlockAction->setShortcut(QKeySequence(Qt::Key_T));
    m_textBlockAction->setData(static_cast<int>(ToolType::Text));
    m_textBlockAction->setToolTip("Herramienta Texto (T): haz clic en el lienzo para escribir en ese punto");
    m_toolActionGroup->addAction(m_textBlockAction);
    connect(m_textBlockAction, &QAction::triggered, this, [this]() {
        if (m_currentScene) {
            m_currentScene->setCurrentTool(ToolType::Text);
        }
    });

    m_codeBlockAction = toolsBar->addAction(AppIcons::get(AppIcons::IconType::Code, QColor("#9ece6a"), 22), "Código (C)");
    m_codeBlockAction->setShortcut(QKeySequence(Qt::Key_C));
    connect(m_codeBlockAction, &QAction::triggered, this, &MainWindow::onInsertCodeBlock);

    m_plotAction = toolsBar->addAction(AppIcons::get(AppIcons::IconType::Plot, QColor("#2ac3de"), 22), "Graficar (F)");
    m_plotAction->setShortcut(QKeySequence(Qt::Key_F));
    connect(m_plotAction, &QAction::triggered, this, &MainWindow::onPlotFunctionDialog);

    m_imageAction = toolsBar->addAction(AppIcons::get(AppIcons::IconType::Image, QColor("#ff9e64"), 22), "Imagen (I)");
    m_imageAction->setShortcut(QKeySequence(Qt::Key_I));
    m_imageAction->setToolTip("Insertar imagen desde archivo (o pegar directo con Ctrl+V)");
    connect(m_imageAction, &QAction::triggered, this, &MainWindow::onInsertImageFromFile);

    m_formulaAction = toolsBar->addAction(AppIcons::get(AppIcons::IconType::Formula, QColor("#bb9af7"), 22), "Fórmula (M)");
    m_formulaAction->setShortcut(QKeySequence(Qt::Key_M));
    m_formulaAction->setToolTip("Insertar fórmula matemática LaTeX");
    connect(m_formulaAction, &QAction::triggered, this, &MainWindow::onInsertFormulaBlock);

    m_rulerAction = toolsBar->addAction(AppIcons::get(AppIcons::IconType::Ruler, QColor("#7dcfff"), 22), "Regla (R)");
    m_rulerAction->setCheckable(true);
    m_rulerAction->setChecked(false);
    m_rulerAction->setShortcut(QKeySequence(Qt::Key_R));
    m_rulerAction->setToolTip("Mostrar/Ocultar Regla Virtual STEM para trazos rectos exactos");
    connect(m_rulerAction, &QAction::triggered, this, &MainWindow::onToggleVirtualRuler);

    toolsBar->addSeparator();

    auto *saveAction = toolsBar->addAction(AppIcons::get(AppIcons::IconType::Save, QColor("#7aa2f7"), 22), "Guardar (Ctrl+S)");
    saveAction->setShortcut(QKeySequence::Save);
    saveAction->setToolTip("Guardar apuntes actuales (Ctrl+S)");
    connect(saveAction, &QAction::triggered, this, &MainWindow::onSaveCurrentPage);

    m_cloudSyncAction = toolsBar->addAction(AppIcons::get(AppIcons::IconType::Cloud, QColor("#7dcfff"), 22), "Google Drive");
    m_cloudSyncAction->setToolTip("Sincronización en la Nube con Google Drive (Copias SQLite y PDF)");
    connect(m_cloudSyncAction, &QAction::triggered, this, &MainWindow::showCloudSyncDialog);

    m_helpAction = toolsBar->addAction(AppIcons::get(AppIcons::IconType::Help, QColor("#bb9af7"), 22), "Ayuda (F1)");
    m_helpAction->setShortcut(QKeySequence("F1"));
    m_helpAction->setToolTip("Guía de atajos de teclado y herramientas STEM (F1)");
    connect(m_helpAction, &QAction::triggered, this, &MainWindow::showShortcutsGuide);

    // ==================== Toolbar 2: Style, Colors, Grid & PDF ====================
    addToolBarBreak(Qt::TopToolBarArea);
    auto *styleBar = addToolBar("Estilo y Documento");
    styleBar->setMovable(false);
    styleBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    styleBar->setIconSize(QSize(22, 22));

    m_widthControl = new StrokeWidthControl(styleBar);
    m_widthControl->setColor(QColor("#ffffff"));
    connect(m_widthControl, &StrokeWidthControl::widthChanged, this, &MainWindow::onStrokeWidthChanged);
    styleBar->addWidget(m_widthControl);

    styleBar->addSeparator();

    m_colorPalette = new ColorPaletteControl(styleBar);
    m_colorPalette->setCurrentColor(QColor("#ffffff"));
    if (m_currentScene) {
        m_currentScene->setCurrentColor(QColor("#ffffff"));
    }
    if (m_widthControl) {
        m_widthControl->setColor(QColor("#ffffff"));
    }
    connect(m_colorPalette, &ColorPaletteControl::colorSelected, this, &MainWindow::onColorSelected);
    connect(m_colorPalette, &ColorPaletteControl::customColorRequested, this, &MainWindow::onCustomColorDialog);
    styleBar->addWidget(m_colorPalette);

    styleBar->addSeparator();

    auto *gridLabel = new QLabel(" Papel: ", styleBar);
    styleBar->addWidget(gridLabel);
    m_gridCombo = new QComboBox(styleBar);
    m_gridCombo->setMaximumWidth(125);
    m_gridCombo->addItem("Cuadrícula Cálculo", static_cast<int>(BackgroundType::CalculusGrid));
    m_gridCombo->addItem("Papel Milimétrico", static_cast<int>(BackgroundType::EngineeringMillimeter));
    m_gridCombo->addItem("Malla Isométrica 3D", static_cast<int>(BackgroundType::Isometric));
    m_gridCombo->addItem("Punteado (Dotted)", static_cast<int>(BackgroundType::Dotted));
    m_gridCombo->addItem("Blanco / Liso", static_cast<int>(BackgroundType::Blank));
    connect(m_gridCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onGridTypeChanged);
    styleBar->addWidget(m_gridCombo);

    styleBar->addSeparator();

    auto *importPdfAction = styleBar->addAction(AppIcons::get(AppIcons::IconType::ImportPdf, QColor("#7aa2f7"), 22), "Importar PDF");
    connect(importPdfAction, &QAction::triggered, this, &MainWindow::onImportPdf);

    auto *exportPdfAction = styleBar->addAction(AppIcons::get(AppIcons::IconType::ExportPdf, QColor("#e0af68"), 22), "Exportar PDF");
    connect(exportPdfAction, &QAction::triggered, this, &MainWindow::onExportPdf);
}

void MainWindow::setupStatusBar() {
    auto *bar = statusBar();

    m_coordLabel = new QLabel("X: 0.0, Y: 0.0", bar);
    m_coordLabel->setMinimumWidth(115);

    m_zoomLabel = new QLabel("Zoom: 100%", bar);
    m_zoomLabel->setMinimumWidth(72);

    auto *zoomOutBtn = new QPushButton("−", bar);
    zoomOutBtn->setFixedWidth(26);
    connect(zoomOutBtn, &QPushButton::clicked, m_canvasView, &VectorCanvasView::zoomOut);

    auto *zoomInBtn = new QPushButton("+", bar);
    zoomInBtn->setFixedWidth(26);
    connect(zoomInBtn, &QPushButton::clicked, m_canvasView, &VectorCanvasView::zoomIn);

    auto *resetZoomBtn = new QPushButton("100%", bar);
    connect(resetZoomBtn, &QPushButton::clicked, m_canvasView, &VectorCanvasView::resetZoom);

    auto *fitWidthBtn = new QPushButton("Ajustar", bar);
    connect(fitWidthBtn, &QPushButton::clicked, m_canvasView, &VectorCanvasView::fitToWidth);

    auto *pressureTitle = new QLabel(" Presión: ", bar);
    m_pressureBar = new QProgressBar(bar);
    m_pressureBar->setRange(0, 100);
    m_pressureBar->setValue(0);
    m_pressureBar->setFixedWidth(75);

    m_tipLabel = new QLabel(" Modo: Lápiz ", bar);
    QFont boldFont = m_tipLabel->font();
    boldFont.setBold(true);
    m_tipLabel->setFont(boldFont);

    m_saveStatusLabel = new QLabel("Guardado", bar);
    m_saveStatusLabel->setFont(boldFont);

    bar->addWidget(m_coordLabel);
    bar->addWidget(m_zoomLabel);
    bar->addWidget(zoomOutBtn);
    bar->addWidget(zoomInBtn);
    bar->addWidget(resetZoomBtn);
    bar->addWidget(fitWidthBtn);
    bar->addWidget(pressureTitle);
    bar->addWidget(m_pressureBar);
    bar->addWidget(m_tipLabel);
    bar->addPermanentWidget(m_saveStatusLabel);
}

void MainWindow::setupShortcuts() {
    auto *delShortcut = new QShortcut(QKeySequence::Delete, this);
    connect(delShortcut, &QShortcut::activated, this, [this]() {
        if (m_currentScene && !m_currentScene->focusItem()) {
            m_currentScene->deleteSelectedItems();
        }
    });

    auto *dupShortcut = new QShortcut(QKeySequence("Ctrl+D"), this);
    connect(dupShortcut, &QShortcut::activated, this, [this]() {
        if (m_currentScene) m_currentScene->duplicateSelectedItems();
    });

    auto *decWidthShortcut = new QShortcut(QKeySequence(Qt::Key_BracketLeft), this);
    connect(decWidthShortcut, &QShortcut::activated, this, [this]() {
        if (m_widthControl) m_widthControl->stepDown();
    });

    auto *incWidthShortcut = new QShortcut(QKeySequence(Qt::Key_BracketRight), this);
    connect(incWidthShortcut, &QShortcut::activated, this, [this]() {
        if (m_widthControl) m_widthControl->stepUp();
    });

    auto *pasteShortcut = new QShortcut(QKeySequence::Paste, this);
    connect(pasteShortcut, &QShortcut::activated, this, &MainWindow::onPasteFromClipboard);
}

void MainWindow::toggleSidebar() {
    m_sidebarContainer->setVisible(!m_sidebarContainer->isVisible());
}

void MainWindow::onToolTriggered(QAction *action) {
    if (!action || !m_currentScene) return;

    int typeVal = action->data().toInt();
    auto tool = static_cast<ToolType>(typeVal);

    if (tool == ToolType::ShapeLine) {
        int shapeVal = m_shapeCombo->currentData().toInt();
        tool = static_cast<ToolType>(shapeVal);
    }

    m_currentScene->setCurrentTool(tool);
}

void MainWindow::onShapeTypeChanged(int /* index */) {
    if (!m_shapeAction->isChecked()) {
        m_shapeAction->setChecked(true);
    }
    onToolTriggered(m_shapeAction);
}

void MainWindow::onGridTypeChanged(int index) {
    if (!m_currentScene) return;
    auto bgType = static_cast<BackgroundType>(m_gridCombo->itemData(index).toInt());
    m_currentScene->setBackgroundType(bgType);

    if (m_currentPageId > 0) {
        m_storage.updatePageBackground(m_currentPageId, static_cast<int>(bgType));
    }
    markModified();
}

void MainWindow::onStrokeWidthChanged(double width) {
    if (m_currentScene) {
        m_currentScene->setCurrentBaseWidth(width);
    }
}

void MainWindow::onColorSelected(const QColor &color) {
    if (m_colorPalette) {
        m_colorPalette->setCurrentColor(color);
    }
    if (m_widthControl) {
        m_widthControl->setColor(color);
    }
    if (m_currentScene) {
        m_currentScene->setCurrentColor(color);
        if (!m_currentScene->selectedItems().isEmpty()) {
            m_currentScene->changeSelectedColor(color);
        }
    }
}

void MainWindow::onCustomColorDialog() {
    QColor initial = m_currentScene ? m_currentScene->currentColor() : Qt::white;
    QColor c = QColorDialog::getColor(initial, this, "Seleccionar Color de Trazo");
    if (c.isValid()) {
        if (m_colorPalette) {
            m_colorPalette->setCustomColor(c);
        }
        onColorSelected(c);
    }
}

void MainWindow::onInsertTextBlock() {
    if (!m_currentScene) return;

    QPoint centerPoint = m_canvasView->viewport()->rect().center();
    QPointF center = m_canvasView->mapToScene(centerPoint) - QPointF(190, 45);
    onInsertTextBlockAt(center);
}

void MainWindow::onInsertTextBlockAt(const QPointF &pos) {
    if (!m_currentScene) return;

    QPointF targetPos = m_currentScene->clampToPage(pos, 20.0);
    auto *textItem = new TextBlockItem(targetPos);
    textItem->setText("Escribe tu texto...");
    connect(textItem, &TextBlockItem::blockModified, this, &MainWindow::markModified);
    connect(textItem, &TextBlockItem::itemDeleted, this, [this](TextBlockItem *) {
        markModified();
    });

    m_currentScene->addItem(textItem);
    m_currentScene->undoStack()->push(new AddStemBlockCommand(m_currentScene, textItem));
    textItem->focusEditorAndSelectAll();
    markModified();

    // Reset tool back to Pen so the user can immediately sketch after typing
    if (m_penAction) {
        m_penAction->setChecked(true);
        m_currentScene->setCurrentTool(ToolType::Pen);
    }
}

void MainWindow::onInsertCodeBlock() {
    if (!m_currentScene) return;

    QPointF center = m_canvasView->mapToScene(m_canvasView->viewport()->rect().center()) - QPointF(210, 120);
    auto *codeItem = new CodeBlockItem(center);
    codeItem->setCode(
        "// Escribe tu código aquí...\n"
        "#include <iostream>\n\n"
        "int main() {\n"
        "    return 0;\n"
        "}\n"
    );
    connect(codeItem, &CodeBlockItem::blockModified, this, &MainWindow::markModified);
    connect(codeItem, &CodeBlockItem::itemDeleted, this, [this](CodeBlockItem *) {
        markModified();
    });

    m_currentScene->addItem(codeItem);
    m_currentScene->undoStack()->push(new AddStemBlockCommand(m_currentScene, codeItem));
    markModified();
}

void MainWindow::onPlotFunctionDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle("Graficador Matemático de Funciones");
    dialog.setMinimumWidth(400);

    auto *formLayout = new QFormLayout(&dialog);

    auto *exprEdit = new QLineEdit("sin(x)/x", &dialog);
    auto *xMinEdit = new QLineEdit("-10", &dialog);
    auto *xMaxEdit = new QLineEdit("10", &dialog);
    auto *yMinEdit = new QLineEdit("-3", &dialog);
    auto *yMaxEdit = new QLineEdit("3", &dialog);

    formLayout->addRow("Función f(x):", exprEdit);
    formLayout->addRow("X Mínimo:", xMinEdit);
    formLayout->addRow("X Máximo:", xMaxEdit);
    formLayout->addRow("Y Mínimo:", yMinEdit);
    formLayout->addRow("Y Máximo:", yMaxEdit);

    auto *presetLayout = new QHBoxLayout();
    const QStringList presets = {"sin(x)/x", "x^2 - 4", "(x^2 - 1)/(x - 1)", "1/x", "exp(-x^2)"};
    for (const auto &p : presets) {
        auto *btn = new QPushButton(p, &dialog);
        connect(btn, &QPushButton::clicked, exprEdit, [exprEdit, p]() {
            exprEdit->setText(p);
        });
        presetLayout->addWidget(btn);
    }
    formLayout->addRow("Ejemplos:", presetLayout);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    formLayout->addRow(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QString expr = exprEdit->text().trimmed();
        double xmin = xMinEdit->text().toDouble();
        double xmax = xMaxEdit->text().toDouble();
        double ymin = yMinEdit->text().toDouble();
        double ymax = yMaxEdit->text().toDouble();

        if (xmin >= xmax) { xmin = -10.0; xmax = 10.0; }
        if (ymin >= ymax) { ymin = -5.0; ymax = 5.0; }

        QPointF center = m_canvasView->mapToScene(m_canvasView->viewport()->rect().center()) - QPointF(220, 150);
        auto *plotItem = new FunctionPlotItem(expr, xmin, xmax, ymin, ymax);
        plotItem->setPos(center);

        m_currentScene->addItem(plotItem);
        m_currentScene->undoStack()->push(new AddStemBlockCommand(m_currentScene, plotItem));
        markModified();
    }
}

void MainWindow::insertImage(const QImage &img, const QPointF &pos) {
    if (!m_currentScene || img.isNull()) return;

    QPointF targetPos = pos;
    if (targetPos.isNull()) {
        QPoint viewCenter(m_canvasView->viewport()->width() / 2, m_canvasView->viewport()->height() / 2);
        targetPos = m_canvasView->mapToScene(viewCenter) - QPointF(160, 120);
    }

    auto *imageItem = new ImageBlockItem(img, targetPos);
    connect(imageItem, &ImageBlockItem::blockModified, this, &MainWindow::markModified);
    connect(imageItem, &ImageBlockItem::itemDeleted, this, [this](ImageBlockItem *) {
        markModified();
    });

    m_currentScene->addItem(imageItem);
    m_currentScene->undoStack()->push(new AddStemBlockCommand(m_currentScene, imageItem));
    markModified();

    if (m_saveStatusLabel) {
        m_saveStatusLabel->setText("Imagen insertada");
    }
}

void MainWindow::onInsertImageFromFile() {
    QString path = QFileDialog::getOpenFileName(this, "Insertar Imagen", QString(),
        "Imágenes (*.png *.jpg *.jpeg *.webp *.bmp *.svg)");
    if (path.isEmpty()) return;

    QImage img(path);
    if (img.isNull()) {
        QMessageBox::warning(this, "Error de Imagen", "No se pudo cargar el archivo de imagen seleccionado.");
        return;
    }
    insertImage(img);
}

void MainWindow::onPasteFromClipboard() {
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard) return;

    const QMimeData *mimeData = clipboard->mimeData();
    if (!mimeData) return;

    if (mimeData->hasImage()) {
        QImage img = qvariant_cast<QImage>(mimeData->imageData());
        if (!img.isNull()) {
            insertImage(img);
            return;
        }
    }

    if (mimeData->hasUrls()) {
        for (const QUrl &url : mimeData->urls()) {
            if (url.isLocalFile()) {
                QString localPath = url.toLocalFile();
                QImage img(localPath);
                if (!img.isNull()) {
                    insertImage(img);
                    return;
                }
            }
        }
    }

    if (mimeData->hasText()) {
        QString text = mimeData->text().trimmed();
        if (text.isEmpty()) return;

        // If math expression / LaTeX
        if (text.startsWith("\\") || text.startsWith("$$") || text.startsWith("$") ||
            text.contains("\\frac") || text.contains("\\sum") || text.contains("\\int") ||
            text.contains("\\sqrt") || text.contains("=")) {
            QPoint viewCenter(m_canvasView->viewport()->width() / 2, m_canvasView->viewport()->height() / 2);
            QPointF targetPos = m_canvasView->mapToScene(viewCenter) - QPointF(180, 70);
            auto *formula = new FormulaBlockItem(text, targetPos);
            connect(formula, &FormulaBlockItem::blockModified, this, &MainWindow::markModified);
            connect(formula, &FormulaBlockItem::itemDeleted, this, [this](FormulaBlockItem *) { markModified(); });
            connect(formula, &FormulaBlockItem::editRequested, m_canvasView, &VectorCanvasView::showLatexEditor);
            m_currentScene->addItem(formula);
            m_currentScene->undoStack()->push(new AddStemBlockCommand(m_currentScene, formula));
            markModified();
            return;
        }

        // Regular text block
        QPoint viewCenter(m_canvasView->viewport()->width() / 2, m_canvasView->viewport()->height() / 2);
        QPointF targetPos = m_canvasView->mapToScene(viewCenter) - QPointF(150, 40);
        auto *textItem = new TextBlockItem(targetPos);
        textItem->setText(text);
        connect(textItem, &TextBlockItem::blockModified, this, &MainWindow::markModified);
        connect(textItem, &TextBlockItem::itemDeleted, this, [this](TextBlockItem *) { markModified(); });
        m_currentScene->addItem(textItem);
        m_currentScene->undoStack()->push(new AddStemBlockCommand(m_currentScene, textItem));
        markModified();
    }
}

void MainWindow::onInsertFormulaBlock() {
    if (m_canvasView) {
        m_canvasView->showLatexEditor(nullptr);
    }
}

void MainWindow::onToggleVirtualRuler() {
    if (!m_currentScene) return;

    if (!m_virtualRuler) {
        QPoint viewCenter(m_canvasView->viewport()->width() / 2, m_canvasView->viewport()->height() / 2);
        QPointF targetPos = m_canvasView->mapToScene(viewCenter) - QPointF(300, 40);
        m_virtualRuler = new VirtualRulerItem(targetPos);
        connect(m_virtualRuler, &VirtualRulerItem::closeRequested, this, [this]() {
            if (m_virtualRuler) {
                m_virtualRuler->hide();
                if (m_rulerAction) m_rulerAction->setChecked(false);
            }
        });
        m_currentScene->addItem(m_virtualRuler);
        m_currentScene->setVirtualRuler(m_virtualRuler);
    }

    bool shouldShow = m_rulerAction ? m_rulerAction->isChecked() : !m_virtualRuler->isVisible();
    m_virtualRuler->setVisible(shouldShow);
    if (m_rulerAction) m_rulerAction->setChecked(shouldShow);

    if (shouldShow) {
        QPoint viewCenter(m_canvasView->viewport()->width() / 2, m_canvasView->viewport()->height() / 2);
        QPointF targetPos = m_canvasView->mapToScene(viewCenter) - QPointF(300, 40);
        m_virtualRuler->setPos(targetPos);
        m_virtualRuler->update();
    }
}

void MainWindow::onImportPdf() {
    QString path = QFileDialog::getOpenFileName(this, "Importar Documento PDF", QString(), "Archivos PDF (*.pdf)");
    if (path.isEmpty()) return;

    if (!m_pdfManager.loadDocument(path)) {
        QMessageBox::critical(this, "Error al abrir PDF", "No se pudo cargar el archivo PDF.");
        return;
    }

    int count = m_pdfManager.pageCount();
    if (count == 0) return;

    QImage pageImg = m_pdfManager.renderPage(0, 150.0);
    if (!pageImg.isNull() && m_currentScene) {
        m_currentScene->setPageSize(pageImg.width(), pageImg.height());
        m_currentScene->setPdfBackground(pageImg, 0);
        m_canvasView->fitToWidth();
        markModified();
    }

    // If a notebook is active and PDF has multiple pages, add the remaining pages
    if (m_currentNotebookId > 0 && count > 1) {
        int existingPages = m_pageList->count();
        for (int i = 1; i < count; ++i) {
            int pNum = existingPages + i;
            qint64 addedId = m_storage.addPage(m_currentNotebookId, pNum, pageImg.width(), pageImg.height(),
                              static_cast<int>(m_currentScene->backgroundType()), i, path);
            auto *pItem = new QListWidgetItem(QString("Página %1 (PDF %2)").arg(pNum).arg(i + 1), m_pageList);
            pItem->setData(Qt::UserRole, addedId);
        }
    }
}

void MainWindow::onExportPdf() {
    if (!m_currentScene) return;

    onSaveCurrentPage();

    QString path = QFileDialog::getSaveFileName(this, "Exportar Cuaderno a PDF", QString(), "Documentos PDF (*.pdf)");
    if (path.isEmpty()) return;

    if (!path.endsWith(".pdf", Qt::CaseInsensitive)) {
        path += ".pdf";
    }

    std::vector<std::unique_ptr<VectorCanvasScene>> scenes;
    QVector<VectorCanvasScene *> rawScenes;

    if (m_currentNotebookId > 0) {
        auto pages = m_storage.getPages(m_currentNotebookId);
        for (const auto &p : pages) {
            auto s = createSceneForPage(p);
            rawScenes.append(s.get());
            scenes.push_back(std::move(s));
        }
    }

    if (rawScenes.isEmpty()) {
        rawScenes.append(m_currentScene);
    }

    bool ok = PdfManager::exportNotebookToPdf(path, rawScenes, 300);

    if (ok) {
        QMessageBox::information(this, "Exportación Exitosa",
                                 QString("El cuaderno (%1 página%2) se exportó en alta resolución vectorial a:\n%3")
                                     .arg(rawScenes.size())
                                     .arg(rawScenes.size() > 1 ? "s" : "")
                                     .arg(path));
    } else {
        QMessageBox::critical(this, "Error de Exportación", "Ocurrió un error al generar el archivo PDF.");
    }
}

void MainWindow::loadInitialData() {
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    QString dbPath = appData + "/omninote_stem.omninote";

    // Transparent migration from legacy paths (e.g. when app name was "OmniNote STEM Engine")
    QString genericData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QStringList legacyCandidates = {
        genericData + "/OmniNote/OmniNote STEM Engine/omninote_stem.omninote",
        genericData + "/OmniNote/omninote_stem.omninote",
        genericData + "/OmniNote/omninote_stem.omninote.SAFETY_COPY"
    };

    if (!QFile::exists(dbPath) || QFileInfo(dbPath).size() <= 32768) {
        for (const QString &cand : legacyCandidates) {
            if (QFile::exists(cand) && QFileInfo(cand).size() > QFileInfo(dbPath).size()) {
                QFile::remove(dbPath);
                QFile::copy(cand, dbPath);
                break;
            }
        }
    }

    if (!m_storage.open(dbPath)) {
        return;
    }

    // Load only user-created subjects (starts completely empty by default)
    auto subjects = m_storage.getSubjects();
    m_subjectsTree->clear();
    m_pageList->clear();

    for (const auto &s : subjects) {
        auto *subItem = new QTreeWidgetItem(m_subjectsTree);
        subItem->setIcon(0, AppIcons::get(AppIcons::IconType::Folder, QColor("#7aa2f7"), 16));
        subItem->setText(0, s.name);
        subItem->setData(0, Qt::UserRole, s.id);
        subItem->setData(0, Qt::UserRole + 1, "subject");

        auto notebooks = m_storage.getNotebooks(s.id);
        for (const auto &nb : notebooks) {
            auto *nbItem = new QTreeWidgetItem(subItem);
            nbItem->setIcon(0, AppIcons::get(AppIcons::IconType::Notebook, QColor("#bb9af7"), 16));
            nbItem->setText(0, nb.title);
            nbItem->setData(0, Qt::UserRole, nb.id);
            nbItem->setData(0, Qt::UserRole + 1, "notebook");
        }
        subItem->setExpanded(true);
    }

    if (m_subjectsTree->topLevelItemCount() > 0) {
        auto *firstSub = m_subjectsTree->topLevelItem(0);
        m_currentSubjectId = firstSub->data(0, Qt::UserRole).toLongLong();
        if (firstSub->childCount() > 0) {
            auto *firstNb = firstSub->child(0);
            m_subjectsTree->setCurrentItem(firstNb);
            onSubjectTreeItemClicked(firstNb, 0);
        } else {
            m_subjectsTree->setCurrentItem(firstSub);
        }
    } else if (m_currentScene) {
        m_currentScene->undoStack()->setClean();
        m_hasUnsavedChanges = false;
    }
}

void MainWindow::onSubjectTreeItemClicked(QTreeWidgetItem *item, int /*column*/) {
    if (!item) return;

    QString type = item->data(0, Qt::UserRole + 1).toString();
    if (type == "subject") {
        m_currentSubjectId = item->data(0, Qt::UserRole).toLongLong();
    } else if (type == "notebook") {
        if (item->parent()) {
            m_currentSubjectId = item->parent()->data(0, Qt::UserRole).toLongLong();
        }
        qint64 newNotebookId = item->data(0, Qt::UserRole).toLongLong();
        if (m_currentPageId > 0 && m_currentNotebookId != newNotebookId) {
            onSaveCurrentPage();
        }
        m_currentNotebookId = newNotebookId;

        auto pages = m_storage.getPages(m_currentNotebookId);
        QSignalBlocker blocker(m_pageList);
        m_pageList->clear();
        for (const auto &p : pages) {
            auto *pItem = new QListWidgetItem(AppIcons::get(AppIcons::IconType::Page, QColor("#7dcfff"), 16), QString("Página %1").arg(p.pageNumber), m_pageList);
            pItem->setData(Qt::UserRole, p.id);
        }

        if (!pages.isEmpty()) {
            m_pageList->setCurrentRow(0);
            blocker.unblock();
            switchPage(pages.first().id);
        } else {
            qint64 newPageId = m_storage.addPage(m_currentNotebookId, 1);
            auto *pItem = new QListWidgetItem(AppIcons::get(AppIcons::IconType::Page, QColor("#7dcfff"), 16), "Página 1", m_pageList);
            pItem->setData(Qt::UserRole, newPageId);
            m_pageList->setCurrentRow(0);
            blocker.unblock();
            switchPage(newPageId);
        }
    }
}

void MainWindow::onPageListSelectionChanged(int row) {
    if (row < 0 || m_currentNotebookId <= 0) return;

    auto *it = m_pageList->item(row);
    if (it) {
        qint64 pageId = it->data(Qt::UserRole).toLongLong();
        if (pageId > 0) {
            switchPage(pageId);
            return;
        }
    }

    auto pages = m_storage.getPages(m_currentNotebookId);
    if (row < pages.size()) {
        switchPage(pages[row].id);
    }
}

void MainWindow::switchPage(qint64 pageId) {
    if (m_currentPageId > 0 && m_currentPageId != pageId) {
        onSaveCurrentPage();
    }

    bool hadUnsavedDraft = (m_currentPageId <= 0 && m_currentNotebookId <= 0 && !m_currentScene->items().isEmpty());
    m_currentPageId = pageId;

    if (hadUnsavedDraft) {
        // Preserve any draft strokes the user drew before creating their first notebook
        onSaveCurrentPage();
        return;
    }

    m_currentScene->clearCanvas();

    // Restore page metadata (background grid & PDF page if applicable)
    if (m_currentNotebookId > 0) {
        auto pages = m_storage.getPages(m_currentNotebookId);
        for (const auto &p : pages) {
            if (p.id == pageId) {
                auto bg = static_cast<BackgroundType>(p.backgroundType);
                m_currentScene->setBackgroundType(bg);
                QSignalBlocker blocker(m_gridCombo);
                int idx = m_gridCombo->findData(p.backgroundType);
                if (idx >= 0) m_gridCombo->setCurrentIndex(idx);

                if (p.pdfPage >= 0 && !p.pdfPath.isEmpty()) {
                    if (m_pdfManager.filePath() != p.pdfPath) {
                        m_pdfManager.loadDocument(p.pdfPath);
                    }
                    if (m_pdfManager.isLoaded()) {
                        QImage img = m_pdfManager.renderPage(p.pdfPage, 150.0);
                        if (!img.isNull()) {
                            m_currentScene->setPageSize(img.width(), img.height());
                            m_currentScene->setPdfBackground(img, p.pdfPage);
                        }
                    }
                }
                break;
            }
        }
    }

    QVector<StrokeRecord> strokes;
    QVector<StemBlockRecord> blocks;
    m_storage.loadPageData(pageId, strokes, blocks);

    for (const auto &sr : strokes) {
        auto *stroke = StrokeItem::deserialize(sr.blobData);
        if (stroke) {
            m_currentScene->addItem(stroke);
        }
    }

    for (const auto &br : blocks) {
        if (br.blockType == 0) {
            auto *codeItem = new CodeBlockItem(QPointF(br.x, br.y));
            codeItem->deserialize(br.data);
            connect(codeItem, &CodeBlockItem::blockModified, this, &MainWindow::markModified);
            connect(codeItem, &CodeBlockItem::itemDeleted, this, [this](CodeBlockItem *) {
                markModified();
            });
            m_currentScene->addItem(codeItem);
        } else if (br.blockType == 1) {
            auto *plotItem = new FunctionPlotItem();
            plotItem->deserialize(br.data);
            m_currentScene->addItem(plotItem);
        } else if (br.blockType == 2) {
            auto *textItem = new TextBlockItem(QPointF(br.x, br.y));
            textItem->deserialize(br.data);
            connect(textItem, &TextBlockItem::blockModified, this, &MainWindow::markModified);
            connect(textItem, &TextBlockItem::itemDeleted, this, [this](TextBlockItem *) {
                markModified();
            });
            m_currentScene->addItem(textItem);
        } else if (br.blockType == 3) {
            auto *imageItem = new ImageBlockItem();
            imageItem->deserialize(br.data);
            connect(imageItem, &ImageBlockItem::blockModified, this, &MainWindow::markModified);
            connect(imageItem, &ImageBlockItem::itemDeleted, this, [this](ImageBlockItem *) {
                markModified();
            });
            m_currentScene->addItem(imageItem);
        } else if (br.blockType == 4) {
            auto *formulaItem = new FormulaBlockItem();
            formulaItem->deserialize(br.data);
            connect(formulaItem, &FormulaBlockItem::blockModified, this, &MainWindow::markModified);
            connect(formulaItem, &FormulaBlockItem::itemDeleted, this, [this](FormulaBlockItem *) {
                markModified();
            });
            connect(formulaItem, &FormulaBlockItem::editRequested, m_canvasView, &VectorCanvasView::showLatexEditor);
            m_currentScene->addItem(formulaItem);
        }
    }

    m_currentScene->undoStack()->setClean();
    m_hasUnsavedChanges = false;
    if (m_saveStatusLabel) {
        m_saveStatusLabel->setText("Guardado");
    }
}

void MainWindow::ensureActivePageForSave() {
    if (m_currentPageId > 0) return;

    // If the user drew strokes on the blank canvas without creating a subject/notebook yet,
    // automatically create or select a container so zero strokes are ever lost on exit.
    auto subjects = m_storage.getSubjects();
    if (subjects.isEmpty()) {
        m_currentSubjectId = m_storage.addSubject("General");
        auto *subItem = new QTreeWidgetItem(m_subjectsTree);
        subItem->setIcon(0, AppIcons::get(AppIcons::IconType::Folder, QColor("#7aa2f7"), 16));
        subItem->setText(0, "General");
        subItem->setData(0, Qt::UserRole, m_currentSubjectId);
        subItem->setData(0, Qt::UserRole + 1, "subject");
        subItem->setExpanded(true);

        m_currentNotebookId = m_storage.addNotebook(m_currentSubjectId, "Cuaderno 1");
        auto *nbItem = new QTreeWidgetItem(subItem);
        nbItem->setIcon(0, AppIcons::get(AppIcons::IconType::Notebook, QColor("#bb9af7"), 16));
        nbItem->setText(0, "Cuaderno 1");
        nbItem->setData(0, Qt::UserRole, m_currentNotebookId);
        nbItem->setData(0, Qt::UserRole + 1, "notebook");
        m_subjectsTree->setCurrentItem(nbItem);
    } else {
        m_currentSubjectId = subjects.first().id;
        auto notebooks = m_storage.getNotebooks(m_currentSubjectId);
        if (notebooks.isEmpty()) {
            m_currentNotebookId = m_storage.addNotebook(m_currentSubjectId, "Cuaderno 1");
            if (m_subjectsTree->topLevelItemCount() > 0) {
                auto *subItem = m_subjectsTree->topLevelItem(0);
                auto *nbItem = new QTreeWidgetItem(subItem);
                nbItem->setIcon(0, AppIcons::get(AppIcons::IconType::Notebook, QColor("#bb9af7"), 16));
                nbItem->setText(0, "Cuaderno 1");
                nbItem->setData(0, Qt::UserRole, m_currentNotebookId);
                nbItem->setData(0, Qt::UserRole + 1, "notebook");
                m_subjectsTree->setCurrentItem(nbItem);
            }
        } else {
            m_currentNotebookId = notebooks.first().id;
        }
    }

    auto pages = m_storage.getPages(m_currentNotebookId);
    if (pages.isEmpty()) {
        int bgType = static_cast<int>(m_currentScene ? m_currentScene->backgroundType() : BackgroundType::CalculusGrid);
        m_currentPageId = m_storage.addPage(m_currentNotebookId, 1, 1240.0, 1754.0, bgType);
        QSignalBlocker blocker(m_pageList);
        m_pageList->clear();
        auto *it = new QListWidgetItem("Página 1", m_pageList);
        it->setData(Qt::UserRole, m_currentPageId);
        m_pageList->setCurrentRow(0);
    } else {
        m_currentPageId = pages.first().id;
    }
}

void MainWindow::onSaveCurrentPage() {
    if (m_isSaving) return;
    if (!m_currentScene) return;

    if (m_currentPageId <= 0) {
        if (m_currentScene->items().isEmpty()) {
            return;
        }
        ensureActivePageForSave();
        if (m_currentPageId <= 0) {
            return;
        }
    }

    m_isSaving = true;
    struct SaveGuard {
        bool &flag;
        ~SaveGuard() { flag = false; }
    } guard{m_isSaving};

    if (m_saveStatusLabel) {
        m_saveStatusLabel->setText("Guardando...");
        m_saveStatusLabel->repaint();
    }

    QVector<StrokeRecord> strokeRecords;
    QVector<StemBlockRecord> blockRecords;

    for (auto *it : m_currentScene->items()) {
        if (it->type() == StrokeItem::Type) {
            auto *stroke = static_cast<StrokeItem *>(it);
            StrokeRecord sr;
            sr.pageId = m_currentPageId;
            sr.toolType = static_cast<int>(stroke->toolType());
            sr.color = stroke->color().name();
            sr.width = stroke->baseWidth();
            sr.zValue = stroke->zValue();
            sr.blobData = stroke->serialize();
            strokeRecords.append(sr);
        } else if (it->type() == CodeBlockItem::Type) {
            auto *code = static_cast<CodeBlockItem *>(it);
            StemBlockRecord br;
            br.pageId = m_currentPageId;
            br.blockType = 0;
            br.x = code->pos().x();
            br.y = code->pos().y();
            br.data = code->serialize();
            blockRecords.append(br);
        } else if (it->type() == FunctionPlotItem::Type) {
            auto *plot = static_cast<FunctionPlotItem *>(it);
            StemBlockRecord br;
            br.pageId = m_currentPageId;
            br.blockType = 1;
            br.x = plot->pos().x();
            br.y = plot->pos().y();
            br.data = plot->serialize();
            blockRecords.append(br);
        } else if (it->type() == TextBlockItem::Type) {
            auto *text = static_cast<TextBlockItem *>(it);
            StemBlockRecord br;
            br.pageId = m_currentPageId;
            br.blockType = 2;
            br.x = text->pos().x();
            br.y = text->pos().y();
            br.data = text->serialize();
            blockRecords.append(br);
        } else if (it->type() == ImageBlockItem::Type) {
            auto *img = static_cast<ImageBlockItem *>(it);
            StemBlockRecord br;
            br.pageId = m_currentPageId;
            br.blockType = 3;
            br.x = img->pos().x();
            br.y = img->pos().y();
            br.data = img->serialize();
            blockRecords.append(br);
        } else if (it->type() == FormulaBlockItem::Type) {
            auto *formula = static_cast<FormulaBlockItem *>(it);
            StemBlockRecord br;
            br.pageId = m_currentPageId;
            br.blockType = 4;
            br.x = formula->pos().x();
            br.y = formula->pos().y();
            br.data = formula->serialize();
            blockRecords.append(br);
        }
    }

    m_storage.updatePageBackground(m_currentPageId, static_cast<int>(m_currentScene->backgroundType()));
    m_storage.savePageData(m_currentPageId, strokeRecords, blockRecords);

    m_currentScene->undoStack()->setClean();
    m_hasUnsavedChanges = false;

    QSettings settings("OmniNote", "OmniNoteSTEM");
    bool autoSync = settings.value("cloud_auto_sync", true).toBool();
    if (autoSync) {
        syncToLocalCloudFolder(false);
    } else {
        if (m_saveStatusLabel) {
            m_saveStatusLabel->setText("Guardado");
        }
    }
}

void MainWindow::syncToLocalCloudFolder(bool notifyUser) {
    QSettings settings("OmniNote", "OmniNoteSTEM");
    QString defaultFolder = QDir::homePath() + "/OmniNote_CloudBackup";
    QString syncFolder = settings.value("cloud_sync_folder", defaultFolder).toString();

    QDir().mkpath(syncFolder);

    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString dbSource = appData + "/omninote_stem.omninote";
    QString dbDest = syncFolder + "/omninote_stem.omninote";

    if (QFile::exists(dbSource)) {
        if (QFile::exists(dbDest)) {
            QFile::remove(dbDest);
        }
        QFile::copy(dbSource, dbDest);
    }

    // Only run full multi-page PDF generation on explicit manual sync
    // Auto-save stays instantaneous (<1ms) to keep handwriting and page-switching 100% fluid
    if (notifyUser) {
        prepareNotebookExportFiles(syncFolder);
    } else if (m_currentScene) {
        QString pdfDest = syncFolder + "/OmniNote_Apuntes.pdf";
        PdfManager::exportNotebookToPdf(pdfDest, {m_currentScene}, 150);
    }

    // Also create a timestamped backup in historial_backups/
    QString backupDir = syncFolder + "/historial_backups";
    QDir().mkpath(backupDir);
    QString timeStamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmm");
    QString timestampDb = backupDir + QString("/omninote_backup_%1.omninote").arg(timeStamp);
    if (!QFile::exists(timestampDb) && QFile::exists(dbSource)) {
        QFile::copy(dbSource, timestampDb);
    }

    settings.setValue("cloud_last_sync", QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss"));

    if (m_saveStatusLabel) {
        m_saveStatusLabel->setText("Guardado y Sincronizado");
    }

    if (notifyUser) {
        QMessageBox::information(this, "Sincronización Exitosa",
            QString("¡Tus apuntes se respaldaron correctamente en tu Google Drive / Nube!\n\n"
                    "Carpeta:\n%1\n\n"
                    "Archivos generados:\n"
                    "• omninote_stem.omninote (Base de datos SQLite completa)\n"
                    "• Hojas individuales numeradas en [Cuaderno]/\n"
                    "• Copia histórica en historial_backups/").arg(syncFolder));
    }
}

std::unique_ptr<VectorCanvasScene> MainWindow::createSceneForPage(const PageRecord &p) {
    auto scene = std::make_unique<VectorCanvasScene>(p.width, p.height);
    scene->setBackgroundType(static_cast<BackgroundType>(p.backgroundType));

    if (p.pdfPage >= 0 && !p.pdfPath.isEmpty()) {
        if (m_pdfManager.filePath() != p.pdfPath) {
            m_pdfManager.loadDocument(p.pdfPath);
        }
        if (m_pdfManager.isLoaded()) {
            QImage img = m_pdfManager.renderPage(p.pdfPage, 150.0);
            if (!img.isNull()) {
                scene->setPageSize(img.width(), img.height());
                scene->setPdfBackground(img, p.pdfPage);
            }
        }
    }

    QVector<StrokeRecord> strokes;
    QVector<StemBlockRecord> blocks;
    m_storage.loadPageData(p.id, strokes, blocks);

    for (const auto &sr : strokes) {
        auto *stroke = StrokeItem::deserialize(sr.blobData);
        if (stroke) {
            scene->addItem(stroke);
        }
    }

    for (const auto &br : blocks) {
        if (br.blockType == 0) {
            auto *codeItem = new CodeBlockItem(QPointF(br.x, br.y));
            codeItem->deserialize(br.data);
            scene->addItem(codeItem);
        } else if (br.blockType == 1) {
            auto *plotItem = new FunctionPlotItem();
            plotItem->deserialize(br.data);
            scene->addItem(plotItem);
        } else if (br.blockType == 2) {
            auto *textItem = new TextBlockItem(QPointF(br.x, br.y));
            textItem->deserialize(br.data);
            scene->addItem(textItem);
        } else if (br.blockType == 3) {
            auto *imageItem = new ImageBlockItem();
            imageItem->deserialize(br.data);
            scene->addItem(imageItem);
        } else if (br.blockType == 4) {
            auto *formulaItem = new FormulaBlockItem();
            formulaItem->deserialize(br.data);
            scene->addItem(formulaItem);
        }
    }

    return scene;
}

QVector<GoogleDriveSyncService::SyncFileEntry> MainWindow::prepareNotebookExportFiles(const QString &targetDir) {
    QVector<GoogleDriveSyncService::SyncFileEntry> filesToSync;

    auto sanitizeName = [](const QString &name) -> QString {
        QString clean = name;
        clean.replace('/', '_').replace('\\', '_').replace(':', '_')
             .replace('*', '_').replace('?', '_').replace('"', '_')
             .replace('<', '_').replace('>', '_').replace('|', '_');
        clean = clean.trimmed();
        return clean.isEmpty() ? "Apuntes" : clean;
    };

    auto subjects = m_storage.getSubjects();
    QSet<qint64> processedNotebookIds;

    for (const auto &sub : subjects) {
        auto notebooks = m_storage.getNotebooks(sub.id);
        for (const auto &nb : notebooks) {
            processedNotebookIds.insert(nb.id);
            auto pages = m_storage.getPages(nb.id);
            if (pages.isEmpty()) continue;

            QString cleanTitle = sanitizeName(nb.title);
            QString subfolder = cleanTitle;
            QString nbDir = targetDir + "/" + subfolder;
            QDir().mkpath(nbDir);

            std::vector<std::unique_ptr<VectorCanvasScene>> scenes;
            QVector<VectorCanvasScene *> rawScenes;

            for (const auto &p : pages) {
                auto scene = createSceneForPage(p);
                QString pageFileName = QString("Hoja %1.pdf").arg(p.pageNumber);
                QString pageLocalPath = nbDir + "/" + pageFileName;

                PdfManager::exportNotebookToPdf(pageLocalPath, {scene.get()}, 300);

                GoogleDriveSyncService::SyncFileEntry entry;
                entry.localFilePath = pageLocalPath;
                entry.remoteFileName = pageFileName;
                entry.subfolderPath = subfolder;
                entry.mimeType = "application/pdf";
                filesToSync.append(entry);

                rawScenes.append(scene.get());
                scenes.push_back(std::move(scene));
            }

            // Export complete merged notebook PDF inside this notebook's folder
            QString fullFileName = QString("%1 - Completo.pdf").arg(cleanTitle);
            QString fullLocalPath = nbDir + "/" + fullFileName;
            PdfManager::exportNotebookToPdf(fullLocalPath, rawScenes, 300);

            GoogleDriveSyncService::SyncFileEntry fullEntry;
            fullEntry.localFilePath = fullLocalPath;
            fullEntry.remoteFileName = fullFileName;
            fullEntry.subfolderPath = subfolder;
            fullEntry.mimeType = "application/pdf";
            filesToSync.append(fullEntry);
        }
    }

    // Fallback: If no notebooks exist in DB but current canvas has items
    if (filesToSync.isEmpty() && m_currentScene) {
        QString subfolder = "Apuntes";
        QString nbDir = targetDir + "/" + subfolder;
        QDir().mkpath(nbDir);

        QString pagePath = nbDir + "/Hoja 1.pdf";
        PdfManager::exportNotebookToPdf(pagePath, {m_currentScene}, 300);
        filesToSync.append({pagePath, "Hoja 1.pdf", subfolder, "application/pdf"});

        QString fullPath = nbDir + "/Apuntes - Completo.pdf";
        PdfManager::exportNotebookToPdf(fullPath, {m_currentScene}, 300);
        filesToSync.append({fullPath, "Apuntes - Completo.pdf", subfolder, "application/pdf"});
    }

    return filesToSync;
}

void MainWindow::onAddSubject() {
    bool ok;
    QString name = QInputDialog::getText(this, "Nueva Materia", "Nombre de la Materia:", QLineEdit::Normal, "", &ok);
    if (ok && !name.trimmed().isEmpty()) {
        qint64 id = m_storage.addSubject(name.trimmed());
        if (id > 0) {
            m_currentSubjectId = id;
            auto *subItem = new QTreeWidgetItem(m_subjectsTree);
            subItem->setIcon(0, AppIcons::get(AppIcons::IconType::Folder, QColor("#7aa2f7"), 16));
            subItem->setText(0, name.trimmed());
            subItem->setData(0, Qt::UserRole, id);
            subItem->setData(0, Qt::UserRole + 1, "subject");
            subItem->setExpanded(true);
            m_subjectsTree->setCurrentItem(subItem);
        } else {
            QMessageBox::warning(this, "Materia Duplicada", "Ya existe una materia con ese nombre.");
        }
    }
}

void MainWindow::onAddNotebook() {
    // If no subject exists at all, help the user create one first
    if (m_subjectsTree->topLevelItemCount() == 0) {
        onAddSubject();
        if (m_subjectsTree->topLevelItemCount() == 0) return;
    }

    auto *current = m_subjectsTree->currentItem();
    if (!current) {
        current = m_subjectsTree->topLevelItem(0);
        m_subjectsTree->setCurrentItem(current);
    }

    qint64 subjectId = -1;
    QTreeWidgetItem *parentSubjectItem = nullptr;
    if (current->data(0, Qt::UserRole + 1).toString() == "subject") {
        subjectId = current->data(0, Qt::UserRole).toLongLong();
        parentSubjectItem = current;
    } else if (current->parent()) {
        subjectId = current->parent()->data(0, Qt::UserRole).toLongLong();
        parentSubjectItem = current->parent();
    }

    if (subjectId <= 0 || !parentSubjectItem) return;

    bool ok;
    QString title = QInputDialog::getText(this, "Nuevo Cuaderno", "Título del Cuaderno:", QLineEdit::Normal, "", &ok);
    if (ok && !title.trimmed().isEmpty()) {
        qint64 id = m_storage.addNotebook(subjectId, title.trimmed());
        if (id > 0) {
            auto *nbItem = new QTreeWidgetItem(parentSubjectItem);
            nbItem->setIcon(0, AppIcons::get(AppIcons::IconType::Notebook, QColor("#bb9af7"), 16));
            nbItem->setText(0, title.trimmed());
            nbItem->setData(0, Qt::UserRole, id);
            nbItem->setData(0, Qt::UserRole + 1, "notebook");
            parentSubjectItem->setExpanded(true);
            m_subjectsTree->setCurrentItem(nbItem);
            onSubjectTreeItemClicked(nbItem, 0);
        }
    }
}

void MainWindow::onAddPage() {
    if (m_currentNotebookId <= 0) {
        QMessageBox::information(this, "Sin Cuaderno Activo", "Primero crea o selecciona un cuaderno para añadir páginas.");
        return;
    }

    int newNum = m_pageList->count() + 1;
    int bgType = static_cast<int>(m_currentScene ? m_currentScene->backgroundType() : BackgroundType::CalculusGrid);
    qint64 newPageId = m_storage.addPage(m_currentNotebookId, newNum, 1240.0, 1754.0, bgType);
    if (newPageId > 0) {
        QSignalBlocker blocker(m_pageList);
        auto *pItem = new QListWidgetItem(AppIcons::get(AppIcons::IconType::Page, QColor("#7dcfff"), 16), QString("Página %1").arg(newNum), m_pageList);
        pItem->setData(Qt::UserRole, newPageId);
        m_pageList->setCurrentRow(m_pageList->count() - 1);
        blocker.unblock();
        switchPage(newPageId);
    }
}

void MainWindow::onDeleteCurrentPage() {
    if (m_currentNotebookId <= 0) return;

    auto pages = m_storage.getPages(m_currentNotebookId);
    if (pages.isEmpty()) return;

    auto *item = m_pageList->currentItem();
    int row = m_pageList->currentRow();
    qint64 targetPageId = item ? item->data(Qt::UserRole).toLongLong() : -1;
    if (targetPageId <= 0 && row >= 0 && row < pages.size()) {
        targetPageId = pages[row].id;
    }
    if (targetPageId <= 0) {
        targetPageId = m_currentPageId;
    }
    if (targetPageId <= 0) return;

    // Find the target page record
    PageRecord targetRecord;
    for (const auto &p : pages) {
        if (p.id == targetPageId) {
            targetRecord = p;
            break;
        }
    }

    if (pages.size() <= 1) {
        // If only 1 page, offer to clear its contents safely
        if (QMessageBox::question(this, "Limpiar Página",
                                  QString("¿Deseas borrar todo el contenido de la Página %1?").arg(targetRecord.pageNumber),
                                  QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
            m_currentScene->clearCanvas();
            m_currentScene->undoStack()->clear();
            onSaveCurrentPage();
        }
        return;
    }

    QString pageTitle = item ? item->text() : QString("Página %1").arg(targetRecord.pageNumber);
    int reply = QMessageBox::question(this, "Eliminar Página",
                                      QString("¿Estás seguro de que deseas eliminar la %1?\n(Tu contenido quedará respaldado en la Papelera y podrás recuperarlo con '↺ Recuperar')")
                                          .arg(pageTitle),
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    // Backup to trash before deleting so user NEVER loses anything
    DeletedPageBackup backup;
    backup.originalNotebookId = m_currentNotebookId;
    backup.originalPageNumber = targetRecord.pageNumber;
    backup.pageRecord = targetRecord;
    if (m_currentPageId == targetPageId && m_hasUnsavedChanges) {
        onSaveCurrentPage();
    }
    m_storage.loadPageData(targetPageId, backup.strokes, backup.blocks);
    m_pageTrash.append(backup);
    if (m_restorePageBtn) {
        m_restorePageBtn->setEnabled(true);
    }

    // If active page is being deleted, clear canvas and cancel pending saves
    bool wasActive = (m_currentPageId == targetPageId);
    if (wasActive) {
        m_currentScene->clearCanvas();
        m_currentScene->undoStack()->clear();
        m_hasUnsavedChanges = false;
        m_currentPageId = -1;
    }

    // Atomic cascade delete and subsequent page renumbering in SQLite
    m_storage.deletePage(targetPageId);

    // Refresh remaining pages in list
    auto remaining = m_storage.getPages(m_currentNotebookId);
    QSignalBlocker blocker(m_pageList);
    m_pageList->clear();
    for (const auto &p : remaining) {
        auto *newItem = new QListWidgetItem(QString("Página %1").arg(p.pageNumber), m_pageList);
        newItem->setData(Qt::UserRole, p.id);
    }
    int nextRow = qBound(0, row, static_cast<int>(remaining.size()) - 1);
    m_pageList->setCurrentRow(nextRow);
    blocker.unblock();

    if (nextRow >= 0 && nextRow < remaining.size()) {
        switchPage(remaining[nextRow].id);
    }

    if (m_saveStatusLabel) {
        m_saveStatusLabel->setText(QString("%1 eliminada (respaldada en Papelera)").arg(pageTitle));
    }
}

void MainWindow::onRestoreLastDeletedPage() {
    if (m_pageTrash.isEmpty()) {
        QMessageBox::information(this, "Papelera vacía", "No hay páginas eliminadas para recuperar.");
        return;
    }

    DeletedPageBackup backup = m_pageTrash.takeLast();
    if (m_restorePageBtn) {
        m_restorePageBtn->setEnabled(!m_pageTrash.isEmpty());
    }

    qint64 nbId = (backup.originalNotebookId > 0) ? backup.originalNotebookId : m_currentNotebookId;
    if (nbId <= 0) return;

    auto existingPages = m_storage.getPages(nbId);
    int newPageNum = existingPages.size() + 1;

    qreal w = (backup.pageRecord.width > 0) ? backup.pageRecord.width : 1240.0;
    qreal h = (backup.pageRecord.height > 0) ? backup.pageRecord.height : 1754.0;
    int bgType = backup.pageRecord.backgroundType;

    qint64 restoredPageId = m_storage.addPage(nbId, newPageNum, w, h, bgType,
                                             backup.pageRecord.pdfPage, backup.pageRecord.pdfPath);

    if (restoredPageId > 0) {
        // Save strokes and blocks
        m_storage.savePageData(restoredPageId, backup.strokes, backup.blocks);

        // Refresh list
        auto pages = m_storage.getPages(nbId);
        QSignalBlocker blocker(m_pageList);
        m_pageList->clear();
        int restoredRow = 0;
        for (int i = 0; i < pages.size(); ++i) {
            auto *it = new QListWidgetItem(AppIcons::get(AppIcons::IconType::Page, QColor("#7dcfff"), 16), QString("Página %1").arg(pages[i].pageNumber), m_pageList);
            it->setData(Qt::UserRole, pages[i].id);
            if (pages[i].id == restoredPageId) {
                restoredRow = i;
            }
        }
        m_pageList->setCurrentRow(restoredRow);
        blocker.unblock();

        switchPage(restoredPageId);

        if (m_saveStatusLabel) {
            m_saveStatusLabel->setText(QString("Página %1 recuperada con éxito").arg(newPageNum));
        }
    }
}

void MainWindow::onDeleteItem() {
    auto *item = m_subjectsTree->currentItem();
    if (!item) return;

    QString type = item->data(0, Qt::UserRole + 1).toString();
    qint64 id = item->data(0, Qt::UserRole).toLongLong();

    if (type == "subject") {
        if (QMessageBox::question(this, "Eliminar Materia", "¿Deseas eliminar esta materia y todos sus cuadernos?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
            m_storage.deleteSubject(id);
            m_currentSubjectId = -1;
            m_currentNotebookId = -1;
            m_currentPageId = -1;
            delete item;
            m_pageList->clear();
            m_currentScene->clearCanvas();
            m_hasUnsavedChanges = false;
        }
    } else if (type == "notebook") {
        if (QMessageBox::question(this, "Eliminar Cuaderno", "¿Deseas eliminar este cuaderno y sus páginas?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
            m_storage.deleteNotebook(id);
            m_currentNotebookId = -1;
            m_currentPageId = -1;
            delete item;
            m_pageList->clear();
            m_currentScene->clearCanvas();
            m_hasUnsavedChanges = false;
        }
    }
}

void MainWindow::updateCoordinateDisplay(const QPointF &pos) {
    if (!m_coordLabel) return;
    static qint64 lastUpdate = 0;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - lastUpdate < 33) return; // ~30 fps throttle for UI labels
    lastUpdate = now;
    m_coordLabel->setText(QString("X: %1, Y: %2").arg(pos.x(), 6, 'f', 1).arg(pos.y(), 6, 'f', 1));
}

void MainWindow::updatePressureDisplay(qreal pressure) {
    if (!m_pressureBar) return;
    static qint64 lastUpdate = 0;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (pressure > 0.0 && now - lastUpdate < 33) return; // ~30 fps throttle during active strokes
    lastUpdate = now;
    int percent = qBound(0, qRound(pressure * 100.0), 100);
    m_pressureBar->setValue(percent);
    m_pressureBar->setFormat(QString("%1%").arg(percent));
}

void MainWindow::updateZoomDisplay(qreal zoom) {
    if (m_zoomLabel) {
        m_zoomLabel->setText(QString("Zoom: %1%").arg(qRound(zoom * 100.0)));
    }
}

void MainWindow::updatePointerTipDisplay(bool isEraser) {
    if (m_tipLabel) {
        if (isEraser) {
            m_tipLabel->setText(" Modo: Borrador ");
        } else {
            m_tipLabel->setText(" Modo: Lápiz ");
        }
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    // 1. Tablet Pad Events (e.g. Wacom CTH-480 ExpressKeys)
    // IMPORTANT: Never treat tablet pen/stylus as an ExpressKey pad!
    if (event->type() == QEvent::TabletPress) {
        auto *te = static_cast<QTabletEvent *>(event);
        const auto *dev = te->pointingDevice();
        bool isPen = (dev && (dev->pointerType() == QPointingDevice::PointerType::Pen ||
                              dev->pointerType() == QPointingDevice::PointerType::Eraser ||
                              dev->type() == QInputDevice::DeviceType::Stylus));
        bool isPad = (dev && (dev->type() == QInputDevice::DeviceType::TouchPad ||
                              dev->name().contains("Pad", Qt::CaseInsensitive)));
        if (!isPen && isPad) {
            if (te->buttons() & Qt::BackButton) {
                if (m_currentScene) { m_currentScene->undoStack()->undo(); m_currentScene->update(); }
                event->accept();
                return true;
            } else if (te->buttons() & Qt::ForwardButton) {
                if (m_currentScene) { m_currentScene->undoStack()->redo(); m_currentScene->update(); }
                event->accept();
                return true;
            } else if (te->buttons() & Qt::ExtraButton1) {
                if (m_penAction) { m_penAction->setChecked(true); onToolTriggered(m_penAction); }
                event->accept();
                return true;
            } else if (te->buttons() & Qt::ExtraButton2) {
                if (m_eraserAction) { m_eraserAction->setChecked(true); onToolTriggered(m_eraserAction); }
                event->accept();
                return true;
            }
        }
    }

    // 2. Extra mouse buttons (Back / Forward / Extra buttons from mouse or tablet pad)
    if (event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::BackButton) {
            if (m_currentScene) { m_currentScene->undoStack()->undo(); m_currentScene->update(); }
            event->accept();
            return true;
        } else if (me->button() == Qt::ForwardButton) {
            if (m_currentScene) { m_currentScene->undoStack()->redo(); m_currentScene->update(); }
            event->accept();
            return true;
        } else if (me->button() == Qt::ExtraButton1) {
            if (m_penAction) { m_penAction->setChecked(true); onToolTriggered(m_penAction); }
            event->accept();
            return true;
        } else if (me->button() == Qt::ExtraButton2) {
            if (m_eraserAction) { m_eraserAction->setChecked(true); onToolTriggered(m_eraserAction); }
            event->accept();
            return true;
        }
    }

    // 3. Text editor focus and shortcut isolation
    if (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress) {
        QWidget *fw = QApplication::focusWidget();
        if (fw && (qobject_cast<QTextEdit *>(fw) ||
                   qobject_cast<QLineEdit *>(fw) ||
                   qobject_cast<QPlainTextEdit *>(fw))) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);

            // Esc key clears focus from text editor so user can easily return to canvas/drawing
            if (keyEvent->key() == Qt::Key_Escape) {
                fw->clearFocus();
                if (m_currentScene) m_currentScene->clearFocus();
                event->accept();
                return true;
            }

            // Allow Ctrl and Alt modifiers (Ctrl+Z, Ctrl+Y, Ctrl+S, Ctrl+C, Ctrl+V, Ctrl+A)
            // Block single-letter hotkeys (P, T, E, L, S, C, M, R, etc.) from stealing text input
            if ((keyEvent->modifiers() == Qt::NoModifier || keyEvent->modifiers() == Qt::ShiftModifier) &&
                !keyEvent->text().isEmpty() &&
                keyEvent->key() != Qt::Key_Tab &&
                keyEvent->key() != Qt::Key_Return &&
                keyEvent->key() != Qt::Key_Enter) {
                event->accept();
                if (event->type() == QEvent::ShortcutOverride) {
                    return true;
                }
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if (m_canvasView && m_canvasView->viewport()) {
        m_canvasView->viewport()->update();
    }
}

void MainWindow::changeEvent(QEvent *event) {
    if (event->type() == QEvent::WindowStateChange || event->type() == QEvent::ActivationChange) {
        if (m_canvasView && m_canvasView->viewport()) {
            m_canvasView->viewport()->update();
        }
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::showCloudSyncDialog() {
    auto *dialog = new QDialog(this);
    dialog->setWindowTitle("Sincronización en la Nube — Google Drive");
    dialog->setMinimumWidth(620);
    dialog->setStyleSheet(
        "QDialog {"
        "   background-color: #16161e;"
        "   border: 1.5px solid #3b4261;"
        "   border-radius: 12px;"
        "}"
        "QLabel { color: #c0caf5; }"
        "QLineEdit {"
        "   background-color: #1f2335;"
        "   color: #c0caf5;"
        "   border: 1px solid #3b4261;"
        "   border-radius: 6px;"
        "   padding: 6px 10px;"
        "   font-size: 11px;"
        "}"
        "QLineEdit:focus {"
        "   border-color: #7aa2f7;"
        "   color: #ffffff;"
        "}"
        "QPushButton#ActionBtn {"
        "   background-color: #7aa2f7;"
        "   color: #15161e;"
        "   font-weight: bold;"
        "   border-radius: 6px;"
        "   padding: 8px 18px;"
        "   font-size: 11.5px;"
        "}"
        "QPushButton#ActionBtn:hover { background-color: #7dcfff; }"
        "QPushButton#SecondaryBtn {"
        "   background-color: #24283b;"
        "   color: #c0caf5;"
        "   border: 1px solid #3b4261;"
        "   border-radius: 6px;"
        "   padding: 7px 14px;"
        "   font-size: 11px;"
        "}"
        "QPushButton#SecondaryBtn:hover { background-color: #2f3549; color: #ffffff; }"
        "QPushButton#DangerBtn {"
        "   background-color: #3b242f;"
        "   color: #f7768e;"
        "   border: 1px solid #782e3b;"
        "   border-radius: 6px;"
        "   padding: 7px 14px;"
        "   font-size: 11px;"
        "}"
        "QPushButton#DangerBtn:hover { background-color: #f7768e; color: #15161e; }"
        "QFrame#Card {"
        "   background-color: #1a1b26;"
        "   border: 1px solid #24283b;"
        "   border-radius: 8px;"
        "   padding: 12px;"
        "}"
        "QProgressBar {"
        "   background-color: #1f2335;"
        "   border: 1px solid #3b4261;"
        "   border-radius: 4px;"
        "   text-align: center;"
        "   color: #ffffff;"
        "   font-weight: bold;"
        "   font-size: 10px;"
        "   height: 16px;"
        "}"
        "QProgressBar::chunk {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3d59a1, stop:1 #7dcfff);"
        "   border-radius: 3px;"
        "}"
    );

    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(12);

    // 1. Header
    auto *headerLayout = new QHBoxLayout();
    auto *iconLabel = new QLabel(dialog);
    iconLabel->setPixmap(AppIcons::getPixmap(AppIcons::IconType::Cloud, QColor("#7dcfff"), 26));
    headerLayout->addWidget(iconLabel);

    auto *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(2);
    auto *titleLabel = new QLabel("Sincronización y Respaldo en la Nube — Google Drive", dialog);
    titleLabel->setStyleSheet("font-size: 14.5px; font-weight: bold; color: #7aa2f7;");
    titleLayout->addWidget(titleLabel);

    auto *subTitleLabel = new QLabel("Respaldo dual: libreta completa SQLite y exportación vectorial continua en PDF", dialog);
    subTitleLabel->setStyleSheet("font-size: 11px; color: #565f89;");
    titleLayout->addWidget(subTitleLabel);

    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();
    layout->addLayout(headerLayout);

    // 2. Primary Google Cloud Sync Card
    auto *googleCard = new QFrame(dialog);
    googleCard->setObjectName("Card");
    auto *gcLayout = new QVBoxLayout(googleCard);
    gcLayout->setContentsMargins(16, 14, 16, 14);
    gcLayout->setSpacing(12);

    auto *badgeLabel = new QLabel(googleCard);
    gcLayout->addWidget(badgeLabel);

    auto *accountInfoLabel = new QLabel(googleCard);
    accountInfoLabel->setStyleSheet("color: #a9b1d6; font-size: 11.5px; line-height: 1.5;");
    accountInfoLabel->setWordWrap(true);
    gcLayout->addWidget(accountInfoLabel);

    auto *actionBtnsLayout = new QHBoxLayout();
    actionBtnsLayout->setSpacing(10);

    auto *connectBtn = new QPushButton("  Iniciar Sesión con Google", googleCard);
    connectBtn->setObjectName("ActionBtn");
    connectBtn->setIcon(AppIcons::get(AppIcons::IconType::Cloud, QColor("#15161e"), 18));
    connectBtn->setCursor(Qt::PointingHandCursor);
    connectBtn->setMinimumHeight(38);
    connectBtn->setStyleSheet("QPushButton { font-weight: bold; font-size: 12px; padding: 8px 18px; }");
    actionBtnsLayout->addWidget(connectBtn);

    auto *syncNowBtn = new QPushButton("  Sincronizar a Google Drive (DB + PDF)", googleCard);
    syncNowBtn->setObjectName("ActionBtn");
    syncNowBtn->setIcon(AppIcons::get(AppIcons::IconType::Cloud, QColor("#15161e"), 16));
    syncNowBtn->setCursor(Qt::PointingHandCursor);
    syncNowBtn->setMinimumHeight(34);
    actionBtnsLayout->addWidget(syncNowBtn);

    auto *disconnectBtn = new QPushButton("Desconectar Cuenta", googleCard);
    disconnectBtn->setObjectName("DangerBtn");
    disconnectBtn->setCursor(Qt::PointingHandCursor);
    disconnectBtn->setMinimumHeight(34);
    actionBtnsLayout->addWidget(disconnectBtn);

    actionBtnsLayout->addStretch();
    gcLayout->addLayout(actionBtnsLayout);

    // Progress bar
    auto *progressBar = new QProgressBar(googleCard);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setVisible(false);
    gcLayout->addWidget(progressBar);

    // Status message label
    auto *statusLabel = new QLabel("Listo para sincronizar con Google.", googleCard);
    statusLabel->setStyleSheet("color: #7dcfff; font-size: 11px;");
    statusLabel->setWordWrap(true);
    gcLayout->addWidget(statusLabel);

    layout->addWidget(googleCard);

    // 3. Optional Local Folder Backup Card
    auto *localCard = new QFrame(dialog);
    localCard->setObjectName("Card");
    auto *lcLayout = new QVBoxLayout(localCard);
    lcLayout->setContentsMargins(14, 12, 14, 12);
    lcLayout->setSpacing(8);

    auto *localHeader = new QLabel("📁 COPIA DE SEGURIDAD EN CARPETA LOCAL (OPCIONAL)", localCard);
    localHeader->setStyleSheet("color: #7aa2f7; font-size: 10.5px; font-weight: bold;");
    lcLayout->addWidget(localHeader);

    QSettings settings("OmniNote", "OmniNoteSTEM");
    QString defaultFolder = QDir::homePath() + "/OmniNote_CloudBackup";
    QString currentFolder = settings.value("cloud_sync_folder", defaultFolder).toString();

    auto *folderRow = new QHBoxLayout();
    folderRow->setSpacing(8);
    auto *folderPathEdit = new QLineEdit(currentFolder, localCard);
    folderPathEdit->setStyleSheet("background-color: #1f2335; border: 1px solid #3b4261; border-radius: 4px; padding: 5px 8px; color: #c0caf5; font-size: 11px;");
    folderRow->addWidget(folderPathEdit, 1);

    auto *browseBtn = new QPushButton("Examinar...", localCard);
    browseBtn->setObjectName("SecondaryBtn");
    browseBtn->setCursor(Qt::PointingHandCursor);
    folderRow->addWidget(browseBtn);

    auto *openDirBtn = new QPushButton("Abrir Carpeta", localCard);
    openDirBtn->setObjectName("SecondaryBtn");
    openDirBtn->setCursor(Qt::PointingHandCursor);
    folderRow->addWidget(openDirBtn);
    lcLayout->addLayout(folderRow);

    bool autoSyncActive = settings.value("cloud_auto_sync", true).toBool();
    auto *autoSyncCheck = new QCheckBox("Guardar automáticamente copias en esta carpeta al presionar Ctrl+S", localCard);
    autoSyncCheck->setChecked(autoSyncActive);
    autoSyncCheck->setStyleSheet("color: #c0caf5; font-size: 11px;");
    lcLayout->addWidget(autoSyncCheck);

    layout->addWidget(localCard);

    // 4. Advanced developer options (hidden by default)
    auto *advContainer = new QWidget(dialog);
    auto *advLayout = new QVBoxLayout(advContainer);
    advLayout->setContentsMargins(0, 0, 0, 0);
    advLayout->setSpacing(6);

    auto *advForm = new QFormLayout();
    advForm->setSpacing(6);
    auto *clientIdInput = new QLineEdit(advContainer);
    clientIdInput->setPlaceholderText("Client ID de Google Cloud");
    clientIdInput->setText(m_driveSyncService->clientId());
    advForm->addRow(new QLabel("Client ID:", advContainer), clientIdInput);

    auto *clientSecretInput = new QLineEdit(advContainer);
    clientSecretInput->setPlaceholderText("Client Secret de Google Cloud");
    clientSecretInput->setEchoMode(QLineEdit::Password);
    clientSecretInput->setText(m_driveSyncService->clientSecret());
    advForm->addRow(new QLabel("Client Secret:", advContainer), clientSecretInput);
    advLayout->addLayout(advForm);

    auto *advBtns = new QHBoxLayout();
    advBtns->setSpacing(8);
    auto *saveCredsBtn = new QPushButton("Guardar Credenciales Personalizadas", advContainer);
    saveCredsBtn->setObjectName("SecondaryBtn");
    saveCredsBtn->setCursor(Qt::PointingHandCursor);
    advBtns->addWidget(saveCredsBtn);

    auto *importJsonBtn = new QPushButton("Importar client_secret.json", advContainer);
    importJsonBtn->setObjectName("SecondaryBtn");
    importJsonBtn->setIcon(AppIcons::get(AppIcons::IconType::ImportPdf, QColor("#c0caf5"), 14));
    importJsonBtn->setCursor(Qt::PointingHandCursor);
    advBtns->addWidget(importJsonBtn);
    advBtns->addStretch();
    advLayout->addLayout(advBtns);

    advContainer->setVisible(false);
    layout->addWidget(advContainer);

    auto *advToggleBtn = new QPushButton("⚙️ Opciones avanzadas de API (Desarrollador)", dialog);
    advToggleBtn->setFlat(true);
    advToggleBtn->setCursor(Qt::PointingHandCursor);
    advToggleBtn->setStyleSheet("color: #565f89; font-size: 10.5px; text-decoration: underline; background: transparent; border: none; text-align: left; padding: 2px 0;");
    connect(advToggleBtn, &QPushButton::clicked, this, [=]() {
        advContainer->setVisible(!advContainer->isVisible());
    });
    layout->addWidget(advToggleBtn);

    // Refresh function for UI state
    auto updateUiState = [=, this]() {
        bool connected = m_driveSyncService->isConnected();
        if (connected) {
            badgeLabel->setText("● ESTADO: CONECTADO CON GOOGLE DRIVE");
            badgeLabel->setStyleSheet("color: #9ece6a; font-size: 11.5px; font-weight: bold;");

            QString lastSyncStr = m_driveSyncService->lastSyncTime().isValid()
                ? m_driveSyncService->lastSyncTime().toString("dd/MM/yyyy hh:mm:ss")
                : "Aún no sincronizado";

            accountInfoLabel->setText(
                QString("<b>Cuenta vinculada:</b> %1<br>"
                        "<b>Destino en la Nube:</b> Carpeta 'OmniNote STEM' en Google Drive<br>"
                        "<b>Última sincronización:</b> %2")
                .arg(m_driveSyncService->userEmail().isEmpty() ? "Usuario Google autenticado" : m_driveSyncService->userEmail(), lastSyncStr)
            );

            connectBtn->setVisible(false);
            syncNowBtn->setVisible(true);
            disconnectBtn->setVisible(true);
        } else {
            badgeLabel->setText("● ESTADO: NO VINCULADO");
            badgeLabel->setStyleSheet("color: #e0af68; font-size: 11.5px; font-weight: bold;");

            accountInfoLabel->setText(
                "Inicia sesión con tu cuenta de Google. OmniNote creará una carpeta 'OmniNote STEM' "
                "en tu Google Drive donde organizará tus cuadernos por nombre y tus hojas en PDF numeradas."
            );

            connectBtn->setVisible(true);
            syncNowBtn->setVisible(false);
            disconnectBtn->setVisible(false);
        }
    };

    updateUiState();

    // Local folder connections
    connect(browseBtn, &QPushButton::clicked, this, [=]() {
        QString dir = QFileDialog::getExistingDirectory(dialog, "Seleccionar Carpeta de Respaldo", folderPathEdit->text());
        if (!dir.isEmpty()) {
            folderPathEdit->setText(dir);
            QSettings s("OmniNote", "OmniNoteSTEM");
            s.setValue("cloud_sync_folder", dir);
            statusLabel->setText("Carpeta local actualizada: " + dir);
        }
    });

    connect(openDirBtn, &QPushButton::clicked, this, [=]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(folderPathEdit->text()));
    });

    connect(autoSyncCheck, &QCheckBox::toggled, this, [=](bool checked) {
        QSettings s("OmniNote", "OmniNoteSTEM");
        s.setValue("cloud_auto_sync", checked);
    });

    // 1-Click Google Login connection
    connect(connectBtn, &QPushButton::clicked, this, [=, this]() {
        statusLabel->setText("Abriendo Google en tu navegador para iniciar sesión...");
        m_driveSyncService->startAuthorization();
    });

    connect(m_driveSyncService, &GoogleDriveSyncService::authorizationUrlReady, dialog, [=](const QString &) {
        statusLabel->setText("Esperando que completes el inicio de sesión en tu navegador...");
    });

    connect(m_driveSyncService, &GoogleDriveSyncService::authorizationSuccess, dialog, [=](const QString &email) {
        statusLabel->setText(QString("<font color='#9ece6a'>¡Conectado con éxito como %1!</font>").arg(email));
        updateUiState();
    });

    connect(m_driveSyncService, &GoogleDriveSyncService::authorizationError, dialog, [=](const QString &err) {
        statusLabel->setText("<font color='#f7768e'>Error de autorización: " + err + "</font>");
    });

    connect(disconnectBtn, &QPushButton::clicked, this, [=, this]() {
        if (QMessageBox::question(dialog, "Desconectar Google Drive", "¿Deseas desconectar tu cuenta de Google Drive?") == QMessageBox::Yes) {
            m_driveSyncService->disconnectAccount();
            statusLabel->setText("Cuenta desconectada.");
            updateUiState();
        }
    });

    // Advanced Credential Handlers
    connect(saveCredsBtn, &QPushButton::clicked, this, [=, this]() {
        m_driveSyncService->setCredentials(clientIdInput->text(), clientSecretInput->text());
        statusLabel->setText("Credenciales personalizadas guardadas.");
    });

    connect(importJsonBtn, &QPushButton::clicked, this, [=, this]() {
        QString file = QFileDialog::getOpenFileName(dialog, "Importar credenciales de Google", QString(), "Archivos JSON (*.json)");
        if (!file.isEmpty()) {
            if (m_driveSyncService->importCredentialsJson(file)) {
                clientIdInput->setText(m_driveSyncService->clientId());
                clientSecretInput->setText(m_driveSyncService->clientSecret());
                statusLabel->setText("Credenciales importadas desde JSON con éxito.");
            } else {
                QMessageBox::warning(dialog, "Error al importar", "No se encontró un client_id válido en el archivo JSON seleccionado.");
            }
        }
    });

    // Sincronizar Ahora a Google Drive
    connect(syncNowBtn, &QPushButton::clicked, this, [=, this]() {
        syncNowBtn->setEnabled(false);
        progressBar->setVisible(true);
        progressBar->setValue(5);
        statusLabel->setText("Guardando apuntes y preparando páginas...");

        onSaveCurrentPage();

        QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QString dbPath = appData + "/omninote_stem.omninote";

        // Export all notebook pages to temporary export directory
        QString tempExportDir = QDir::tempPath() + "/OmniNote_Export";
        QDir().mkpath(tempExportDir);

        auto filesToSync = prepareNotebookExportFiles(tempExportDir);

        statusLabel->setText(QString("Sincronizando %1 archivos de cuadernos con Google Drive...").arg(filesToSync.size()));

        m_driveSyncService->syncNotebookPages(dbPath, filesToSync, [=](bool success, const QString &msg) {
            syncNowBtn->setEnabled(true);
            if (success) {
                progressBar->setValue(100);
                statusLabel->setText("<font color='#9ece6a'>✓ " + msg + "</font>");
                updateUiState();
            } else {
                statusLabel->setText("<font color='#f7768e'>✕ " + msg + "</font>");
            }
        });
    });

    connect(m_driveSyncService, &GoogleDriveSyncService::syncProgress, dialog, [=](int pct, const QString &msg) {
        progressBar->setValue(pct);
        statusLabel->setText(msg);
    });

    // 5. Close Button
    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto *closeBtn = new QPushButton("Cerrar", dialog);
    closeBtn->setObjectName("SecondaryBtn");
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);
    btnLayout->addWidget(closeBtn);
    layout->addLayout(btnLayout);

    dialog->exec();
}

void MainWindow::showShortcutsGuide() {
    auto *dialog = new QDialog(this);
    dialog->setWindowTitle("Guía de Atajos y Herramientas STEM — OmniNote");
    dialog->setMinimumWidth(600);
    dialog->setStyleSheet(
        "QDialog {"
        "   background-color: #16161e;"
        "   border: 1.5px solid #3b4261;"
        "   border-radius: 12px;"
        "}"
        "QLabel { color: #c0caf5; font-size: 11.5px; }"
        "QFrame#Card {"
        "   background-color: #1a1b26;"
        "   border: 1px solid #24283b;"
        "   border-radius: 8px;"
        "   padding: 10px;"
        "}"
        "QPushButton#CloseBtn {"
        "   background-color: #7aa2f7;"
        "   color: #15161e;"
        "   font-weight: bold;"
        "   border-radius: 6px;"
        "   padding: 7px 20px;"
        "}"
        "QPushButton#CloseBtn:hover { background-color: #7dcfff; }"
    );

    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    // Title
    auto *headerLayout = new QHBoxLayout();
    auto *iconLabel = new QLabel(dialog);
    iconLabel->setPixmap(AppIcons::getPixmap(AppIcons::IconType::Help, QColor("#bb9af7"), 22));
    headerLayout->addWidget(iconLabel);

    auto *titleLabel = new QLabel("Atajos de Teclado y Herramientas STEM", dialog);
    titleLabel->setStyleSheet("font-size: 15px; font-weight: bold; color: #7aa2f7; margin-left: 6px;");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    layout->addLayout(headerLayout);

    // Grid of shortcuts in cards
    auto *card1 = new QFrame(dialog);
    card1->setObjectName("Card");
    auto *c1Layout = new QVBoxLayout(card1);
    c1Layout->setSpacing(5);
    auto *t1 = new QLabel("HERRAMIENTAS DE DIBUJO Y APUNTES:", card1);
    t1->setStyleSheet("color: #7dcfff; font-weight: bold; font-size: 10px;");
    c1Layout->addWidget(t1);

    auto addRow = [](QVBoxLayout *l, const QString &key, const QString &desc) {
        auto *lbl = new QLabel(QString("• <b style='color:#bb9af7'>%1</b>: %2").arg(key, desc));
        lbl->setTextFormat(Qt::RichText);
        l->addWidget(lbl);
    };

    addRow(c1Layout, "P", "Lápiz inteligente con respuesta de presión dinámica y anti-temblor");
    addRow(c1Layout, "H", "Resaltador translúcido de trazo suave");
    addRow(c1Layout, "E", "Borrador atómico por trazo (o clic derecho con lápiz)");
    addRow(c1Layout, "L", "Lazo de selección libre, traslación, escala y cambio de color");
    addRow(c1Layout, "S", "Formas geométricas (Líneas rectas, Ejes 2D/3D, Rectángulos, Círculos)");
    addRow(c1Layout, "R", "Regla Virtual STEM con guía recta láser y medidor angular");
    layout->addWidget(card1);

    auto *card2 = new QFrame(dialog);
    card2->setObjectName("Card");
    auto *c2Layout = new QVBoxLayout(card2);
    c2Layout->setSpacing(5);
    auto *t2 = new QLabel("BLOQUES STEM Y FÓRMULAS:", card2);
    t2->setStyleSheet("color: #9ece6a; font-weight: bold; font-size: 10px;");
    c2Layout->addWidget(t2);

    addRow(c2Layout, "T", "Modo Texto (haz clic en el lienzo para escribir en ese punto)");
    addRow(c2Layout, "M", "Editor integrado de Fórmulas LaTeX (Computer Modern de alta definición)");
    addRow(c2Layout, "C", "Bloque de Código con resaltado de sintaxis C++20 y Python");
    addRow(c2Layout, "F", "Graficador de funciones matemáticas cartesianas f(x)");
    addRow(c2Layout, "I", "Insertar imagen desde archivo (o pegar con Ctrl+V)");
    layout->addWidget(card2);

    auto *card3 = new QFrame(dialog);
    card3->setObjectName("Card");
    auto *c3Layout = new QVBoxLayout(card3);
    c3Layout->setSpacing(5);
    auto *t3 = new QLabel("NAVEGACIÓN Y EDICIÓN:", card3);
    t3->setStyleSheet("color: #e0af68; font-weight: bold; font-size: 10px;");
    c3Layout->addWidget(t3);

    addRow(c3Layout, "Espacio + Arrastrar", "Mover / Desplazar lienzo con la mano (Pan)");
    addRow(c3Layout, "Ctrl + Rueda", "Zoom fluido hacia el cursor");
    addRow(c3Layout, "Ctrl + Z / Ctrl + Y", "Deshacer / Rehacer historial ilimitado");
    addRow(c3Layout, "Ctrl + S", "Guardar página inmediatamente en base de datos SQLite");
    addRow(c3Layout, "F4", "Mostrar u ocultar la barra lateral de materias y páginas");
    addRow(c3Layout, "Esc", "Salir de la edición de texto y volver a dibujar");
    layout->addWidget(card3);

    auto *card4 = new QFrame(dialog);
    card4->setObjectName("Card");
    auto *c4Layout = new QVBoxLayout(card4);
    c4Layout->setSpacing(5);
    auto *t4 = new QLabel("TABLETA WACOM (4 EXPRESSKEYS CTH-480):", card4);
    t4->setStyleSheet("color: #7aa2f7; font-weight: bold; font-size: 10px;");
    c4Layout->addWidget(t4);

    addRow(c4Layout, "Botón Superior Izquierdo", "Deshacer último cambio (Ctrl+Z)");
    addRow(c4Layout, "Botón Inferior Izquierdo", "Rehacer cambio deshecho (Ctrl+Y)");
    addRow(c4Layout, "Botón Superior Derecho", "Seleccionar herramienta Lápiz (P)");
    addRow(c4Layout, "Botón Inferior Derecho", "Seleccionar herramienta Borrador (E)");
    addRow(c4Layout, "Botones de Lápiz Stylus", "Botón inferior: Borrador momentáneo | Botón superior: Lazo momentáneo");
    layout->addWidget(card4);

    // Bottom
    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto *closeBtn = new QPushButton("Entendido", dialog);
    closeBtn->setObjectName("CloseBtn");
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);
    btnLayout->addWidget(closeBtn);
    layout->addLayout(btnLayout);

    dialog->exec();
}

} // namespace OmniNote
