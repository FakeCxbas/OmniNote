#pragma once

#include <QString>
#include <QImage>
#include <QSizeF>
#include <QVector>
#include <memory>
#include <poppler-qt6.h>
#include "../canvas/VectorCanvasScene.hpp"

namespace OmniNote {

class PdfManager {
public:
    PdfManager();
    ~PdfManager();

    bool loadDocument(const QString &filePath);
    void closeDocument();

    bool isLoaded() const { return m_document != nullptr; }
    QString filePath() const { return m_filePath; }
    int pageCount() const;

    QSizeF pageSize(int pageIndex) const;
    QImage renderPage(int pageIndex, double dpi = 150.0);

    // High-resolution vector PDF notebook export using QPdfWriter
    static bool exportNotebookToPdf(const QString &outputPath,
                                   const QVector<VectorCanvasScene *> &scenes,
                                   int dpi = 300);

private:
    QString m_filePath;
    std::unique_ptr<Poppler::Document> m_document;
};

} // namespace OmniNote
