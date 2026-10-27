#include "SqliteStorage.hpp"
#include <QFileInfo>
#include <QDir>
#include <QDebug>

namespace OmniNote {

SqliteStorage::SqliteStorage() = default;

SqliteStorage::~SqliteStorage() {
    close();
}

bool SqliteStorage::open(const QString &filePath) {
    close();

    QFileInfo fi(filePath);
    QDir dir = fi.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    int rc = sqlite3_open(filePath.toUtf8().constData(), &m_db);
    if (rc != SQLITE_OK) {
        close();
        return false;
    }

    m_dbPath = filePath;

    // Enable WAL mode and foreign key constraints
    sqlite3_exec(m_db, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA synchronous = NORMAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

    return createTables();
}

void SqliteStorage::close() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
    m_dbPath.clear();
}

bool SqliteStorage::beginTransaction() {
    if (!m_db) return false;
    return sqlite3_exec(m_db, "BEGIN IMMEDIATE TRANSACTION;", nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool SqliteStorage::commitTransaction() {
    if (!m_db) return false;
    return sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool SqliteStorage::rollbackTransaction() {
    if (!m_db) return false;
    return sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool SqliteStorage::createTables() {
    if (!m_db) return false;

    const char *schemaSql =
        "CREATE TABLE IF NOT EXISTS Subjects ("
        "   id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "   name TEXT NOT NULL UNIQUE,"
        "   color TEXT DEFAULT '#7aa2f7',"
        "   created_at INTEGER NOT NULL"
        ");"

        "CREATE TABLE IF NOT EXISTS Notebooks ("
        "   id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "   subject_id INTEGER NOT NULL,"
        "   title TEXT NOT NULL,"
        "   created_at INTEGER NOT NULL,"
        "   updated_at INTEGER NOT NULL,"
        "   FOREIGN KEY(subject_id) REFERENCES Subjects(id) ON DELETE CASCADE"
        ");"

        "CREATE TABLE IF NOT EXISTS Pages ("
        "   id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "   notebook_id INTEGER NOT NULL,"
        "   page_number INTEGER NOT NULL,"
        "   width REAL NOT NULL,"
        "   height REAL NOT NULL,"
        "   background_type INTEGER NOT NULL,"
        "   pdf_page INTEGER DEFAULT -1,"
        "   pdf_path TEXT,"
        "   FOREIGN KEY(notebook_id) REFERENCES Notebooks(id) ON DELETE CASCADE"
        ");"

        "CREATE TABLE IF NOT EXISTS Strokes ("
        "   id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "   page_id INTEGER NOT NULL,"
        "   tool_type INTEGER NOT NULL,"
        "   color TEXT NOT NULL,"
        "   width REAL NOT NULL,"
        "   z_value REAL NOT NULL,"
        "   data BLOB NOT NULL,"
        "   FOREIGN KEY(page_id) REFERENCES Pages(id) ON DELETE CASCADE"
        ");"

        "CREATE TABLE IF NOT EXISTS StemBlocks ("
        "   id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "   page_id INTEGER NOT NULL,"
        "   block_type INTEGER NOT NULL,"
        "   x REAL NOT NULL,"
        "   y REAL NOT NULL,"
        "   w REAL NOT NULL,"
        "   h REAL NOT NULL,"
        "   data TEXT NOT NULL,"
        "   FOREIGN KEY(page_id) REFERENCES Pages(id) ON DELETE CASCADE"
        ");";

    char *errMsg = nullptr;
    int rc = sqlite3_exec(m_db, schemaSql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        if (errMsg) {
            sqlite3_free(errMsg);
        }
        return false;
    }

    return true;
}

QVector<SubjectRecord> SqliteStorage::getSubjects() {
    QVector<SubjectRecord> result;
    if (!m_db) return result;

    const char *sql = "SELECT id, name, color, created_at FROM Subjects ORDER BY id ASC;";
    sqlite3_stmt *stmt = nullptr;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            SubjectRecord s;
            s.id = sqlite3_column_int64(stmt, 0);
            s.name = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)));
            s.color = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2)));
            s.createdAt = sqlite3_column_int64(stmt, 3);
            result.append(s);
        }
        sqlite3_finalize(stmt);
    }

    return result;
}

