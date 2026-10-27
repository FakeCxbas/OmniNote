#include "CanvasCursorFactory.hpp"
#include <QPolygonF>
#include <QPen>
#include <QBrush>
#include <QtMath>

namespace OmniNote {

QCursor CanvasCursorFactory::createCursor(ToolType tool, const QColor &color, qreal width) {
    switch (tool) {
    case ToolType::Pen:
        return createPenCursor(color, width);
    case ToolType::Highlighter:
        return createHighlighterCursor(color);
    case ToolType::Eraser:
        return createEraserCursor(14.0);
    case ToolType::Lasso:
        return createLassoCursor();
    case ToolType::ShapeLine:
    case ToolType::ShapeAxes2D:
    case ToolType::ShapeAxes3D:
    case ToolType::ShapeRect:
    case ToolType::ShapeEllipse:
        return createShapeCursor(tool);
    case ToolType::Text:
        return QCursor(Qt::IBeamCursor);
    default:
        return createPenCursor(color, width);
    }
}

QCursor CanvasCursorFactory::createPenCursor(const QColor &color, qreal /*width*/) {
    // 32x32 high-resolution cursor with subpixel antialiasing
    QPixmap pix(32, 32);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 1. Stylus Body Shadow / Dark Outline (guarantees visibility over bright white PDFs)
    QPolygonF bodyOutline;
    bodyOutline << QPointF(2.0, 2.0)
                << QPointF(3.5, 9.5)
                << QPointF(15.5, 21.5)
                << QPointF(21.5, 15.5)
                << QPointF(9.5, 3.5);

    p.setPen(QPen(QColor(15, 17, 26, 240), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPolygon(bodyOutline);

    // 2. Stylus Body Core (Tokyo Night dark surface)
    QPolygonF body;
    body << QPointF(3.5, 9.5)
         << QPointF(14.5, 20.5)
         << QPointF(20.5, 14.5)
         << QPointF(9.5, 3.5);

    p.setPen(QPen(QColor(192, 202, 245, 200), 1.0));
    p.setBrush(QColor("#1f2335"));
    p.drawPolygon(body);

    // 3. Metallic Accent Band on the Grip
    QPolygonF accent;
    accent << QPointF(6.5, 6.5)
           << QPointF(10.5, 10.5)
           << QPointF(12.5, 8.5)
           << QPointF(8.5, 4.5);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#7aa2f7"));
    p.drawPolygon(accent);

    // 4. Stylus Nib Cone
    QPolygonF nibCone;
    nibCone << QPointF(2.0, 2.0)
            << QPointF(3.5, 9.5)
            << QPointF(9.5, 3.5);
    p.setPen(QPen(QColor(192, 202, 245), 1.0));
    p.setBrush(QColor("#3b4261"));
    p.drawPolygon(nibCone);

    // 5. Active Ink Tip Dot at Hotspot (2, 2)
    // Dark outer halo for contrast
    p.setPen(QPen(QColor(15, 17, 26, 240), 1.0));
    p.setBrush(color.isValid() ? color : QColor("#ffffff"));
    p.drawEllipse(QPointF(2.0, 2.0), 2.5, 2.5);

    // Inner bright center dot for precision
    qreal lum = 0.299 * color.redF() + 0.587 * color.greenF() + 0.114 * color.blueF();
    QColor innerDotColor = (lum > 0.6) ? QColor(20, 22, 30, 220) : QColor(255, 255, 255, 240);
    p.setPen(Qt::NoPen);
    p.setBrush(innerDotColor);
    p.drawEllipse(QPointF(2.0, 2.0), 0.8, 0.8);

    return QCursor(pix, 2, 2);
}

QCursor CanvasCursorFactory::createHighlighterCursor(const QColor &color) {
    QPixmap pix(32, 32);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 1. Chisel Marker Outer Dark Halo
    QPolygonF outerBody;
    outerBody << QPointF(2.0, 7.0)
              << QPointF(7.0, 2.0)
              << QPointF(22.0, 17.0)
              << QPointF(17.0, 22.0);

    p.setPen(QPen(QColor(15, 17, 26, 240), 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPolygon(outerBody);

    // 2. Chisel Marker Body
    QPolygonF body;
    body << QPointF(5.0, 10.0)
         << QPointF(10.0, 5.0)
         << QPointF(21.0, 16.0)
         << QPointF(16.0, 21.0);

    p.setPen(QPen(QColor(192, 202, 245, 180), 1.0));
    p.setBrush(QColor("#24283b"));
    p.drawPolygon(body);

    // Accent line along marker barrel
    p.setPen(QPen(QColor("#ff9e64"), 1.4));
    p.drawLine(QPointF(8.0, 9.0), QPointF(17.0, 18.0));

    // 3. Chisel Slanted Nib with Active Highlighter Color
    QPolygonF nib;
    nib << QPointF(2.0, 7.0)
        << QPointF(7.0, 2.0)
        << QPointF(10.0, 5.0)
        << QPointF(5.0, 10.0);

    QColor hlColor = color.isValid() ? color : QColor("#ff9e64");
    hlColor.setAlpha(220);

    p.setPen(QPen(QColor("#ffffff"), 1.0));
    p.setBrush(hlColor);
    p.drawPolygon(nib);

    // 4. Hotspot indicator dot at (2, 2)
    p.setPen(QPen(QColor(15, 17, 26), 1.0));
    p.setBrush(QColor("#ffffff"));
    p.drawEllipse(QPointF(2.0, 2.0), 1.8, 1.8);

    return QCursor(pix, 2, 2);
}

QCursor CanvasCursorFactory::createEraserCursor(qreal /*radius*/) {
    // 32x32 circular precision eraser reticle
    QPixmap pix(32, 32);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QPointF center(16.0, 16.0);
    const qreal r = 11.5;

    // 1. Translucent erase footprint wash
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(125, 207, 255, 35));
    p.drawEllipse(center, r, r);

    // 2. High-contrast outer dark halo (contrast on white pages)
    p.setPen(QPen(QColor(15, 17, 26, 230), 2.2));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(center, r, r);

    // 3. Vibrant bright inner circle (contrast on dark Tokyo Night canvas)
    p.setPen(QPen(QColor("#7dcfff"), 1.4, Qt::SolidLine));
    p.drawEllipse(center, r, r);

    // 4. Precision center crosshair with center gap
    p.setPen(QPen(QColor(15, 17, 26, 240), 2.0));
    p.drawLine(QPointF(16.0, 11.0), QPointF(16.0, 13.5));
    p.drawLine(QPointF(16.0, 18.5), QPointF(16.0, 21.0));
    p.drawLine(QPointF(11.0, 16.0), QPointF(13.5, 16.0));
    p.drawLine(QPointF(18.5, 16.0), QPointF(21.0, 16.0));

    p.setPen(QPen(QColor("#ffffff"), 1.2));
    p.drawLine(QPointF(16.0, 11.0), QPointF(16.0, 13.5));
    p.drawLine(QPointF(16.0, 18.5), QPointF(16.0, 21.0));
    p.drawLine(QPointF(11.0, 16.0), QPointF(13.5, 16.0));
    p.drawLine(QPointF(18.5, 16.0), QPointF(21.0, 16.0));

    // Center precision point
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#7dcfff"));
    p.drawEllipse(center, 1.2, 1.2);

    // 5. Mini eraser badge at corner (23, 5)
    QRectF badgeRect(21.0, 3.0, 8.0, 6.0);
    p.setPen(QPen(QColor(15, 17, 26), 1.0));
    p.setBrush(QColor("#f7768e"));
    p.drawRoundedRect(badgeRect, 1.5, 1.5);
    p.setBrush(QColor("#c0caf5"));
    p.drawRect(QRectF(21.0, 3.0, 3.5, 6.0));

    return QCursor(pix, 16, 16);
}

QCursor CanvasCursorFactory::createLassoCursor() {
    QPixmap pix(32, 32);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 1. Outer dark halo for lasso loop
    p.setPen(QPen(QColor(15, 17, 26, 240), 2.5));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(14.0, 14.0), 9.0, 7.0);

    // 2. Bright dashed lasso rope
    p.setPen(QPen(QColor("#bb9af7"), 1.4, Qt::DashLine));
    p.drawEllipse(QPointF(14.0, 14.0), 9.0, 7.0);

    // 3. Lasso Knot & tail
    p.setPen(QPen(QColor(15, 17, 26), 2.0));
    p.drawLine(QPointF(7.0, 18.0), QPointF(4.0, 25.0));
    p.setPen(QPen(QColor("#ffffff"), 1.2));
    p.drawLine(QPointF(7.0, 18.0), QPointF(4.0, 25.0));

    // 4. Precision targeting crosshair at top-left hotspot (3, 3)
    p.setPen(QPen(QColor(15, 17, 26, 240), 2.2));
    p.drawLine(QPointF(3.0, 0.0), QPointF(3.0, 7.0));
    p.drawLine(QPointF(0.0, 3.0), QPointF(7.0, 3.0));

    p.setPen(QPen(QColor("#7dcfff"), 1.4));
    p.drawLine(QPointF(3.0, 0.0), QPointF(3.0, 7.0));
    p.drawLine(QPointF(0.0, 3.0), QPointF(7.0, 3.0));

    return QCursor(pix, 3, 3);
}

QCursor CanvasCursorFactory::createShapeCursor(ToolType tool) {
    QPixmap pix(32, 32);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QPointF center(16.0, 16.0);

    // 1. Precision Technical Drafting Crosshair (32x32)
    // Dark shadow background for contrast on white/bright PDFs
    p.setPen(QPen(QColor(15, 17, 26, 240), 2.4));
    p.drawLine(QPointF(16.0, 6.0), QPointF(16.0, 13.0));
    p.drawLine(QPointF(16.0, 19.0), QPointF(16.0, 26.0));
    p.drawLine(QPointF(6.0, 16.0), QPointF(13.0, 16.0));
    p.drawLine(QPointF(19.0, 16.0), QPointF(26.0, 16.0));

    // Crisp white precision lines
    p.setPen(QPen(QColor("#ffffff"), 1.2));
    p.drawLine(QPointF(16.0, 6.0), QPointF(16.0, 13.0));
    p.drawLine(QPointF(16.0, 19.0), QPointF(16.0, 26.0));
    p.drawLine(QPointF(6.0, 16.0), QPointF(13.0, 16.0));
    p.drawLine(QPointF(19.0, 16.0), QPointF(26.0, 16.0));

    // Center targeting pixel
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#7aa2f7"));
    p.drawEllipse(center, 1.2, 1.2);

    // 2. Shape Indicator Badge in the bottom-right quadrant
    p.setPen(QPen(QColor(15, 17, 26, 240), 2.0));
    p.setBrush(Qt::NoBrush);

    if (tool == ToolType::ShapeLine) {
        p.drawLine(QPointF(22.0, 28.0), QPointF(28.0, 22.0));
        p.setPen(QPen(QColor("#7dcfff"), 1.4));
        p.drawLine(QPointF(22.0, 28.0), QPointF(28.0, 22.0));
    } else if (tool == ToolType::ShapeRect) {
        p.drawRect(QRectF(21.0, 21.0, 8.0, 7.0));
        p.setPen(QPen(QColor("#7dcfff"), 1.2));
        p.drawRect(QRectF(21.0, 21.0, 8.0, 7.0));
    } else if (tool == ToolType::ShapeEllipse) {
        p.drawEllipse(QPointF(25.0, 25.0), 4.5, 4.5);
        p.setPen(QPen(QColor("#7dcfff"), 1.2));
        p.drawEllipse(QPointF(25.0, 25.0), 4.5, 4.5);
    } else if (tool == ToolType::ShapeAxes2D || tool == ToolType::ShapeAxes3D) {
        p.drawLine(QPointF(21.0, 29.0), QPointF(29.0, 29.0));
        p.drawLine(QPointF(21.0, 29.0), QPointF(21.0, 21.0));
        p.setPen(QPen(QColor("#9ece6a"), 1.2));
        p.drawLine(QPointF(21.0, 29.0), QPointF(29.0, 29.0));
        p.drawLine(QPointF(21.0, 29.0), QPointF(21.0, 21.0));
    }

    return QCursor(pix, 16, 16);
}

} // namespace OmniNote
