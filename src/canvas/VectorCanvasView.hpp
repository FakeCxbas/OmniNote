#pragma once

#include <QGraphicsView>
#include <QOpenGLWidget>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QTabletEvent>
#include <QKeyEvent>
#include "VectorCanvasScene.hpp"

namespace OmniNote {

class VectorCanvasView : public QGraphicsView {
    Q_OBJECT

public:
    explicit VectorCanvasView(VectorCanvasScene *scene, QWidget *parent = nullptr);
    ~VectorCanvasView() override = default;

    VectorCanvasScene *canvasScene() const { return m_canvasScene; }

    qreal currentZoom() const { return m_zoomFactor; }

public slots:
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void fitToWidth();
    void setZoomFactor(qreal factor);
    void updateToolCursor();
    void showLatexEditor(class FormulaBlockItem *item = nullptr);

signals:
    void zoomChanged(qreal zoomFactor);
    void cursorCoordinatesChanged(const QPointF &scenePos);
    void pressureChanged(qreal pressure);
    void eraserModeChanged(bool isEraser);
    void pasteRequested();

protected:
    bool viewportEvent(QEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void tabletEvent(QTabletEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void scrollContentsBy(int dx, int dy) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void applyZoom(qreal factor, const QPointF &centerPoint);
    bool shouldForwardToSceneWidgets(const QPointF &scenePos) const;
    void updateSelectionActionBarPosition();

    VectorCanvasScene *m_canvasScene;
    qreal m_zoomFactor{1.0};
    QCursor m_currentToolCursor;
    class SelectionActionBar *m_selectionActionBar{nullptr};
    class LatexEditorOverlay *m_latexOverlay{nullptr};

    // Pan state
    bool m_isPanning{false};
    QPoint m_panStartPos;
    bool m_spacePressed{false};

    // Tablet vs Mouse isolation & Widget interaction state
    bool m_isTabletStroke{false};
    qint64 m_lastTabletTimeMs{0};
    bool m_interactingWithWidget{false};

    // Dynamic simulated pressure for mouse/trackpad
    QPointF m_lastMousePos;
    qint64 m_lastMouseTimeMs{0};
    qreal m_simulatedPressure{0.65};

    // Stylus barrel button momentary shortcuts
    bool m_stylusEraserActive{false};
    bool m_stylusLassoActive{false};
    ToolType m_preStylusTool{ToolType::Pen};
};

} // namespace OmniNote