qint64 SqliteStorage::addSubject(const QString &name, const QString &color) {
    if (!m_db) return -1;

    const char *sql = "INSERT INTO Subjects (name, color, created_at) VALUES (?, ?, ?);";
    sqlite3_stmt *stmt = nullptr;
    qint64 newId = -1;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        QByteArray nameBytes = name.toUtf8();
        QByteArray colorBytes = color.toUtf8();
        qint64 now = QDateTime::currentSecsSinceEpoch();

        sqlite3_bind_text(stmt, 1, nameBytes.constData(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, colorBytes.constData(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, now);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            newId = sqlite3_last_insert_rowid(m_db);
        }
        sqlite3_finalize(stmt);
    }

    return newId;
}

bool SqliteStorage::deleteSubject(qint64 subjectId) {
    if (!m_db) return false;
    const char *sql = "DELETE FROM Subjects WHERE id = ?;";
    sqlite3_stmt *stmt = nullptr;
    bool ok = false;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, subjectId);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    return ok;
}

QVector<NotebookRecord> SqliteStorage::getNotebooks(qint64 subjectId) {
    QVector<NotebookRecord> result;
    if (!m_db) return result;

    const char *sql = "SELECT id, subject_id, title, created_at, updated_at FROM Notebooks WHERE subject_id = ? ORDER BY id ASC;";
    sqlite3_stmt *stmt = nullptr;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, subjectId);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            NotebookRecord nb;
            nb.id = sqlite3_column_int64(stmt, 0);
            nb.subjectId = sqlite3_column_int64(stmt, 1);
            nb.title = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2)));
            nb.createdAt = sqlite3_column_int64(stmt, 3);
            nb.updatedAt = sqlite3_column_int64(stmt, 4);
            result.append(nb);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

qint64 SqliteStorage::addNotebook(qint64 subjectId, const QString &title) {
    if (!m_db) return -1;

    const char *sql = "INSERT INTO Notebooks (subject_id, title, created_at, updated_at) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *stmt = nullptr;
    qint64 newId = -1;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        QByteArray titleBytes = title.toUtf8();
        qint64 now = QDateTime::currentSecsSinceEpoch();

        sqlite3_bind_int64(stmt, 1, subjectId);
        sqlite3_bind_text(stmt, 2, titleBytes.constData(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, now);
        sqlite3_bind_int64(stmt, 4, now);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            newId = sqlite3_last_insert_rowid(m_db);
        }
        sqlite3_finalize(stmt);
    }
    return newId;
}

bool SqliteStorage::deleteNotebook(qint64 notebookId) {
    if (!m_db) return false;
    const char *sql = "DELETE FROM Notebooks WHERE id = ?;";
    sqlite3_stmt *stmt = nullptr;
    bool ok = false;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, notebookId);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    return ok;
}

QString SqliteStorage::getNotebookTitle(qint64 notebookId) {
    if (!m_db || notebookId <= 0) return QString();
    const char *sql = "SELECT title FROM Notebooks WHERE id = ?;";
    sqlite3_stmt *stmt = nullptr;
    QString title;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, notebookId);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *t = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
            if (t) title = QString::fromUtf8(t);
        }
        sqlite3_finalize(stmt);
    }
    return title;
}

QVector<PageRecord> SqliteStorage::getPages(qint64 notebookId) {
    QVector<PageRecord> result;
    if (!m_db) return result;

    const char *sql = "SELECT id, notebook_id, page_number, width, height, background_type, pdf_page, pdf_path FROM Pages WHERE notebook_id = ? ORDER BY page_number ASC;";
    sqlite3_stmt *stmt = nullptr;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, notebookId);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            PageRecord p;
            p.id = sqlite3_column_int64(stmt, 0);
            p.notebookId = sqlite3_column_int64(stmt, 1);
            p.pageNumber = sqlite3_column_int(stmt, 2);
            p.width = sqlite3_column_double(stmt, 3);
            p.height = sqlite3_column_double(stmt, 4);
            p.backgroundType = sqlite3_column_int(stmt, 5);
            p.pdfPage = sqlite3_column_int(stmt, 6);
            const char *pdfPathStr = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
            p.pdfPath = pdfPathStr ? QString::fromUtf8(pdfPathStr) : QString();
            result.append(p);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

