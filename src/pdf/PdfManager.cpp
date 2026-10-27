#include "PdfManager.hpp"
#include <QPdfWriter>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QFileInfo>
#include <QDebug>

namespace OmniNote {

PdfManager::PdfManager() = default;

PdfManager::~PdfManager() {
    closeDocument();
}

bool PdfManager::loadDocument(const QString &filePath) {
    closeDocument();

    m_document = Poppler::Document::load(filePath);
    if (!m_document || m_document->isLocked()) {
        m_document.reset();
        return false;
    }

    m_document->setRenderHint(Poppler::Document::Antialiasing, true);
    m_document->setRenderHint(Poppler::Document::TextAntialiasing, true);
    m_document->setRenderHint(Poppler::Document::ThinLineSolid, true);

    m_filePath = filePath;
    return true;
}

void PdfManager::closeDocument() {
    m_document.reset();
    m_filePath.clear();
}

int PdfManager::pageCount() const {
    if (!m_document) return 0;
    return m_document->numPages();
}

QSizeF PdfManager::pageSize(int pageIndex) const {
    if (!m_document || pageIndex < 0 || pageIndex >= m_document->numPages()) {
        return QSizeF(0, 0);
    }

    std::unique_ptr<Poppler::Page> page(m_document->page(pageIndex));
    if (!page) return QSizeF(0, 0);

    return page->pageSize();
}

QImage PdfManager::renderPage(int pageIndex, double dpi) {
    if (!m_document || pageIndex < 0 || pageIndex >= m_document->numPages()) {
        return QImage();
    }

    std::unique_ptr<Poppler::Page> page(m_document->page(pageIndex));
    if (!page) return QImage();

    double clampedDpi = qBound(72.0, dpi, 400.0);
    return page->renderToImage(clampedDpi, clampedDpi);
}

bool PdfManager::exportNotebookToPdf(const QString &outputPath,
                                     const QVector<VectorCanvasScene *> &scenes,
                                     int dpi) {
    if (scenes.isEmpty() || outputPath.isEmpty()) {
        return false;
    }

    QPdfWriter pdfWriter(outputPath);
    pdfWriter.setResolution(dpi);
    pdfWriter.setPageSize(QPageSize(QPageSize::A4));
    pdfWriter.setPageMargins(QMarginsF(0, 0, 0, 0));

    QPainter painter(&pdfWriter);
    if (!painter.isActive()) {
        return false;
    }

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const int targetW = pdfWriter.width();
    const int targetH = pdfWriter.height();
    const QRectF targetRect(0, 0, targetW, targetH);

    for (int i = 0; i < scenes.size(); ++i) {
        if (i > 0) {
            pdfWriter.newPage();
        }

        VectorCanvasScene *scene = scenes[i];
        if (!scene) continue;

        QRectF sourceRect = scene->sceneRect();
        scene->render(&painter, targetRect, sourceRect);
    }

    painter.end();
    return true;
}

} // namespace OmniNote
