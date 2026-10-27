#pragma once

#include <QIcon>
#include <QPixmap>
#include <QColor>
#include <QString>

namespace OmniNote {

class AppIcons {
public:
    enum class IconType {
        Panel,
        Undo,
        Redo,
        Pen,
        Highlighter,
        Eraser,
        Lasso,
        Shapes,
        StraightLine,
        Axes2D,
        Axes3D,
        RectShape,
        CircleShape,
        Ruler,
        Sparkles,
        Text,
        Code,
        Plot,
        Image,
        Formula,
        Palette,
        ImportPdf,
        ExportPdf,
        Save,
        Duplicate,
        Trash,
        Close,
        Check,
        Folder,
        Notebook,
        Page,
        Plus,
        Restore,
        Edit,
        Copy,
        Cloud,
        Help
    };

    static QIcon get(IconType type, const QColor &color = QColor("#c0caf5"), int size = 20);
    static QPixmap getPixmap(IconType type, const QColor &color = QColor("#c0caf5"), int size = 20);
    static QString getSvgXml(IconType type, const QString &colorHex);
    static QString getSolarSvgName(IconType type);
    static QString getIconFilename(IconType type);
};

} // namespace OmniNote
