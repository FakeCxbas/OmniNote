#include "VectorCanvasScene.hpp"
#include "UndoCommands.hpp"
#include "VirtualRulerItem.hpp"
#include "../stem/ImageBlockItem.hpp"
#include "../stem/FormulaBlockItem.hpp"
#include "../stem/TextBlockItem.hpp"
#include "../stem/CodeBlockItem.hpp"
#include "../stem/FunctionPlotItem.hpp"
#include <QPainter>
#include <QtMath>
#include <QGraphicsView>
#include <QDateTime>
#include <QSet>
#include <QMap>
#include <QAtomicInteger>

namespace OmniNote {

static QAtomicInteger<qint64> s_globalGroupIdCounter(100);
static inline qint64 generateUniqueGroupId() {
    return QDateTime::currentMSecsSinceEpoch() * 10000 + (s_globalGroupIdCounter.fetchAndAddRelaxed(1) % 10000);
}

VectorCanvasScene::VectorCanvasScene(qreal width, qreal height, QObject *parent)
    : QGraphicsScene(parent),
      m_pageSize(width, height),
      m_bgType(BackgroundType::CalculusGrid),
      m_currentTool(ToolType::Pen),
      m_currentColor(QColor("#ffffff")),
      m_currentBaseWidth(3.5) {
    setSceneRect(0, 0, m_pageSize.width(), m_pageSize.height());
    setItemIndexMethod(QGraphicsScene::NoIndex);
    connect(&m_undoStack, &QUndoStack::cleanChanged, this, [this](bool clean) {
        if (!clean) {
            emit contentModified();
        }
    });
}

void VectorCanvasScene::setPageSize(qreal width, qreal height) {
    m_pageSize = QSizeF(width, height);
    setSceneRect(0, 0, width, height);
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
    invalidate(sceneRect(), BackgroundLayer);
    update();
}

bool VectorCanvasScene::isLightBackground() const {
    return m_bgType == BackgroundType::CanvaGrid ||
           m_bgType == BackgroundType::WarmSepia ||
           m_bgType == BackgroundType::BlankLight;
}

QColor VectorCanvasScene::sheetColor() const {
    switch (m_bgType) {
    case BackgroundType::CanvaGrid:
    case BackgroundType::BlankLight:
        return QColor("#ffffff");
    case BackgroundType::WarmSepia:
        return QColor("#fbf8f1");
    case BackgroundType::ChalkboardGreen:
        return QColor("#182920");
    case BackgroundType::CalculusGrid:
    case BackgroundType::EngineeringMillimeter:
    case BackgroundType::Isometric:
    case BackgroundType::Dotted:
    case BackgroundType::Blank:
    default:
        return QColor("#14161c");
    }
}

QColor VectorCanvasScene::deskColor() const {
    switch (m_bgType) {
    case BackgroundType::CanvaGrid:
    case BackgroundType::BlankLight:
    case BackgroundType::WarmSepia:
        return QColor("#14161f");
    case BackgroundType::ChalkboardGreen:
        return QColor("#0d1712");
    case BackgroundType::CalculusGrid:
    case BackgroundType::EngineeringMillimeter:
    case BackgroundType::Isometric:
    case BackgroundType::Dotted:
    case BackgroundType::Blank:
    default:
        return QColor("#090a0f");
    }
}

QColor VectorCanvasScene::borderColor() const {
    switch (m_bgType) {
    case BackgroundType::CanvaGrid:
    case BackgroundType::BlankLight:
        return QColor("#d0d7de");
    case BackgroundType::WarmSepia:
        return QColor("#dcd3c4");
    case BackgroundType::ChalkboardGreen:
        return QColor("#325240");
    case BackgroundType::CalculusGrid:
    case BackgroundType::EngineeringMillimeter:
    case BackgroundType::Isometric:
    case BackgroundType::Dotted:
    case BackgroundType::Blank:
    default:
        return QColor("#546694");
    }
}

QColor VectorCanvasScene::shadowColor() const {
    if (isLightBackground()) {
        return QColor(0, 0, 0, 110);
    }
    return QColor(0, 0, 0, 180);
}

qreal VectorCanvasScene::gridRowHeight() const {
    if (m_bgType == BackgroundType::CanvaGrid) {
        return (m_pageSize.height() - 88.0 - 148.0) / 48.0; // 31.625 px
    }
    if (m_bgType == BackgroundType::CalculusGrid) {
        return 25.0;
    }
    if (m_bgType == BackgroundType::EngineeringMillimeter) {
        return 20.0;
    }
    return 31.625;
}

void VectorCanvasScene::setHeaderTitle(const QString &title) {
    if (m_headerTitle != title) {
        m_headerTitle = title;
        invalidate(sceneRect(), BackgroundLayer);
        update();
    }
}

QRectF VectorCanvasScene::headerTitleRect() const {
    if (m_bgType != BackgroundType::CanvaGrid) {
        return QRectF();
    }
    const qreal marginL = 80.0;
    QString title = m_headerTitle.trimmed();
    if (title.isEmpty()) {
        title = QStringLiteral("Tarea de Matematicas I");
    }
    QFont titleFont("Inter", 16, QFont::Bold);
    titleFont.setStyleHint(QFont::SansSerif);
    QFontMetrics fm(titleFont);
    int textW = fm.horizontalAdvance(title);
    return QRectF(marginL - 8.0, 78.0, textW + 48.0, 38.0);
}

bool VectorCanvasScene::isHeaderTitleHit(const QPointF &scenePos) const {
    return headerTitleRect().contains(scenePos);
}

void VectorCanvasScene::setPdfBackground(const QImage &img, int pageNumber) {
    m_pdfImage = img;
    m_pdfPageNumber = pageNumber;
    invalidate(sceneRect(), BackgroundLayer);
    update();
}

void VectorCanvasScene::clearPdfBackground() {
    m_pdfImage = QImage();
    m_pdfPageNumber = -1;
    invalidate(sceneRect(), BackgroundLayer);
    update();
}

void VectorCanvasScene::setCurrentTool(ToolType tool) {
    if (m_currentTool != tool) {
        m_currentTool = tool;

        const bool isCursorOrText = (tool == ToolType::Cursor || tool == ToolType::Text);
        for (QGraphicsItem *it : items()) {
            if (it->type() == TextBlockItem::Type) {
                auto *tb = static_cast<TextBlockItem *>(it);
                tb->setInteractiveMode(isCursorOrText);
            }
        }

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

    if (isHeaderTitleHit(clampedPos) && activeTool != ToolType::Eraser) {
        emit headerTitleEditRequested();
        return;
    }

    // Deselect any selected items immediately if clicking/drawing outside them and outside the selection bounding box
    if (activeTool != ToolType::Lasso && activeTool != ToolType::Cursor && !selectedItems().isEmpty()) {
        bool hitSelectedItem = (hitTestSelectionHandle(clampedPos) != SelectionHandle::None);
        if (!hitSelectedItem) {
            const auto itemsUnder = items(clampedPos);
            for (auto *it : itemsUnder) {
                if (it->isSelected()) {
                    hitSelectedItem = true;
                    break;
                }
            }
        }
        if (!hitSelectedItem) {
            clearSelection();
            emit selectionStateChanged(false);
            invalidate(sceneRect(), ForegroundLayer);
            update();
        }
    }

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

    if (activeTool == ToolType::Lasso || activeTool == ToolType::Cursor) {
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

        if (activeTool == ToolType::Lasso) {
            clearSelection();
            emit selectionStateChanged(false);
            m_isLassoing = true;
            m_lassopoly.clear();
            m_lassopoly << clampedPos;
            invalidate(sceneRect(), ForegroundLayer);
            return;
        } else {
            // Cursor tool clicked outside selection
            QGraphicsItem *topItem = itemAt(clampedPos, QTransform());
            if (!topItem || topItem->type() == StrokeItem::Type) {
                clearSelection();
                emit selectionStateChanged(false);
                invalidate(sceneRect(), ForegroundLayer);
                update();
            }
            return;
        }
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
    m_lastInputPressure = pressure;

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

        // Distance filter to drop micro-tremors and redundant events (< 0.2px) unless pressure changed significantly
        qreal dx = effectivePos.x() - m_lastInputPos.x();
        qreal dy = effectivePos.y() - m_lastInputPos.y();
        qreal dp = std::abs(pressure - m_lastInputPressure);
        if (dx * dx + dy * dy < 0.04 && dp < 0.02) {
            return;
        }
        m_lastInputPos = effectivePos;
        m_lastInputPressure = pressure;

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
            m_undoStack.push(new MoveItemsCommand(selectedItems(), m_moveTotalDelta, true));
            emit contentModified();
        }
        invalidate(sceneRect(), ForegroundLayer);
        return;
    }

    if (m_isScalingSelection) {
        m_isScalingSelection = false;
        if (std::abs(m_scaleTotalFactor - 1.0) > 0.01) {
            m_undoStack.push(new ScaleStrokesCommand(selectedItems(), m_scaleTotalFactor, m_scalePivot, true));
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
        StrokePoint rawPt(effectivePos, pressure, tx, ty, QDateTime::currentMSecsSinceEpoch());

        if (m_activeStrokeItem->toolType() == ToolType::Pen || m_activeStrokeItem->toolType() == ToolType::Highlighter) {
            StrokePoint filtered = m_smoother.filterPoint(rawPt);
            if (m_activePoints.isEmpty() || std::hypot(effectivePos.x() - m_activePoints.last().pos.x(), effectivePos.y() - m_activePoints.last().pos.y()) > 0.2) {
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
            // Search with a generous click tolerance radius (14px) so thin handwriting strokes are easily selected
            QPainterPath clickArea;
            clickArea.addEllipse(clickPos, 14.0, 14.0);
            QList<QGraphicsItem *> clicked = items(clickArea);
            for (auto *it : clicked) {
                QGraphicsItem *curr = it;
                while (curr && !(curr->flags() & QGraphicsItem::ItemIsSelectable)) {
                    curr = curr->parentItem();
                }
                if (curr && (curr->flags() & QGraphicsItem::ItemIsSelectable)) {
                    curr->setSelected(true);
                    expandSelectionToGroups();
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

    if (anySelected) {
        expandSelectionToGroups();
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
    const qreal maxHandleRadius = 12.0;
    const qreal handleRadius = qMin(maxHandleRadius, qMin(box.width(), box.height()) * 0.3);

    auto hit = [pos, handleRadius](const QPointF &corner) {
        return std::hypot(pos.x() - corner.x(), pos.y() - corner.y()) <= handleRadius;
    };

    if (box.width() >= 24.0 && box.height() >= 24.0) {
        if (hit(box.topLeft())) return SelectionHandle::TopLeft;
        if (hit(box.topRight())) return SelectionHandle::TopRight;
        if (hit(box.bottomRight())) return SelectionHandle::BottomRight;
        if (hit(box.bottomLeft())) return SelectionHandle::BottomLeft;
    }

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
    QList<QGraphicsItem *> origItems;

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
            origItems.append(orig);
        } else if (it->type() == TextBlockItem::Type) {
            auto *orig = static_cast<TextBlockItem *>(it);
            auto *copy = new TextBlockItem(orig->pos() + QPointF(25, 25));
            copy->deserialize(orig->serialize());
            addItem(copy);
            copy->setSelected(true);
            newItems.append(copy);
            origItems.append(orig);
        }
    }

    // Preserve grouping on duplicated items with fresh group IDs
    QMap<qint64, qint64> oldToNewGroups;
    for (int i = 0; i < origItems.size() && i < newItems.size(); ++i) {
        qint64 origGid = getItemGroupId(origItems[i]);
        if (origGid > 0) {
            if (!oldToNewGroups.contains(origGid)) {
                oldToNewGroups[origGid] = generateUniqueGroupId();
            }
            setItemGroupId(newItems[i], oldToNewGroups[origGid]);
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

qint64 VectorCanvasScene::getItemGroupId(const QGraphicsItem *item) {
    if (!item) return 0;
    if (item->type() == StrokeItem::Type) {
        return static_cast<const StrokeItem *>(item)->groupId();
    }
    return item->data(StrokeItem::UserRoleGroupId).toLongLong();
}

void VectorCanvasScene::setItemGroupId(QGraphicsItem *item, qint64 groupId) {
    if (!item) return;
    item->setData(StrokeItem::UserRoleGroupId, groupId > 0 ? QVariant(groupId) : QVariant());
    if (item->type() == StrokeItem::Type) {
        static_cast<StrokeItem *>(item)->setGroupId(groupId);
    }
}

void VectorCanvasScene::expandSelectionToGroups() {
    QSet<qint64> groupIds;
    for (auto *it : selectedItems()) {
        qint64 gid = getItemGroupId(it);
        if (gid > 0) {
            groupIds.insert(gid);
        }
    }
    if (groupIds.isEmpty()) return;

    for (auto *it : items()) {
        if (it->flags() & QGraphicsItem::ItemIsSelectable) {
            qint64 gid = getItemGroupId(it);
            if (gid > 0 && groupIds.contains(gid) && !it->isSelected()) {
                it->setSelected(true);
            }
        }
    }
}

bool VectorCanvasScene::canGroupSelection() const {
    auto selected = selectedItems();
    if (selected.size() < 2) return false;

    qint64 firstGid = getItemGroupId(selected.first());
    if (firstGid == 0) return true;

    for (int i = 1; i < selected.size(); ++i) {
        if (getItemGroupId(selected[i]) != firstGid) {
            return true;
        }
    }
    return false;
}

bool VectorCanvasScene::canUngroupSelection() const {
    for (auto *it : selectedItems()) {
        if (getItemGroupId(it) > 0) return true;
    }
    return false;
}

void VectorCanvasScene::groupSelectedItems() {
    auto selected = selectedItems();
    if (selected.size() < 2) return;

    if (!canGroupSelection()) return;

    qint64 newGroupId = generateUniqueGroupId();

    QVector<GroupCommand::ItemPreviousGroup> prevGroups;
    prevGroups.reserve(selected.size());

    for (auto *it : selected) {
        prevGroups.append({it, getItemGroupId(it)});
        setItemGroupId(it, newGroupId);
    }

    m_undoStack.push(new GroupCommand(this, prevGroups, newGroupId));
    emit selectionStateChanged(true);
    emit contentModified();
    invalidate(sceneRect(), ForegroundLayer);
    update();
}

void VectorCanvasScene::ungroupSelectedItems() {
    auto selected = selectedItems();
    if (selected.isEmpty()) return;

    QVector<UngroupCommand::ItemPreviousGroup> prevGroups;
    for (auto *it : selected) {
        qint64 gid = getItemGroupId(it);
        if (gid > 0) {
            prevGroups.append({it, gid});
            setItemGroupId(it, 0);
        }
    }
    if (prevGroups.isEmpty()) return;

    m_undoStack.push(new UngroupCommand(this, prevGroups));
    emit selectionStateChanged(true);
    emit contentModified();
    invalidate(sceneRect(), ForegroundLayer);
    update();
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
    m_undoStack.push(new ScaleStrokesCommand(selectedItems(), factor, pivot, true));
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

void VectorCanvasScene::drawBackground(QPainter *painter, const QRectF &rect) {
    // 1. Fill outer workspace desk area (outside the sheet)
    painter->fillRect(rect, deskColor());

    const QRectF sheet = pageRect();
    const QRectF visibleSheet = sheet.intersected(rect);

    // 2. Drop shadow with depth depending on theme
    const qreal shadowOffset = isLightBackground() ? 5.0 : 8.0;
    painter->fillRect(sheet.translated(shadowOffset, shadowOffset).intersected(rect), shadowColor());

    if (!visibleSheet.isEmpty()) {
        // 3. Lossless Vector Sheet Fill
        painter->fillRect(visibleSheet, sheetColor());

        if (!m_pdfImage.isNull()) {
            painter->drawImage(sheet, m_pdfImage);
        }

        // 4. Pure Vector Grid & Typography (infinite resolution, SVG quality at any zoom level)
        renderGridToPainter(painter, visibleSheet);
    }

    // 5. Crisp Sheet Border so the user clearly sees the page limits
    painter->save();
    painter->setPen(QPen(borderColor(), isLightBackground() ? 1.0 : 2.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(sheet);
    painter->restore();
}

void VectorCanvasScene::drawGrid(QPainter *painter, const QRectF &rect) {
    renderGridToPainter(painter, rect);
}

void VectorCanvasScene::renderGridToPainter(QPainter *painter, const QRectF &rect) {
    if (m_bgType == BackgroundType::Blank || m_bgType == BackgroundType::BlankLight) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    qreal left = rect.left();
    qreal right = rect.right();
    qreal top = rect.top();
    qreal bottom = rect.bottom();

    if (m_bgType == BackgroundType::CanvaGrid) {
        // Canva Style Student Grid:
        // Proportions accurately mapped from Canva template (35 columns x 48 rows, 5mm grid)
        // Left margin: 80.0px, Right margin: 80.0px -> Grid width = 1080.0px (35 columns)
        // Top header margin: 148.0px, Bottom margin: 88.0px -> Grid height = 1518.0px (48 rows)
        const qreal marginL = 80.0;
        const qreal marginR = m_pageSize.width() - 80.0;
        const qreal marginT = 148.0;
        const qreal marginB = m_pageSize.height() - 88.0;

        const int cols = 35;
        const int rows = 48;
        const qreal stepX = (marginR - marginL) / static_cast<qreal>(cols);
        const qreal stepY = (marginB - marginT) / static_cast<qreal>(rows);

        // 1. Grid Lines (delicate #d9d9d9 matching Canva SVG stroke)
        QPen linePen(QColor("#d9d9d9"), 0.85);
        painter->setPen(linePen);

        qreal x1 = qMax(marginL, left - 2.0);
        qreal x2 = qMin(marginR, right + 2.0);
        if (x1 < x2) {
            for (int i = 0; i <= rows; ++i) {
                qreal y = marginT + i * stepY;
                if (y >= top - 2.0 && y <= bottom + 2.0) {
                    painter->drawLine(QPointF(x1, y), QPointF(x2, y));
                }
            }
        }

        qreal y1 = qMax(marginT, top - 2.0);
        qreal y2 = qMin(marginB, bottom + 2.0);
        if (y1 < y2) {
            for (int j = 0; j <= cols; ++j) {
                qreal x = marginL + j * stepX;
                if (x >= left - 2.0 && x <= right + 2.0) {
                    painter->drawLine(QPointF(x, y1), QPointF(x, y2));
                }
            }
        }

        // 2. Framed Grid Box Border
        painter->drawRect(QRectF(marginL, marginT, marginR - marginL, marginB - marginT));

        // 3. Header Title in top margin area (e.g. "Tarea de Matematicas I")
        if (top <= 140.0 && bottom >= 70.0 && right >= marginL) {
            QString title = m_headerTitle.trimmed();
            if (title.isEmpty()) {
                title = QStringLiteral("Tarea de Matematicas I");
            }
            QFont titleFont("Inter", 16, QFont::Bold);
            titleFont.setStyleHint(QFont::SansSerif);
            painter->setFont(titleFont);
            painter->setPen(QColor("#111827"));
            painter->drawText(QPointF(marginL, 108.0), title);

            // Visual edit indicator badge (subtle pencil icon)
            QFontMetrics fm(titleFont);
            int textW = fm.horizontalAdvance(title);
            QRectF pencilRect(marginL + textW + 8.0, 89.0, 20.0, 20.0);
            painter->setPen(QPen(QColor(107, 114, 128, 120), 1.0));
            painter->setBrush(QColor(243, 244, 246, 220));
            painter->drawRoundedRect(pencilRect, 4.0, 4.0);
            QFont iconFont("Inter", 8, QFont::Bold);
            painter->setFont(iconFont);
            painter->setPen(QColor("#4b5563"));
            painter->drawText(pencilRect, Qt::AlignCenter, "✎");
        }

    } else if (m_bgType == BackgroundType::CalculusGrid) {
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

    } else if (m_bgType == BackgroundType::WarmSepia) {
        // Warm Cream Paper with subtle warm lines
        const qreal step = 25.0;
        QPen sepiaPen(QColor("#e2d8c6"), 0.9);
        painter->setPen(sepiaPen);

        qreal startX = std::floor(left / step) * step;
        for (qreal x = startX; x <= right; x += step) {
            painter->drawLine(QPointF(x, top), QPointF(x, bottom));
        }

        qreal startY = std::floor(top / step) * step;
        for (qreal y = startY; y <= bottom; y += step) {
            painter->drawLine(QPointF(left, y), QPointF(right, y));
        }

    } else if (m_bgType == BackgroundType::ChalkboardGreen) {
        // Classic Chalkboard with chalk green grid
        const qreal minorStep = 25.0;
        const qreal majorStep = 125.0;

        QPen minorPen(QColor(65, 105, 80, 100), 0.9);
        QPen majorPen(QColor(85, 135, 105, 150), 1.3);

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
    bool hasOnlySingleStemBlock = false;
    const auto selItems = selectedItems();
    if (selItems.size() == 1) {
        int t = selItems.first()->type();
        if (t == ImageBlockItem::Type || t == TextBlockItem::Type ||
            t == FormulaBlockItem::Type || t == FunctionPlotItem::Type ||
            t == CodeBlockItem::Type) {
            hasOnlySingleStemBlock = true;
        }
    }

    if (!sel.isEmpty() && !hasOnlySingleStemBlock) {
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
