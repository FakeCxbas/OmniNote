#pragma once

#include <QString>
#include <QVector>
#include <QByteArray>
#include <QDateTime>
#include <sqlite3.h>

namespace OmniNote {

struct SubjectRecord {
    qint64 id{0};
    QString name;
    QString color;
    qint64 createdAt{0};
};

struct NotebookRecord {
    qint64 id{0};
    qint64 subjectId{0};
    QString title;
    qint64 createdAt{0};
    qint64 updatedAt{0};
};

struct PageRecord {
    qint64 id{0};
    qint64 notebookId{0};
    int pageNumber{1};
    qreal width{1240.0};
    qreal height{1754.0};
    int backgroundType{0};
    int pdfPage{-1};
    QString pdfPath;
};

struct StrokeRecord {
    qint64 id{0};
    qint64 pageId{0};
    int toolType{0};
    QString color;
    qreal width{2.5};
    qreal zValue{10.0};
    QByteArray blobData;
};

struct StemBlockRecord {
    qint64 id{0};
    qint64 pageId{0};
    int blockType{0}; // 0 = CodeBlock, 1 = FunctionPlot
    qreal x{0.0};
    qreal y{0.0};
    qreal w{400.0};
    qreal h{240.0};
    QString data; // JSON string payload
};

class SqliteStorage {
public:
    SqliteStorage();
    ~SqliteStorage();

    bool open(const QString &filePath);
    void close();
    bool isOpen() const { return m_db != nullptr; }
    QString currentFilePath() const { return m_dbPath; }

    // ACID Transactions
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

    // Subjects
    QVector<SubjectRecord> getSubjects();
    qint64 addSubject(const QString &name, const QString &color = "#7aa2f7");
    bool deleteSubject(qint64 subjectId);

    // Notebooks
    QVector<NotebookRecord> getNotebooks(qint64 subjectId);
    qint64 addNotebook(qint64 subjectId, const QString &title);
    bool deleteNotebook(qint64 notebookId);

    // Pages
    QVector<PageRecord> getPages(qint64 notebookId);
    qint64 addPage(qint64 notebookId, int pageNumber, qreal width = 1240.0, qreal height = 1754.0,
                   int bgType = 0, int pdfPage = -1, const QString &pdfPath = "");
    bool updatePageBackground(qint64 pageId, int bgType);
    bool deletePage(qint64 pageId);

    // Page Content (Strokes and STEM blocks)
    bool savePageData(qint64 pageId, const QVector<StrokeRecord> &strokes, const QVector<StemBlockRecord> &blocks);
    bool loadPageData(qint64 pageId, QVector<StrokeRecord> &strokes, QVector<StemBlockRecord> &blocks);

private:
    bool createTables();

    sqlite3 *m_db{nullptr};
    QString m_dbPath;
};

} // namespace OmniNote
