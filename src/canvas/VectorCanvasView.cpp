#include "VectorCanvasView.hpp"
#include "CanvasCursorFactory.hpp"
#include "SelectionActionBar.hpp"
#include "../stem/CodeBlockItem.hpp"
#include "../stem/FunctionPlotItem.hpp"
#include "../stem/TextBlockItem.hpp"
#include "../stem/ImageBlockItem.hpp"
#include "../stem/FormulaBlockItem.hpp"
#include "../stem/LatexEditorOverlay.hpp"
#include "UndoCommands.hpp"
#include "VirtualRulerItem.hpp"
#include <QScrollBar>
#include <QDateTime>
#include <QtMath>
#include <QSurfaceFormat>

namespace OmniNote {

VectorCanvasView::VectorCanvasView(VectorCanvasScene *scene, QWidget *parent)
    : QGraphicsView(scene, parent), m_canvasScene(scene) {
    // 1. Double-buffered native viewport (Wayland resilient across resize and workspace switching)
    viewport()->setAttribute(Qt::WA_OpaquePaintEvent);
    viewport()->setAttribute(Qt::WA_NoSystemBackground);

    // 2. High-Quality Vector Render Hints
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setOptimizationFlags(QGraphicsView::DontAdjustForAntialiasing);

    // 3. Coordinate Anchoring & Scrollbars
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setResizeAnchor(QGraphicsView::NoAnchor);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    setStyleSheet(
        "QGraphicsView { border: none; background-color: #0c0d12; }"
        "QScrollBar:vertical, QScrollBar:horizontal {"
        "   background-color: #14161C;"
        "   border: none;"
        "   width: 10px;"
        "   height: 10px;"
        "}"
        "QScrollBar::handle:vertical, QScrollBar::handle:horizontal {"
        "   background-color: #24283b;"
        "   border-radius: 5px;"
        "}"
        "QScrollBar::handle:vertical:hover, QScrollBar::handle:horizontal:hover {"
        "   background-color: #7aa2f7;"
        "}"
        "QScrollBar::add-line, QScrollBar::sub-line {"
        "   width: 0px;"
        "   height: 0px;"
        "}"
    );

    setMouseTracking(true);
    viewport()->setMouseTracking(true);

    // Floating Selection Action Bar
    m_selectionActionBar = new SelectionActionBar(viewport());
    connect(m_selectionActionBar, &SelectionActionBar::duplicateRequested, this, [this]() {
        if (m_canvasScene) m_canvasScene->duplicateSelectedItems();
        updateSelectionActionBarPosition();
    });
    connect(m_selectionActionBar, &SelectionActionBar::deleteRequested, this, [this]() {
        if (m_canvasScene) m_canvasScene->deleteSelectedItems();
        if (m_selectionActionBar) m_selectionActionBar->hide();
    });
    connect(m_selectionActionBar, &SelectionActionBar::recolorRequested, this, [this]() {
        if (m_canvasScene) m_canvasScene->changeSelectedColor(m_canvasScene->currentColor());
    });
    connect(m_selectionActionBar, &SelectionActionBar::clearRequested, this, [this]() {
        if (m_canvasScene) {
            m_canvasScene->clearSelection();
            m_canvasScene->invalidate(m_canvasScene->sceneRect(), QGraphicsScene::ForegroundLayer);
        }
        if (m_selectionActionBar) m_selectionActionBar->hide();
    });

    if (m_canvasScene) {
        setSceneRect(m_canvasScene->pageRect().adjusted(-80, -80, 80, 80));
        connect(m_canvasScene, &VectorCanvasScene::cursorMoved, this, &VectorCanvasView::cursorCoordinatesChanged);
        connect(m_canvasScene, &VectorCanvasScene::toolChanged, this, &VectorCanvasView::updateToolCursor);
        connect(m_canvasScene, &VectorCanvasScene::colorChanged, this, &VectorCanvasView::updateToolCursor);
        connect(m_canvasScene, &VectorCanvasScene::widthChanged, this, &VectorCanvasView::updateToolCursor);
        connect(m_canvasScene, &VectorCanvasScene::selectionStateChanged, this, [this](bool hasSelection) {
            if (hasSelection) {
                updateSelectionActionBarPosition();
                m_selectionActionBar->show();
                m_selectionActionBar->raise();
            } else {
                m_selectionActionBar->hide();
            }
        });
    }

    // In-App LaTeX Formula Editor Overlay (Centered inside canvas view, zero window tiling)
    m_latexOverlay = new LatexEditorOverlay(viewport());
    m_latexOverlay->hide();

    connect(m_latexOverlay, &LatexEditorOverlay::formulaSaved, this, [this](FormulaBlockItem *item, const QString &latex, const QColor &color) {
        if (item) {
            item->setLatex(latex, color);
            if (m_canvasScene) emit m_canvasScene->contentModified();
        }
    });

    connect(m_latexOverlay, &LatexEditorOverlay::newFormulaRequested, this, [this](const QString &latex, const QColor &color) {
        if (!m_canvasScene) return;
        QPoint viewCenter(viewport()->width() / 2, viewport()->height() / 2);
        QPointF targetPos = mapToScene(viewCenter) - QPointF(120, 35);
        auto *formula = new FormulaBlockItem(latex, targetPos, color);
        connect(formula, &FormulaBlockItem::blockModified, this, [this]() {
            if (m_canvasScene) emit m_canvasScene->contentModified();
        });
        connect(formula, &FormulaBlockItem::itemDeleted, this, [this](FormulaBlockItem *) {
            if (m_canvasScene) emit m_canvasScene->contentModified();
        });
        connect(formula, &FormulaBlockItem::editRequested, this, &VectorCanvasView::showLatexEditor);

        m_canvasScene->addItem(formula);
        m_canvasScene->undoStack()->push(new AddStemBlockCommand(m_canvasScene, formula));
        emit m_canvasScene->contentModified();
    });

    updateToolCursor();
}

void VectorCanvasView::enterEvent(QEnterEvent *event) {
    updateToolCursor();
    QGraphicsView::enterEvent(event);
}

void VectorCanvasView::updateToolCursor() {
    if (m_spacePressed) {
        viewport()->setCursor(Qt::OpenHandCursor);
        setCursor(Qt::OpenHandCursor);
        return;
    }
    if (m_isPanning) {
        viewport()->setCursor(Qt::ClosedHandCursor);
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    ToolType tool = m_canvasScene ? m_canvasScene->currentTool() : ToolType::Pen;
    QColor color = m_canvasScene ? m_canvasScene->currentColor() : QColor("#ffffff");
    qreal width = m_canvasScene ? m_canvasScene->currentBaseWidth() : 2.5;

    m_currentToolCursor = CanvasCursorFactory::createCursor(tool, color, width);
    viewport()->setCursor(m_currentToolCursor);
    setCursor(m_currentToolCursor);
}

void VectorCanvasView::drawBackground(QPainter *painter, const QRectF &rect) {
    QGraphicsView::drawBackground(painter, rect);
}

bool VectorCanvasView::shouldForwardToSceneWidgets(const QPointF &scenePos) const {
    if (!m_canvasScene) return false;

    // In Lasso mode, the canvas handles lasso selection, moving selected items, and handle scaling.
    // Do not forward mouse clicks on canvas items to child widgets during Lasso mode.
    if (m_canvasScene->currentTool() == ToolType::Lasso) {
        return false;
    }

    const auto itemsAtPos = m_canvasScene->items(scenePos);
    for (QGraphicsItem *item : itemsAtPos) {
        // Check if item or its ancestor is CodeBlockItem or TextBlockItem
        QGraphicsItem *curr = item;
        while (curr) {
            if (curr->type() == CodeBlockItem::Type || curr->type() == TextBlockItem::Type) {
                return true;
            }
            if (curr->type() == FunctionPlotItem::Type) {
                QPointF localPos = curr->mapFromScene(scenePos);
                QRectF br = curr->boundingRect();
                // Header bar (top 32px) or bottom-right resize grip (20x20)
                if (localPos.y() <= 32.0 || (localPos.x() >= br.width() - 20.0 && localPos.y() >= br.height() - 20.0)) {
                    return true;
                }
            }
            if (curr->type() == ImageBlockItem::Type) {
                auto *imgItem = static_cast<const ImageBlockItem *>(curr);
                QPointF localPos = imgItem->mapFromScene(scenePos);
                if (imgItem->isControlHit(localPos)) {
                    return true;
                }
            }
            if (curr->type() == FormulaBlockItem::Type) {
                auto *formulaItem = static_cast<const FormulaBlockItem *>(curr);
                QPointF localPos = formulaItem->mapFromScene(scenePos);
                if (formulaItem->isControlHit(localPos)) {
                    return true;
                }
            }
            if (curr->type() == VirtualRulerItem::Type) {
                auto *rulerItem = static_cast<const VirtualRulerItem *>(curr);
                QPointF localPos = rulerItem->mapFromScene(scenePos);
                if (rulerItem->isControlHit(localPos)) {
                    return true;
                }
            }
            curr = curr->parentItem();
        }
    }
    return false;
}

void VectorCanvasView::applyZoom(qreal factor, const QPointF &centerPoint) {
    qreal newZoom = qBound(0.15, m_zoomFactor * factor, 8.0);
    qreal actualScale = newZoom / m_zoomFactor;
    if (std::abs(actualScale - 1.0) < 1e-4) return;

    QPointF scenePosBefore = mapToScene(centerPoint.toPoint());

    scale(actualScale, actualScale);
    m_zoomFactor = newZoom;

    QPointF scenePosAfter = mapToScene(centerPoint.toPoint());
    QPointF diff = scenePosAfter - scenePosBefore;

    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - qRound(diff.x() * m_zoomFactor));
    verticalScrollBar()->setValue(verticalScrollBar()->value() - qRound(diff.y() * m_zoomFactor));

    updateSelectionActionBarPosition();
    emit zoomChanged(m_zoomFactor);
}

void VectorCanvasView::zoomIn() {
    applyZoom(1.2, QPointF(viewport()->width() * 0.5, viewport()->height() * 0.5));
}

void VectorCanvasView::zoomOut() {
    applyZoom(1.0 / 1.2, QPointF(viewport()->width() * 0.5, viewport()->height() * 0.5));
}

void VectorCanvasView::resetZoom() {
    setZoomFactor(1.0);
}

void VectorCanvasView::setZoomFactor(qreal factor) {
    qreal target = qBound(0.15, factor, 8.0);
    qreal ratio = target / m_zoomFactor;
    scale(ratio, ratio);
    m_zoomFactor = target;
    emit zoomChanged(m_zoomFactor);
}

void VectorCanvasView::fitToWidth() {
    if (!m_canvasScene) return;
    setSceneRect(m_canvasScene->pageRect().adjusted(-80, -80, 80, 80));
    qreal pageW = m_canvasScene->pageSize().width() + 60.0;
    qreal viewW = viewport()->width();
    if (pageW <= 0.0 || viewW <= 0.0) return;

    qreal targetZoom = viewW / pageW;
    setZoomFactor(targetZoom);
    centerOn(m_canvasScene->pageSize().width() * 0.5,
             (viewport()->height() * 0.5) / m_zoomFactor - 25.0);
}

void VectorCanvasView::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() & Qt::ControlModifier) {
        const qreal degrees = event->angleDelta().y() / 8.0;
        const qreal steps = degrees / 15.0;
        const qreal factor = std::pow(1.12, steps);
        applyZoom(factor, event->position());
        event->accept();
    } else {
        QGraphicsView::wheelEvent(event);
    }
}

