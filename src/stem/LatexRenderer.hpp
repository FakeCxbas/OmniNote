#pragma once

#include <QObject>
#include <QPixmap>
#include <QColor>
#include <QProcess>
#include <QHash>

namespace OmniNote {

class LatexRenderer : public QObject {
    Q_OBJECT

public:
    static LatexRenderer &instance();

    // Renders LaTeX formula to a high-quality transparent QPixmap.
    // Uses the local mathtext rendering engine with automatic caching and graceful fallback.
    QPixmap render(const QString &latex, const QColor &color = QColor("#f0f3ff"), int dpi = 220);

    // Fallback HTML string generator for rich text rendering
    static QString convertLatexToHtml(const QString &latex);

    bool isWorkerHealthy();

private:
    LatexRenderer();
    ~LatexRenderer() override;

    bool ensureWorkerStarted();
    QPixmap renderFallback(const QString &latex, const QColor &color);

    QProcess *m_worker{nullptr};
    QString m_scriptPath;
    QHash<QString, QPixmap> m_cache;
};

} // namespace OmniNote
