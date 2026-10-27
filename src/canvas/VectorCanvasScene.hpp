#pragma once

#include <QGraphicsScene>
#include <QUndoStack>
#include <QImage>
#include <QPolygonF>
#include <QColor>
#include <QVector>
#include "StrokeItem.hpp"
#include "../input/StrokeSmoother.hpp"

namespace OmniNote {

enum class BackgroundType {
    CalculusGrid = 0,
    EngineeringMillimeter = 1,
    Isometric = 2,
    Dotted = 3,
    Blank = 4
};

class VectorCanvasScene : public QGraphicsScene {
    Q_OBJECT

public:
    explicit VectorCanvasScene(qreal width = 1240, qreal height = 1754, QObject *parent = nullptr);
    ~VectorCanvasScene() override = default;

    // Canvas configuration
    void setPageSize(qreal width, qreal height);
    QSizeF pageSize() const { return m_pageSize; }
    QRectF pageRect() const { return QRectF(0.0, 0.0, m_pageSize.width(), m_pageSize.height()); }
    QPointF clampToPage(const QPointF &scenePos, qreal margin = 2.0) const;

    BackgroundType backgroundType() const { return m_bgType; }
    void setBackgroundType(BackgroundType type);

    void setPdfBackground(const QImage &img, int pageNumber = 0);
    void clearPdfBackground();
    bool hasPdfBackground() const { return !m_pdfImage.isNull(); }
    const QImage &pdfBackground() const { return m_pdfImage; }

    // Tools and parameters
    ToolType currentTool() const { return m_currentTool; }
    void setCurrentTool(ToolType tool);

    QColor currentColor() const { return m_currentColor; }
    void setCurrentColor(const QColor &c);

    qreal currentBaseWidth() const { return m_currentBaseWidth; }
    void setCurrentBaseWidth(qreal w);

    void setAutoImproveEnabled(bool enable) { m_autoImproveEnabled = enable; }
    bool autoImproveEnabled() const { return m_autoImproveEnabled; }

    QUndoStack *undoStack() { return &m_undoStack; }

    // Direct input points from tablet or mouse
    void handlePointerPress(const QPointF &scenePos, qreal pressure, qreal tx = 0.0, qreal ty = 0.0, bool isEraser = false);
    void handlePointerMove(const QPointF &scenePos, qreal pressure, qreal tx = 0.0, qreal ty = 0.0, bool isEraser = false);
    void handlePointerRelease(const QPointF &scenePos, qreal pressure, qreal tx = 0.0, qreal ty = 0.0, bool isEraser = false);

    // Lasso operations
    QRectF selectionBoundingRect() const;
    void deleteSelectedItems();
    void changeSelectedColor(const QColor &color);
    void duplicateSelectedItems();
    void scaleSelectedItems(qreal factor, const QPointF &pivot);

    // Serialization of page items
    QVector<StrokeItem *> allStrokes() const;
    void clearCanvas();

    class VirtualRulerItem *virtualRuler() const { return m_activeRuler; }
    void setVirtualRuler(class VirtualRulerItem *ruler) { m_activeRuler = ruler; }

signals:
    void contentModified();
    void cursorMoved(const QPointF &scenePos);
    void selectionStateChanged(bool hasSelection);
    void toolChanged(ToolType tool);
    void colorChanged(const QColor &color);
    void widthChanged(qreal width);
    void textPlacementRequested(const QPointF &scenePos);

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;

public:
    enum class SelectionHandle { None, TopLeft, TopRight, BottomRight, BottomLeft, Inside };
    SelectionHandle hitTestSelectionHandle(const QPointF &pos) const;

private:
    void drawGrid(QPainter *painter, const QRectF &rect);
    void performEraserAt(const QPointF &pos, qreal radius);
    void applyLassoSelection();

    QSizeF m_pageSize;
    BackgroundType m_bgType;
    ToolType m_currentTool;
    QColor m_currentColor;
    qreal m_currentBaseWidth;

    QUndoStack m_undoStack;
    StrokeSmoother m_smoother;

    // PDF Background
    QImage m_pdfImage;
    int m_pdfPageNumber{-1};

    // Cached Background Grid (drastically accelerates repaints & reduces CPU/GPU load)
    QPixmap m_gridCache;
    bool m_gridCacheDirty{true};
    void rebuildGridCache();
    void renderGridToPainter(QPainter *painter, const QRectF &rect);

    // Active stroke in progress
    bool m_isDrawing{false};
    bool m_autoImproveEnabled{true};
    QVector<StrokePoint> m_activePoints;
    StrokeItem *m_activeStrokeItem{nullptr};
    QPointF m_lastInputPos;
    class VirtualRulerItem *m_activeRuler{nullptr};

    // Lasso selection
    bool m_isLassoing{false};
    QPolygonF m_lassopoly;

    // Eraser tracking
    bool m_isErasing{false};
    QPointF m_lastEraserPos;
    qreal m_eraserRadius{14.0};
    QList<QGraphicsItem *> m_erasedStrokesInGesture;

    // Moving and scaling selection
    bool m_isMovingSelection{false};
    bool m_isScalingSelection{false};
    QPointF m_moveStartPos;
    QPointF m_moveTotalDelta;
    SelectionHandle m_scaleHandle{SelectionHandle::None};
    QPointF m_scalePivot;
    qreal m_scaleStartDist{0.0};
    qreal m_scaleLastDist{0.0};
    qreal m_scaleTotalFactor{1.0};
};

} // namespace OmniNote