bool VectorCanvasView::viewportEvent(QEvent *event) {
    switch (event->type()) {
    case QEvent::TabletPress:
    case QEvent::TabletMove:
    case QEvent::TabletRelease: {
        auto *te = static_cast<QTabletEvent *>(event);
        tabletEvent(te);
        if (te->isAccepted()) {
            return true;
        }
        break;
    }
    default:
        break;
    }
    return QGraphicsView::viewportEvent(event);
}

void VectorCanvasView::mousePressEvent(QMouseEvent *event) {
    const auto *dev = event->pointingDevice();
    bool isStylus = (dev && (dev->pointerType() == QPointingDevice::PointerType::Pen ||
                             dev->pointerType() == QPointingDevice::PointerType::Eraser ||
                             dev->type() == QInputDevice::DeviceType::Stylus));

    // Panning: Middle mouse button (if regular mouse) or Space + Left button
    if ((!isStylus && event->button() == Qt::MiddleButton) ||
        (event->button() == Qt::LeftButton && m_spacePressed)) {
        m_isPanning = true;
        m_panStartPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    QPointF scenePos = mapToScene(event->pos());
    if (shouldForwardToSceneWidgets(scenePos)) {
        m_interactingWithWidget = true;
        QGraphicsView::mousePressEvent(event);
        return;
    }

    // Clear focus from any embedded code editor when clicking on the canvas
    if (m_canvasScene && m_canvasScene->focusItem()) {
        m_canvasScene->clearFocus();
    }

    // Guard against duplicate synthetic mouse press events during tablet usage
    if (m_isTabletStroke || event->source() == Qt::MouseEventSynthesizedBySystem) {
        event->accept();
        return;
    }
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    m_lastMousePos = scenePos;
    m_lastMouseTimeMs = now;

    qreal initialPressure = 0.65;
    qreal devPressure = event->points().isEmpty() ? 0.0 : event->points().first().pressure();
    if (isStylus && devPressure > 0.01) {
        initialPressure = qBound(0.05, devPressure, 1.0);
    }
    m_simulatedPressure = initialPressure;

    // Stylus Barrel Button 1 (or Right Click): Momentary Eraser
    if (event->button() == Qt::RightButton && m_canvasScene) {
        m_stylusEraserActive = true;
        m_preStylusTool = m_canvasScene->currentTool();
        viewport()->setCursor(CanvasCursorFactory::createEraserCursor());
        m_canvasScene->handlePointerPress(scenePos, initialPressure, 0.0, 0.0, true);
        emit pressureChanged(initialPressure);
        emit cursorCoordinatesChanged(scenePos);
        event->accept();
        return;
    }

    // Stylus Barrel Button 2 (or Middle Click on stylus): Momentary Lasso
    if (event->button() == Qt::MiddleButton && isStylus && m_canvasScene) {
        m_stylusLassoActive = true;
        m_preStylusTool = m_canvasScene->currentTool();
        m_canvasScene->setCurrentTool(ToolType::Lasso);
        m_canvasScene->handlePointerPress(scenePos, initialPressure, 0.0, 0.0, false);
        emit pressureChanged(initialPressure);
        emit cursorCoordinatesChanged(scenePos);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_canvasScene) {
        m_canvasScene->handlePointerPress(scenePos, initialPressure, 0.0, 0.0, false);
        emit pressureChanged(initialPressure);
        emit cursorCoordinatesChanged(scenePos);
        event->accept();
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void VectorCanvasView::mouseMoveEvent(QMouseEvent *event) {
    if (m_isPanning) {
        QPoint delta = event->pos() - m_panStartPos;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        m_panStartPos = event->pos();
        event->accept();
        return;
    }

    if (m_interactingWithWidget) {
        QGraphicsView::mouseMoveEvent(event);
        return;
    }

    // If a tablet stroke is active, or if this is a synthetic drag event, tabletEvent handles drawing
    if (m_isTabletStroke || ((event->buttons() & Qt::LeftButton) && event->source() == Qt::MouseEventSynthesizedBySystem)) {
        event->accept();
        return;
    }
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    QPointF scenePos = mapToScene(event->pos());
    emit cursorCoordinatesChanged(scenePos);

    if (m_interactingWithWidget) {
        QGraphicsView::mouseMoveEvent(event);
        return;
    }

    if (shouldForwardToSceneWidgets(scenePos)) {
        QGraphicsView::mouseMoveEvent(event);
        return;
    }

    // Dynamic cursor over lasso selection handles
    if (m_canvasScene && m_canvasScene->currentTool() == ToolType::Lasso && !m_canvasScene->selectedItems().isEmpty()) {
        auto h = m_canvasScene->hitTestSelectionHandle(scenePos);
        if (h == VectorCanvasScene::SelectionHandle::TopLeft || h == VectorCanvasScene::SelectionHandle::BottomRight) {
            viewport()->setCursor(Qt::SizeFDiagCursor);
            setCursor(Qt::SizeFDiagCursor);
        } else if (h == VectorCanvasScene::SelectionHandle::TopRight || h == VectorCanvasScene::SelectionHandle::BottomLeft) {
            viewport()->setCursor(Qt::SizeBDiagCursor);
            setCursor(Qt::SizeBDiagCursor);
        } else if (h == VectorCanvasScene::SelectionHandle::Inside) {
            viewport()->setCursor(Qt::SizeAllCursor);
            setCursor(Qt::SizeAllCursor);
        } else {
            viewport()->setCursor(m_currentToolCursor);
            setCursor(m_currentToolCursor);
        }
    }

    if ((event->buttons() & (Qt::LeftButton | Qt::RightButton | Qt::MiddleButton)) && m_canvasScene) {
        qint64 dt = qMax(qint64(1), now - m_lastMouseTimeMs);
        qreal dist = std::hypot(scenePos.x() - m_lastMousePos.x(), scenePos.y() - m_lastMousePos.y());
        qreal speed = dist / static_cast<qreal>(dt); // px per ms
        m_lastMousePos = scenePos;
        m_lastMouseTimeMs = now;

        qreal currentPressure;
        const auto *dev = event->pointingDevice();
        bool isStylus = (dev && (dev->pointerType() == QPointingDevice::PointerType::Pen ||
                                 dev->pointerType() == QPointingDevice::PointerType::Eraser ||
                                 dev->type() == QInputDevice::DeviceType::Stylus));
        qreal devPressure = event->points().isEmpty() ? 0.0 : event->points().first().pressure();
        if (isStylus && devPressure > 0.01) {
            currentPressure = qBound(0.05, devPressure, 1.0);
        } else {
            qreal targetP = qBound(0.20, 0.90 - 0.40 * std::log1p(speed * 2.0), 1.0);
            m_simulatedPressure += 0.35 * (targetP - m_simulatedPressure);
            currentPressure = m_simulatedPressure;
        }

        bool isEraser = m_stylusEraserActive || (event->buttons() & Qt::RightButton);
        m_canvasScene->handlePointerMove(scenePos, currentPressure, 0.0, 0.0, isEraser);
        emit pressureChanged(currentPressure);
        if (!m_canvasScene->selectedItems().isEmpty()) {
            updateSelectionActionBarPosition();
        }
        event->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
}

void VectorCanvasView::mouseReleaseEvent(QMouseEvent *event) {
    if (m_isPanning && (event->button() == Qt::MiddleButton || (event->button() == Qt::LeftButton && m_spacePressed))) {
        m_isPanning = false;
        if (m_spacePressed) {
            viewport()->setCursor(Qt::OpenHandCursor);
            setCursor(Qt::OpenHandCursor);
        } else {
            updateToolCursor();
        }
        event->accept();
        return;
    }

    if (m_interactingWithWidget) {
        m_interactingWithWidget = false;
        updateToolCursor();
        QGraphicsView::mouseReleaseEvent(event);
        return;
    }

    // Guard against duplicate synthetic mouse release events during tablet usage
    if (m_isTabletStroke || event->source() == Qt::MouseEventSynthesizedBySystem) {
        event->accept();
        return;
    }

    if (m_stylusEraserActive && (event->button() == Qt::RightButton || event->buttons() == Qt::NoButton)) {
        m_stylusEraserActive = false;
        if (m_canvasScene) {
            QPointF scenePos = mapToScene(event->pos());
            m_canvasScene->handlePointerRelease(scenePos, 0.0, 0.0, 0.0, true);
        }
        updateToolCursor();
        emit pressureChanged(0.0);
        event->accept();
        return;
    }

    if (m_stylusLassoActive && (event->button() == Qt::MiddleButton || event->buttons() == Qt::NoButton)) {
        m_stylusLassoActive = false;
        if (m_canvasScene) {
            QPointF scenePos = mapToScene(event->pos());
            m_canvasScene->handlePointerRelease(scenePos, 0.0, 0.0, 0.0, false);
            m_canvasScene->setCurrentTool(m_preStylusTool);
        }
        updateToolCursor();
        emit pressureChanged(0.0);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_canvasScene) {
        QPointF scenePos = mapToScene(event->pos());
        m_canvasScene->handlePointerRelease(scenePos, 0.0, 0.0, 0.0, false);
        emit pressureChanged(0.0);
        updateSelectionActionBarPosition();
        event->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void VectorCanvasView::tabletEvent(QTabletEvent *event) {
    if (!m_canvasScene) {
        event->ignore();
        return;
    }

    // Viewport-relative coordinates from QTabletEvent
    QPointF scenePos = mapToScene(event->position().toPoint());

    qreal pressure = qBound(0.05, event->pressure(), 1.0);
    qreal tx = event->xTilt();
    qreal ty = event->yTilt();

    bool isEraser = false;
    const auto *dev = event->pointingDevice();
    if (dev && dev->pointerType() == QPointingDevice::PointerType::Eraser) {
        isEraser = true;
    }

    // Stylus Barrel Buttons:
    // Button 1 (lower button / right click) -> momentary eraser
    if (event->buttons() & Qt::RightButton) {
        isEraser = true;
    }

    // Button 2 (upper button / middle click) -> momentary lasso
    if ((event->buttons() & Qt::MiddleButton) && !m_stylusLassoActive) {
        m_stylusLassoActive = true;
        m_preStylusTool = m_canvasScene->currentTool();
        m_canvasScene->setCurrentTool(ToolType::Lasso);
    } else if (!(event->buttons() & Qt::MiddleButton) && m_stylusLassoActive) {
        m_stylusLassoActive = false;
        m_canvasScene->setCurrentTool(m_preStylusTool);
    }

    static bool s_lastEraserState = false;
    if (isEraser != s_lastEraserState) {
        s_lastEraserState = isEraser;
        if (isEraser) {
            viewport()->setCursor(CanvasCursorFactory::createEraserCursor());
        } else if (!m_spacePressed && !m_isPanning) {
            viewport()->setCursor(m_currentToolCursor);
        }
    }

    emit eraserModeChanged(isEraser);
    emit cursorCoordinatesChanged(scenePos);

    switch (event->type()) {
    case QEvent::TabletPress:
        if (shouldForwardToSceneWidgets(scenePos)) {
            event->ignore(); // Let Qt synthesize widget click for UI controls
            return;
        }
        m_isTabletStroke = true;
        m_lastTabletTimeMs = QDateTime::currentMSecsSinceEpoch();
        emit pressureChanged(pressure);
        m_canvasScene->handlePointerPress(scenePos, pressure, tx, ty, isEraser);
        event->accept();
        break;

    case QEvent::TabletMove:
        if (m_isTabletStroke) {
            m_lastTabletTimeMs = QDateTime::currentMSecsSinceEpoch();
            emit pressureChanged(pressure);
            m_canvasScene->handlePointerMove(scenePos, pressure, tx, ty, isEraser);
            event->accept();
        } else {
            // Hovering without pressure: let Qt synthesize smooth mouse moves so cursor doesn't freeze or jump
            event->ignore();
        }
        break;

    case QEvent::TabletRelease:
        if (m_isTabletStroke) {
            m_isTabletStroke = false;
            m_lastTabletTimeMs = QDateTime::currentMSecsSinceEpoch();
            m_canvasScene->handlePointerRelease(scenePos, 0.0, tx, ty, isEraser);
        }
        if (m_stylusLassoActive && !(event->buttons() & Qt::MiddleButton)) {
            m_stylusLassoActive = false;
            m_canvasScene->setCurrentTool(m_preStylusTool);
        }
        emit pressureChanged(0.0);
        event->accept();
        break;

    default:
        event->ignore();
        break;
    }
}

void VectorCanvasView::keyPressEvent(QKeyEvent *event) {
    if (event->matches(QKeySequence::Paste) || (event->modifiers() & Qt::ControlModifier && event->key() == Qt::Key_V)) {
        if (!m_canvasScene || !m_canvasScene->focusItem()) {
            emit pasteRequested();
            event->accept();
            return;
        }
    }
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat() && (!m_canvasScene || !m_canvasScene->focusItem())) {
        m_spacePressed = true;
        viewport()->setCursor(Qt::OpenHandCursor);
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void VectorCanvasView::keyReleaseEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spacePressed = false;
        updateToolCursor();
        event->accept();
        return;
    }
    QGraphicsView::keyReleaseEvent(event);
}

void VectorCanvasView::scrollContentsBy(int dx, int dy) {
    QGraphicsView::scrollContentsBy(dx, dy);
    updateSelectionActionBarPosition();
}

void VectorCanvasView::resizeEvent(QResizeEvent *event) {
    QGraphicsView::resizeEvent(event);
    m_isTabletStroke = false;
    m_isPanning = false;
    updateSelectionActionBarPosition();
    if (m_latexOverlay && m_latexOverlay->isVisible()) {
        m_latexOverlay->setGeometry(viewport()->rect());
    }
    viewport()->update();
}

void VectorCanvasView::changeEvent(QEvent *event) {
    if (event->type() == QEvent::WindowStateChange || event->type() == QEvent::ActivationChange) {
        m_isTabletStroke = false;
        m_isPanning = false;
        m_spacePressed = false;
        m_interactingWithWidget = false;
        m_stylusEraserActive = false;
        m_stylusLassoActive = false;
        if (m_canvasScene) {
            m_canvasScene->handlePointerRelease(m_lastMousePos, 0.0, 0.0, 0.0, false);
        }
        updateToolCursor();
        viewport()->update();
    }
    QGraphicsView::changeEvent(event);
}

void VectorCanvasView::focusOutEvent(QFocusEvent *event) {
    m_isTabletStroke = false;
    m_isPanning = false;
    m_spacePressed = false;
    m_interactingWithWidget = false;
    QGraphicsView::focusOutEvent(event);
}

void VectorCanvasView::hideEvent(QHideEvent *event) {
    m_isTabletStroke = false;
    m_isPanning = false;
    m_spacePressed = false;
    m_interactingWithWidget = false;
    QGraphicsView::hideEvent(event);
}

void VectorCanvasView::showLatexEditor(FormulaBlockItem *item) {
    if (!m_latexOverlay) return;
    m_latexOverlay->openForFormula(item);
}

void VectorCanvasView::updateSelectionActionBarPosition() {
    if (!m_selectionActionBar || !m_canvasScene || m_canvasScene->selectedItems().isEmpty()) {
        if (m_selectionActionBar) m_selectionActionBar->hide();
        return;
    }

    QRectF sceneSel = m_canvasScene->selectionBoundingRect();
    if (sceneSel.isEmpty()) {
        m_selectionActionBar->hide();
        return;
    }

    QRect viewRect = mapFromScene(sceneSel).boundingRect();
    int x = viewRect.center().x() - m_selectionActionBar->width() / 2;
    int y = viewRect.top() - m_selectionActionBar->height() - 10;

    x = qBound(10, x, viewport()->width() - m_selectionActionBar->width() - 10);
    if (y < 10) {
        y = viewRect.bottom() + 10;
    }
    y = qBound(10, y, viewport()->height() - m_selectionActionBar->height() - 10);

    m_selectionActionBar->move(x, y);
}

} // namespace OmniNote