qint64 SqliteStorage::addPage(qint64 notebookId, int pageNumber, qreal width, qreal height,
                              int bgType, int pdfPage, const QString &pdfPath) {
    if (!m_db) return -1;

    const char *sql = "INSERT INTO Pages (notebook_id, page_number, width, height, background_type, pdf_page, pdf_path) VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt = nullptr;
    qint64 newId = -1;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        QByteArray pathBytes = pdfPath.toUtf8();
        sqlite3_bind_int64(stmt, 1, notebookId);
        sqlite3_bind_int(stmt, 2, pageNumber);
        sqlite3_bind_double(stmt, 3, width);
        sqlite3_bind_double(stmt, 4, height);
        sqlite3_bind_int(stmt, 5, bgType);
        sqlite3_bind_int(stmt, 6, pdfPage);
        sqlite3_bind_text(stmt, 7, pathBytes.constData(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            newId = sqlite3_last_insert_rowid(m_db);
        }
        sqlite3_finalize(stmt);
    }
    return newId;
}

bool SqliteStorage::updatePageBackground(qint64 pageId, int bgType) {
    if (!m_db) return false;
    const char *sql = "UPDATE Pages SET background_type = ? WHERE id = ?;";
    sqlite3_stmt *stmt = nullptr;
    bool ok = false;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, bgType);
        sqlite3_bind_int64(stmt, 2, pageId);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    return ok;
}

