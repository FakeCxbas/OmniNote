#pragma once

#include <QCursor>
#include <QColor>
#include <QPixmap>
#include <QPainter>
#include "StrokeItem.hpp"

namespace OmniNote {

class CanvasCursorFactory {
public:
    // Generates a high-contrast, specialized tool cursor with subpixel precision
    static QCursor createCursor(ToolType tool, const QColor &color = Qt::white, qreal width = 2.5);

    // Specialized cursor creators
    static QCursor createPenCursor(const QColor &color, qreal width = 2.5);
    static QCursor createHighlighterCursor(const QColor &color);
    static QCursor createEraserCursor(qreal radius = 14.0);
    static QCursor createLassoCursor();
    static QCursor createShapeCursor(ToolType tool);
};

} // namespace OmniNote
