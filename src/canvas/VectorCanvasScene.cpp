#include "VectorCanvasScene.hpp"
#include "UndoCommands.hpp"
#include "VirtualRulerItem.hpp"
#include <QPainter>
#include <QtMath>
#include <QGraphicsView>
#include <QDateTime>

namespace OmniNote {

VectorCanvasScene::VectorCanvasScene(qreal width, qreal height, QObject *parent)
    : QGraphicsScene(parent),
      m_pageSize(width, height),
      m_bgType(BackgroundType::CalculusGrid),
      m_currentTool(ToolType::Pen),
      m_currentColor(QColor("#ffffff")),
      m_currentBaseWidth(2.5) {
    setSceneRect(0, 0, m_pageSize.width(), m_pageSize.height());
    connect(&m_undoStack, &QUndoStack::cleanChanged, this, [this](bool clean) {
        if (!clean) {
            emit contentModified();
        }
    });
}

void VectorCanvasScene::setPageSize(qreal width, qreal height) {
    m_pageSize = QSizeF(width, height);
    setSceneRect(0, 0, width, height);
    m_gridCacheDirty = true;
    update();
}

QPointF VectorCanvasScene::clampToPage(const QPointF &scenePos, qreal margin) const {
    const qreal minX = qMin(margin, m_pageSize.width() * 0.5);
    const qreal maxX = qMax(minX, m_pageSize.width() - margin);
    const qreal minY = qMin(margin, m_pageSize.height() * 0.5);
    const qreal maxY = qMax(minY, m_pageSize.height() - margin);
    return QPointF(qBound(minX, scenePos.x(), maxX),
                   qBound(minY, scenePos.y(), maxY));
}

void VectorCanvasScene::setBackgroundType(BackgroundType type) {
    m_bgType = type;
    m_gridCacheDirty = true;
    invalidate(sceneRect(), BackgroundLayer);
    update();
}

void VectorCanvasScene::setPdfBackground(const QImage &img, int pageNumber) {
    m_pdfImage = img;
    m_pdfPageNumber = pageNumber;
    m_gridCacheDirty = true;
    invalidate(sceneRect(), BackgroundLayer);
    update();
}

void VectorCanvasScene::clearPdfBackground() {
    m_pdfImage = QImage();
    m_pdfPageNumber = -1;
    m_gridCacheDirty = true;
    invalidate(sceneRect(), BackgroundLayer);
    update();
}

void VectorCanvasScene::setCurrentTool(ToolType tool) {
    if (m_currentTool != tool) {
        m_currentTool = tool;
        emit toolChanged(tool);
    }
}

void VectorCanvasScene::setCurrentColor(const QColor &c) {
    if (m_currentColor != c) {
        m_currentColor = c;
        emit colorChanged(c);
    }
}

void VectorCanvasScene::setCurrentBaseWidth(qreal w) {
    qreal clamped = qBound(0.5, w, 50.0);
    if (std::abs(m_currentBaseWidth - clamped) > 1e-4) {
        m_currentBaseWidth = clamped;
        emit widthChanged(clamped);
    }
}

void VectorCanvasScene::handlePointerPress(const QPointF &scenePos, qreal pressure, qreal tx, qreal ty, bool isEraser) {
    emit cursorMoved(scenePos);

    // Forbid starting any stroke, eraser, or shape outside the physical page boundaries
    if (!pageRect().contains(scenePos)) {
        return;
    }

    const QPointF clampedPos = clampToPage(scenePos, qMax(2.0, m_currentBaseWidth * 0.5));
    ToolType activeTool = isEraser ? ToolType::Eraser : m_currentTool;

    if (activeTool == ToolType::Eraser) {
        m_isErasing = true;
        m_lastEraserPos = clampedPos;
        m_erasedStrokesInGesture.clear();
        performEraserAt(clampedPos, m_eraserRadius);
        return;
    }

    if (activeTool == ToolType::Text) {
        emit textPlacementRequested(clampedPos);
        return;
    }

    if (activeTool == ToolType::Lasso) {
        if (!selectedItems().isEmpty()) {
            SelectionHandle handle = hitTestSelectionHandle(clampedPos);
            QRectF box = selectionBoundingRect().adjusted(-8, -8, 8, 8);

            if (handle == SelectionHandle::TopLeft || handle == SelectionHandle::TopRight ||
                handle == SelectionHandle::BottomRight || handle == SelectionHandle::BottomLeft) {
                m_isScalingSelection = true;
                m_scaleHandle = handle;
                m_scaleTotalFactor = 1.0;

                if (handle == SelectionHandle::TopLeft) m_scalePivot = box.bottomRight();
                else if (handle == SelectionHandle::TopRight) m_scalePivot = box.bottomLeft();
                else if (handle == SelectionHandle::BottomRight) m_scalePivot = box.topLeft();
                else if (handle == SelectionHandle::BottomLeft) m_scalePivot = box.topRight();

                m_scaleStartDist = std::hypot(clampedPos.x() - m_scalePivot.x(), clampedPos.y() - m_scalePivot.y());
                m_scaleLastDist = m_scaleStartDist;
                return;
            }

            if (handle == SelectionHandle::Inside) {
                m_isMovingSelection = true;
                m_moveStartPos = clampedPos;
                m_moveTotalDelta = QPointF(0, 0);
                return;
            }
        }

        clearSelection();
        emit selectionStateChanged(false);
        m_isLassoing = true;
        m_lassopoly.clear();
        m_lassopoly << clampedPos;
        invalidate(sceneRect(), ForegroundLayer);
        return;
    }

    // Drawing a stroke (Pen, Highlighter, or Shapes) strictly inside the page
    QPointF effectivePos = clampedPos;
    if (m_activeRuler && m_activeRuler->isVisible() &&
        (activeTool == ToolType::Pen || activeTool == ToolType::Highlighter || activeTool == ToolType::ShapeLine)) {
        QPointF snapped;
        if (m_activeRuler->snapPoint(clampedPos, snapped, 22.0)) {
            effectivePos = snapped;
        }
    }

    m_isDrawing = true;
    m_smoother.reset();
    m_activePoints.clear();
    m_lastInputPos = effectivePos;

    StrokePoint rawPt(effectivePos, pressure, tx, ty, QDateTime::currentMSecsSinceEpoch());
    StrokePoint filtered = m_smoother.filterPoint(rawPt);
    m_activePoints.append(filtered);

    m_activeStrokeItem = new StrokeItem(activeTool, m_currentColor, m_currentBaseWidth);
    m_activeStrokeItem->setAutoImprove(m_autoImproveEnabled);
    m_activeStrokeItem->setPoints(m_activePoints);
    addItem(m_activeStrokeItem);
    update(m_activeStrokeItem->boundingRect().adjusted(-12, -12, 12, 12));
}

void VectorCanvasScene::handlePointerMove(const QPointF &scenePos, qreal pressure, qreal tx, qreal ty, bool /*isEraser*/) {
    emit cursorMoved(scenePos);

    const QPointF clampedPos = clampToPage(scenePos, qMax(2.0, m_currentBaseWidth * 0.5));

    if (m_isErasing) {
        qreal dist = std::hypot(clampedPos.x() - m_lastEraserPos.x(), clampedPos.y() - m_lastEraserPos.y());
        int steps = qMax(1, static_cast<int>(dist / (m_eraserRadius * 0.5)));
        for (int i = 1; i <= steps; ++i) {
            qreal t = static_cast<qreal>(i) / steps;
            QPointF p = m_lastEraserPos + t * (clampedPos - m_lastEraserPos);
            performEraserAt(p, m_eraserRadius);
        }
        m_lastEraserPos = clampedPos;
        return;
    }

    if (m_isMovingSelection) {
        QPointF delta = clampedPos - m_moveStartPos;
        m_moveTotalDelta += delta;
        for (auto *it : selectedItems()) {
            if (it->type() == StrokeItem::Type) {
                static_cast<StrokeItem *>(it)->translateStroke(delta);
            } else {
                it->setPos(it->pos() + delta);
            }
        }
        m_moveStartPos = clampedPos;
        invalidate(sceneRect(), ForegroundLayer);
        update();
        return;
    }

    if (m_isScalingSelection) {
        qreal curDist = std::hypot(clampedPos.x() - m_scalePivot.x(), clampedPos.y() - m_scalePivot.y());
        if (m_scaleLastDist > 5.0 && curDist > 5.0) {
            qreal stepFactor = curDist / m_scaleLastDist;
            stepFactor = qBound(0.7, stepFactor, 1.4);
            m_scaleTotalFactor *= stepFactor;
            for (auto *it : selectedItems()) {
                if (it->type() == StrokeItem::Type) {
                    static_cast<StrokeItem *>(it)->scaleStroke(stepFactor, m_scalePivot);
                } else {
                    it->setPos(m_scalePivot + (it->pos() - m_scalePivot) * stepFactor);
                }
            }
            m_scaleLastDist = curDist;
            invalidate(sceneRect(), ForegroundLayer);
            update();
        }
        return;
    }

    if (m_isLassoing) {
        m_lassopoly << clampedPos;
        invalidate(sceneRect(), ForegroundLayer);
        return;
    }

    if (m_isDrawing && m_activeStrokeItem) {
        QPointF effectivePos = clampedPos;
        if (m_activeRuler && m_activeRuler->isVisible() &&
            (m_activeStrokeItem->toolType() == ToolType::Pen || m_activeStrokeItem->toolType() == ToolType::Highlighter || m_activeStrokeItem->toolType() == ToolType::ShapeLine)) {
            QPointF snapped;
            if (m_activeRuler->snapPoint(clampedPos, snapped, 22.0)) {
                effectivePos = snapped;
            }
        }

        // Distance filter to drop micro-tremors and redundant events (< 0.5px)
        qreal dx = effectivePos.x() - m_lastInputPos.x();
        qreal dy = effectivePos.y() - m_lastInputPos.y();
        if (dx * dx + dy * dy < 0.25) {
            return;
        }
        m_lastInputPos = effectivePos;

        StrokePoint rawPt(effectivePos, pressure, tx, ty, QDateTime::currentMSecsSinceEpoch());

        if (m_activeStrokeItem->toolType() == ToolType::Pen || m_activeStrokeItem->toolType() == ToolType::Highlighter) {
            StrokePoint filtered = m_smoother.filterPoint(rawPt);
            m_activePoints.append(filtered);
            m_activeStrokeItem->addPointFast(filtered);
        } else {
            // Geometric shapes: start point + current cursor position clamped to page
            QVector<StrokePoint> shapePts;
            shapePts.append(m_activePoints.first());
            shapePts.append(rawPt);
            m_activeStrokeItem->setPoints(shapePts);
        }
    }
}

void VectorCanvasScene::handlePointerRelease(const QPointF &scenePos, qreal pressure, qreal tx, qreal ty, bool /*isEraser*/) {
    const QPointF clampedPos = clampToPage(scenePos, qMax(2.0, m_currentBaseWidth * 0.5));
    QPointF effectivePos = clampedPos;
    if (m_activeRuler && m_activeRuler->isVisible() && m_activeStrokeItem) {
        QPointF snapped;
        if (m_activeRuler->snapPoint(clampedPos, snapped, 22.0)) {
            effectivePos = snapped;
        }
    }

    if (m_isErasing) {
        m_isErasing = false;
        if (!m_erasedStrokesInGesture.isEmpty()) {
            m_undoStack.push(new DeleteItemsCommand(this, m_erasedStrokesInGesture));
            m_erasedStrokesInGesture.clear();
            emit contentModified();
        }
        return;
    }

    if (m_isMovingSelection) {
        m_isMovingSelection = false;
        if (!m_moveTotalDelta.isNull()) {
            m_undoStack.push(new MoveItemsCommand(selectedItems(), m_moveTotalDelta));
            emit contentModified();
        }
        invalidate(sceneRect(), ForegroundLayer);
        return;
    }

    if (m_isScalingSelection) {
        m_isScalingSelection = false;
        if (std::abs(m_scaleTotalFactor - 1.0) > 0.01) {
            m_undoStack.push(new ScaleStrokesCommand(selectedItems(), m_scaleTotalFactor, m_scalePivot));
            emit contentModified();
        }
        invalidate(sceneRect(), ForegroundLayer);
        return;
    }

    if (m_isLassoing) {
        m_isLassoing = false;
        m_lassopoly << clampedPos;
        applyLassoSelection();
        invalidate(sceneRect(), ForegroundLayer);
        return;
    }

    if (m_isDrawing && m_activeStrokeItem) {
        StrokePoint rawPt(clampedPos, pressure, tx, ty, QDateTime::currentMSecsSinceEpoch());

        if (m_activeStrokeItem->toolType() == ToolType::Pen || m_activeStrokeItem->toolType() == ToolType::Highlighter) {
            StrokePoint filtered = m_smoother.filterPoint(rawPt);
            if (m_activePoints.isEmpty() || std::hypot(clampedPos.x() - m_activePoints.last().pos.x(), clampedPos.y() - m_activePoints.last().pos.y()) > 0.2) {
                m_activePoints.append(filtered);
                m_activeStrokeItem->addPointFast(filtered);
            }
            m_activeStrokeItem->finalizeStroke();
        } else {
            QVector<StrokePoint> shapePts;
            shapePts.append(m_activePoints.first());
            shapePts.append(rawPt);
            m_activeStrokeItem->setPoints(shapePts);
        }

        m_isDrawing = false;
        // Pushing to undo stack; since item was added during press, AddStrokeCommand manages its redo/undo
        m_undoStack.push(new AddStrokeCommand(this, m_activeStrokeItem));
        m_activeStrokeItem = nullptr;
        m_activePoints.clear();
        emit contentModified();
    }
}

void VectorCanvasScene::performEraserAt(const QPointF &pos, qreal radius) {
    QPainterPath eraserArea;
    eraserArea.addEllipse(pos, radius, radius);

    QList<QGraphicsItem *> colliding = items(eraserArea);
    for (auto *item : colliding) {
        if (item->type() == StrokeItem::Type) {
            if (!m_erasedStrokesInGesture.contains(item)) {
                m_erasedStrokesInGesture.append(item);
                removeItem(item);
            }
        }
    }
}

void VectorCanvasScene::applyLassoSelection() {
    clearSelection();
    if (m_lassopoly.size() < 3) {
        if (!m_lassopoly.isEmpty()) {
            QPointF clickPos = m_lassopoly.first();
            QList<QGraphicsItem *> clicked = items(clickPos);
            for (auto *it : clicked) {
                QGraphicsItem *curr = it;
                while (curr && !(curr->flags() & QGraphicsItem::ItemIsSelectable)) {
                    curr = curr->parentItem();
                }
                if (curr && (curr->flags() & QGraphicsItem::ItemIsSelectable)) {
                    curr->setSelected(true);
                    emit selectionStateChanged(true);
                    return;
                }
            }
        }
        emit selectionStateChanged(false);
        return;
    }

    QPainterPath lassoPath;
    lassoPath.addPolygon(m_lassopoly);
    lassoPath.closeSubpath();

    QList<QGraphicsItem *> all = items();
    bool anySelected = false;

    for (auto *it : all) {
        if (it->flags() & QGraphicsItem::ItemIsSelectable) {
            QRectF sbr = it->sceneBoundingRect();
            bool hit = lassoPath.intersects(it->mapToScene(it->shape())) ||
                       lassoPath.contains(sbr.center()) ||
                       lassoPath.intersects(sbr);
            if (hit) {
                it->setSelected(true);
                anySelected = true;
            }
        }
    }

    emit selectionStateChanged(anySelected);
}

QRectF VectorCanvasScene::selectionBoundingRect() const {
    auto selected = selectedItems();
    if (selected.isEmpty()) return QRectF();
    QRectF rect;
    for (auto *it : selected) {
        rect = rect.united(it->sceneBoundingRect());
    }
    return rect;
}

VectorCanvasScene::SelectionHandle VectorCanvasScene::hitTestSelectionHandle(const QPointF &pos) const {
    QRectF sel = selectionBoundingRect();
    if (sel.isEmpty()) return SelectionHandle::None;

    QRectF box = sel.adjusted(-8, -8, 8, 8);
    const qreal handleRadius = 12.0;

    auto hit = [pos, handleRadius](const QPointF &corner) {
        return std::hypot(pos.x() - corner.x(), pos.y() - corner.y()) <= handleRadius;
    };

    if (hit(box.topLeft())) return SelectionHandle::TopLeft;
    if (hit(box.topRight())) return SelectionHandle::TopRight;
    if (hit(box.bottomRight())) return SelectionHandle::BottomRight;
    if (hit(box.bottomLeft())) return SelectionHandle::BottomLeft;

    if (box.contains(pos)) return SelectionHandle::Inside;

    return SelectionHandle::None;
}

void VectorCanvasScene::deleteSelectedItems() {
    auto selected = selectedItems();
    if (!selected.isEmpty()) {
        m_undoStack.push(new DeleteItemsCommand(this, selected));
        emit selectionStateChanged(false);
        emit contentModified();
        invalidate(sceneRect(), ForegroundLayer);
        update();
    }
}

void VectorCanvasScene::changeSelectedColor(const QColor &color) {
    QVector<ChangeColorCommand::ItemColor> strokeChanges;
    QVector<ChangeColorCommand::TextBlockColor> textChanges;

    for (auto *it : selectedItems()) {
        if (it->type() == StrokeItem::Type) {
            auto *stroke = static_cast<StrokeItem *>(it);
            if (stroke->color() != color) {
                strokeChanges.append({stroke, stroke->color(), color});
                stroke->setColor(color);
            }
        } else if (it->type() == TextBlockItem::Type) {
            auto *textBlock = static_cast<TextBlockItem *>(it);
            if (textBlock->textColor() != color) {
                textChanges.append({textBlock, textBlock->textColor(), color});
                textBlock->setTextColor(color);
            }
        }
    }
    if (!strokeChanges.isEmpty() || !textChanges.isEmpty()) {
        m_undoStack.push(new ChangeColorCommand(strokeChanges, textChanges));
        emit contentModified();
    }
    invalidate(sceneRect(), ForegroundLayer);
    update();
}

void VectorCanvasScene::duplicateSelectedItems() {
    auto selected = selectedItems();
    if (selected.isEmpty()) return;

    clearSelection();
    QList<QGraphicsItem *> newItems;

    for (auto *it : selected) {
        if (it->type() == StrokeItem::Type) {
            auto *orig = static_cast<StrokeItem *>(it);
            auto *copy = new StrokeItem(orig->toolType(), orig->color(), orig->baseWidth());
            QVector<StrokePoint> movedPts;
            for (const auto &p : orig->points()) {
                movedPts.append(StrokePoint(p.pos + QPointF(25, 25), p.pressure, p.tiltX, p.tiltY, p.timestamp));
            }
            copy->setPoints(movedPts);
            addItem(copy);
            copy->setSelected(true);
            newItems.append(copy);
        } else if (it->type() == TextBlockItem::Type) {
            auto *orig = static_cast<TextBlockItem *>(it);
            auto *copy = new TextBlockItem(orig->pos() + QPointF(25, 25));
            copy->deserialize(orig->serialize());
            addItem(copy);
            copy->setSelected(true);
            newItems.append(copy);
        }
    }

    if (!newItems.isEmpty()) {
        m_undoStack.beginMacro("Duplicar Selección");
        for (auto *it : newItems) {
            if (it->type() == StrokeItem::Type) {
                m_undoStack.push(new AddStrokeCommand(this, static_cast<StrokeItem *>(it)));
            } else {
                m_undoStack.push(new AddStemBlockCommand(this, it));
            }
        }
        m_undoStack.endMacro();
        emit selectionStateChanged(true);
        emit contentModified();
        invalidate(sceneRect(), ForegroundLayer);
        update();
    }
}

void VectorCanvasScene::scaleSelectedItems(qreal factor, const QPointF &pivot) {
    if (factor <= 0.05 || factor > 20.0 || std::abs(factor - 1.0) < 1e-4) return;
    for (auto *it : selectedItems()) {
        if (it->type() == StrokeItem::Type) {
            static_cast<StrokeItem *>(it)->scaleStroke(factor, pivot);
        } else {
            it->setPos(pivot + (it->pos() - pivot) * factor);
        }
    }
    m_undoStack.push(new ScaleStrokesCommand(selectedItems(), factor, pivot));
    emit contentModified();
    invalidate(sceneRect(), ForegroundLayer);
    update();
}

QVector<StrokeItem *> VectorCanvasScene::allStrokes() const {
    QVector<StrokeItem *> result;
    for (auto *it : items()) {
        if (it->type() == StrokeItem::Type) {
            result.append(static_cast<StrokeItem *>(it));
        }
    }
    return result;
}

void VectorCanvasScene::clearCanvas() {
    m_undoStack.clear();
    clear();
    clearPdfBackground();
    update();
}

void VectorCanvasScene::rebuildGridCache() {
    if (m_pageSize.isEmpty()) return;
    int w = qMax(100, static_cast<int>(std::ceil(m_pageSize.width())));
    int h = qMax(100, static_cast<int>(std::ceil(m_pageSize.height())));

    m_gridCache = QPixmap(w, h);
    m_gridCache.fill(QColor("#14161C")); // Dark Mode Sheet Surface (#14161C)

    QPainter painter(&m_gridCache);
    if (!m_pdfImage.isNull()) {
        painter.drawImage(QRectF(0, 0, w, h), m_pdfImage);
    }

    renderGridToPainter(&painter, QRectF(0, 0, w, h));
    m_gridCacheDirty = false;
}

void VectorCanvasScene::drawBackground(QPainter *painter, const QRectF &rect) {
    // 1. Fill outer workspace desk area (outside the sheet)
    painter->fillRect(rect, QColor("#090A0F"));

    const QRectF sheet = pageRect();
    const QRectF visibleSheet = sheet.intersected(rect);

    // 2. Strong page drop shadow
    painter->fillRect(sheet.translated(8.0, 8.0).intersected(rect), QColor(0, 0, 0, 180));

    if (!visibleSheet.isEmpty()) {
        if (m_gridCacheDirty || m_gridCache.isNull() || m_gridCache.size() != m_pageSize.toSize()) {
            rebuildGridCache();
        }

        // 3. Fast GPU Pixmap Blit of the cached sheet and grid
        QRectF sourceRect = visibleSheet.translated(-sheet.topLeft());
        painter->drawPixmap(visibleSheet.topLeft(), m_gridCache, sourceRect.toRect());
    }

    // 4. Crisp Sheet Border so the user clearly sees the page limits
    painter->save();
    painter->setPen(QPen(QColor("#546694"), 2.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(sheet);
    painter->restore();
}

void VectorCanvasScene::drawGrid(QPainter *painter, const QRectF &rect) {
    renderGridToPainter(painter, rect);
}

void VectorCanvasScene::renderGridToPainter(QPainter *painter, const QRectF &rect) {
    if (m_bgType == BackgroundType::Blank) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    qreal left = rect.left();
    qreal right = rect.right();
    qreal top = rect.top();
    qreal bottom = rect.bottom();

    if (m_bgType == BackgroundType::CalculusGrid) {
        // Minor grid every 25px, major master lines every 125px (5x)
        const qreal minorStep = 25.0;
        const qreal majorStep = 125.0;

        QPen minorPen(QColor("#36415e"), 1.0);
        QPen majorPen(QColor("#546694"), 1.5);

        qreal startX = std::floor(left / minorStep) * minorStep;
        for (qreal x = startX; x <= right; x += minorStep) {
            bool isMajor = std::abs(std::fmod(x, majorStep)) < 1.0;
            painter->setPen(isMajor ? majorPen : minorPen);
            painter->drawLine(QPointF(x, top), QPointF(x, bottom));
        }

        qreal startY = std::floor(top / minorStep) * minorStep;
        for (qreal y = startY; y <= bottom; y += minorStep) {
            bool isMajor = std::abs(std::fmod(y, majorStep)) < 1.0;
            painter->setPen(isMajor ? majorPen : minorPen);
            painter->drawLine(QPointF(left, y), QPointF(right, y));
        }

    } else if (m_bgType == BackgroundType::EngineeringMillimeter) {
        // 10px minor, 50px medium, 100px major
        const qreal step1 = 10.0;
        QPen pen1(QColor("#2e3852"), 0.8);
        QPen pen2(QColor("#435278"), 1.1);
        QPen pen3(QColor("#6176ab"), 1.5);

        qreal startX = std::floor(left / step1) * step1;
        for (qreal x = startX; x <= right; x += step1) {
            if (std::abs(std::fmod(x, 100.0)) < 1.0) {
                painter->setPen(pen3);
            } else if (std::abs(std::fmod(x, 50.0)) < 1.0) {
                painter->setPen(pen2);
            } else {
                painter->setPen(pen1);
            }
            painter->drawLine(QPointF(x, top), QPointF(x, bottom));
        }

        qreal startY = std::floor(top / step1) * step1;
        for (qreal y = startY; y <= bottom; y += step1) {
            if (std::abs(std::fmod(y, 100.0)) < 1.0) {
                painter->setPen(pen3);
            } else if (std::abs(std::fmod(y, 50.0)) < 1.0) {
                painter->setPen(pen2);
            } else {
                painter->setPen(pen1);
            }
            painter->drawLine(QPointF(left, y), QPointF(right, y));
        }

    } else if (m_bgType == BackgroundType::Isometric) {
        // 30 degree isometric triangular grid
        const qreal h = 30.0;
        const qreal w = h * std::sqrt(3.0);

        QPen isoPen(QColor("#415075"), 1.1);
        painter->setPen(isoPen);

        // Horizontal lines
        qreal startY = std::floor(top / h) * h;
        for (qreal y = startY; y <= bottom; y += h) {
            painter->drawLine(QPointF(left, y), QPointF(right, y));
        }

        // Slanted lines at +30 degrees and -30 degrees
        qreal diagSpan = (right - left) + (bottom - top) * 2.0;
        qreal diagStart = left - (bottom - top) * 1.5;

        for (qreal d = diagStart; d <= right + diagSpan; d += w) {
            painter->drawLine(QPointF(d, top), QPointF(d + (bottom - top) / std::sqrt(3.0), bottom));
            painter->drawLine(QPointF(d, top), QPointF(d - (bottom - top) / std::sqrt(3.0), bottom));
        }

    } else if (m_bgType == BackgroundType::Dotted) {
        const qreal dotStep = 25.0;
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor("#687ea8"));

        qreal startX = std::floor(left / dotStep) * dotStep;
        qreal startY = std::floor(top / dotStep) * dotStep;

        for (qreal x = startX; x <= right; x += dotStep) {
            for (qreal y = startY; y <= bottom; y += dotStep) {
                painter->drawEllipse(QPointF(x, y), 1.8, 1.8);
            }
        }
    }

    painter->restore();
}

void VectorCanvasScene::drawForeground(QPainter *painter, const QRectF & /*rect*/) {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (m_isLassoing && m_lassopoly.size() > 1) {
        painter->setPen(QPen(QColor("#7aa2f7"), 1.5, Qt::DashLine));
        painter->setBrush(QColor(122, 162, 247, 35));
        painter->drawPolygon(m_lassopoly);
    }

    QRectF sel = selectionBoundingRect();
    if (!sel.isEmpty()) {
        QRectF box = sel.adjusted(-8, -8, 8, 8);

        // 1. Translucent selection wash
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(122, 162, 247, 20));
        painter->drawRoundedRect(box, 6.0, 6.0);

        // 2. High-contrast dashed border
        // Dark outer halo for contrast on bright white PDFs
        painter->setPen(QPen(QColor(15, 17, 26, 220), 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(box, 6.0, 6.0);

        // Bright electric cyan/blue dashed line
        painter->setPen(QPen(QColor("#7dcfff"), 1.6, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
        painter->drawRoundedRect(box, 6.0, 6.0);

        // 3. Corner Resize Handles (white circular grips with blue outline and drop shadow)
        const qreal r = 5.0;
        const QPointF corners[] = { box.topLeft(), box.topRight(), box.bottomRight(), box.bottomLeft() };

        for (const auto &c : corners) {
            // Shadow
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(0, 0, 0, 180));
            painter->drawEllipse(c + QPointF(1.0, 1.0), r, r);

            // Grip body
            painter->setPen(QPen(QColor("#7aa2f7"), 1.8));
            painter->setBrush(QColor("#ffffff"));
            painter->drawEllipse(c, r, r);
        }

        // 4. Center Move Crosshair Indicator
        QPointF center = box.center();
        painter->setPen(QPen(QColor(15, 17, 26, 220), 2.2));
        painter->drawLine(center - QPointF(6, 0), center + QPointF(6, 0));
        painter->drawLine(center - QPointF(0, 6), center + QPointF(0, 6));

        painter->setPen(QPen(QColor("#7dcfff"), 1.4));
        painter->drawLine(center - QPointF(6, 0), center + QPointF(6, 0));
        painter->drawLine(center - QPointF(0, 6), center + QPointF(0, 6));
    }

    painter->restore();
}

} // namespace OmniNote