bool SqliteStorage::deletePage(qint64 pageId) {
    if (!m_db) return false;

    // First find the notebook_id and page_number of this page
    qint64 nbId = -1;
    int pageNum = -1;
    const char *selectSql = "SELECT notebook_id, page_number FROM Pages WHERE id = ?;";
    sqlite3_stmt *selectStmt = nullptr;
    if (sqlite3_prepare_v2(m_db, selectSql, -1, &selectStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(selectStmt, 1, pageId);
        if (sqlite3_step(selectStmt) == SQLITE_ROW) {
            nbId = sqlite3_column_int64(selectStmt, 0);
            pageNum = sqlite3_column_int(selectStmt, 1);
        }
        sqlite3_finalize(selectStmt);
    }

    if (nbId <= 0 || pageNum <= 0) return false;

    if (!beginTransaction()) return false;

    // Explicitly delete strokes and blocks for this page to prevent any orphan records
    const char *delStrokes = "DELETE FROM Strokes WHERE page_id = ?;";
    sqlite3_stmt *delStrokesStmt = nullptr;
    if (sqlite3_prepare_v2(m_db, delStrokes, -1, &delStrokesStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(delStrokesStmt, 1, pageId);
        sqlite3_step(delStrokesStmt);
        sqlite3_finalize(delStrokesStmt);
    }

    const char *delBlocks = "DELETE FROM StemBlocks WHERE page_id = ?;";
    sqlite3_stmt *delBlocksStmt = nullptr;
    if (sqlite3_prepare_v2(m_db, delBlocks, -1, &delBlocksStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(delBlocksStmt, 1, pageId);
        sqlite3_step(delBlocksStmt);
        sqlite3_finalize(delBlocksStmt);
    }

    // Delete the page entry
    const char *delPageSql = "DELETE FROM Pages WHERE id = ?;";
    sqlite3_stmt *delPageStmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(m_db, delPageSql, -1, &delPageStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(delPageStmt, 1, pageId);
        ok = (sqlite3_step(delPageStmt) == SQLITE_DONE);
        sqlite3_finalize(delPageStmt);
    }

    if (!ok) {
        rollbackTransaction();
        return false;
    }

    // Renumber remaining pages so there are never gaps or duplicate numbers
    const char *renumberSql = "UPDATE Pages SET page_number = page_number - 1 WHERE notebook_id = ? AND page_number > ?;";
    sqlite3_stmt *renumberStmt = nullptr;
    if (sqlite3_prepare_v2(m_db, renumberSql, -1, &renumberStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(renumberStmt, 1, nbId);
        sqlite3_bind_int(renumberStmt, 2, pageNum);
        sqlite3_step(renumberStmt);
        sqlite3_finalize(renumberStmt);
    }

    return commitTransaction();
}

int SqliteStorage::getPageNumber(qint64 pageId) {
    if (!m_db || pageId <= 0) return 1;
    const char *sql = "SELECT page_number FROM Pages WHERE id = ?;";
    sqlite3_stmt *stmt = nullptr;
    int pageNum = 1;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, pageId);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            pageNum = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return pageNum;
}

bool SqliteStorage::savePageData(qint64 pageId, const QVector<StrokeRecord> &strokes, const QVector<StemBlockRecord> &blocks) {
    if (!m_db) return false;

    if (!beginTransaction()) return false;

    // Remove existing strokes and blocks for this page
    {
        const char *delStrokes = "DELETE FROM Strokes WHERE page_id = ?;";
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, delStrokes, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int64(stmt, 1, pageId);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        const char *delBlocks = "DELETE FROM StemBlocks WHERE page_id = ?;";
        if (sqlite3_prepare_v2(m_db, delBlocks, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int64(stmt, 1, pageId);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    // Insert strokes
    {
        const char *insStroke = "INSERT INTO Strokes (page_id, tool_type, color, width, z_value, data) VALUES (?, ?, ?, ?, ?, ?);";
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, insStroke, -1, &stmt, nullptr) == SQLITE_OK) {
            for (const auto &s : strokes) {
                QByteArray colorBytes = s.color.toUtf8();
                sqlite3_bind_int64(stmt, 1, pageId);
                sqlite3_bind_int(stmt, 2, s.toolType);
                sqlite3_bind_text(stmt, 3, colorBytes.constData(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_double(stmt, 4, s.width);
                sqlite3_bind_double(stmt, 5, s.zValue);
                sqlite3_bind_blob(stmt, 6, s.blobData.constData(), s.blobData.size(), SQLITE_TRANSIENT);

                sqlite3_step(stmt);
                sqlite3_reset(stmt);
            }
            sqlite3_finalize(stmt);
        }
    }

    // Insert STEM Blocks
    {
        const char *insBlock = "INSERT INTO StemBlocks (page_id, block_type, x, y, w, h, data) VALUES (?, ?, ?, ?, ?, ?, ?);";
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, insBlock, -1, &stmt, nullptr) == SQLITE_OK) {
            for (const auto &b : blocks) {
                QByteArray dataBytes = b.data.toUtf8();
                sqlite3_bind_int64(stmt, 1, pageId);
                sqlite3_bind_int(stmt, 2, b.blockType);
                sqlite3_bind_double(stmt, 3, b.x);
                sqlite3_bind_double(stmt, 4, b.y);
                sqlite3_bind_double(stmt, 5, b.w);
                sqlite3_bind_double(stmt, 6, b.h);
                sqlite3_bind_text(stmt, 7, dataBytes.constData(), -1, SQLITE_TRANSIENT);

                sqlite3_step(stmt);
                sqlite3_reset(stmt);
            }
            sqlite3_finalize(stmt);
        }
    }

    return commitTransaction();
}

bool SqliteStorage::loadPageData(qint64 pageId, QVector<StrokeRecord> &strokes, QVector<StemBlockRecord> &blocks) {
    strokes.clear();
    blocks.clear();
    if (!m_db) return false;

    // Read strokes
    {
        const char *sql = "SELECT id, tool_type, color, width, z_value, data FROM Strokes WHERE page_id = ? ORDER BY id ASC;";
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int64(stmt, 1, pageId);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                StrokeRecord s;
                s.id = sqlite3_column_int64(stmt, 0);
                s.pageId = pageId;
                s.toolType = sqlite3_column_int(stmt, 1);
                s.color = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2)));
                s.width = sqlite3_column_double(stmt, 3);
                s.zValue = sqlite3_column_double(stmt, 4);

                const void *blob = sqlite3_column_blob(stmt, 5);
                int bytes = sqlite3_column_bytes(stmt, 5);
                s.blobData = QByteArray(reinterpret_cast<const char *>(blob), bytes);

                strokes.append(s);
            }
            sqlite3_finalize(stmt);
        }
    }

    // Read blocks
    {
        const char *sql = "SELECT id, block_type, x, y, w, h, data FROM StemBlocks WHERE page_id = ? ORDER BY id ASC;";
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int64(stmt, 1, pageId);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                StemBlockRecord b;
                b.id = sqlite3_column_int64(stmt, 0);
                b.pageId = pageId;
                b.blockType = sqlite3_column_int(stmt, 1);
                b.x = sqlite3_column_double(stmt, 2);
                b.y = sqlite3_column_double(stmt, 3);
                b.w = sqlite3_column_double(stmt, 4);
                b.h = sqlite3_column_double(stmt, 5);
                b.data = QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6)));

                blocks.append(b);
            }
            sqlite3_finalize(stmt);
        }
    }

    return true;
}

} // namespace OmniNote
