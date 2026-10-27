#include "LatexRenderer.hpp"
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QByteArray>
#include <QTextDocument>
#include <QPainter>
#include <QRegularExpression>
#include <QDebug>

namespace OmniNote {

LatexRenderer &LatexRenderer::instance() {
    static LatexRenderer s_instance;
    return s_instance;
}

LatexRenderer::LatexRenderer() {
    // Search candidates for render_latex.py
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/render_latex.py",
        QCoreApplication::applicationDirPath() + "/../OmniNoteSTEM/src/stem/render_latex.py",
        QDir::currentPath() + "/OmniNoteSTEM/src/stem/render_latex.py",
        "/home/fakecxbas/Proyectos/Apuntes/OmniNoteSTEM/src/stem/render_latex.py"
    };

    for (const auto &path : candidates) {
        if (QFileInfo::exists(path)) {
            m_scriptPath = QFileInfo(path).canonicalFilePath();
            break;
        }
    }

    ensureWorkerStarted();
}

LatexRenderer::~LatexRenderer() {
    if (m_worker && m_worker->state() != QProcess::NotRunning) {
        m_worker->terminate();
        if (!m_worker->waitForFinished(1000)) {
            m_worker->kill();
        }
    }
}

bool LatexRenderer::isWorkerHealthy() {
    return m_worker && m_worker->state() == QProcess::Running;
}

bool LatexRenderer::ensureWorkerStarted() {
    if (isWorkerHealthy()) return true;
    if (m_scriptPath.isEmpty() || !QFileInfo::exists(m_scriptPath)) return false;

    if (!m_worker) {
        m_worker = new QProcess(this);
    } else {
        m_worker->kill();
        m_worker->waitForFinished(500);
    }

    m_worker->setProgram("python3");
    m_worker->setArguments({"-u", m_scriptPath});
    m_worker->start();

    return m_worker->waitForStarted(2000);
}

QPixmap LatexRenderer::render(const QString &latex, const QColor &color, int dpi) {
    if (latex.trimmed().isEmpty()) {
        return QPixmap();
    }

    QString cacheKey = QString("%1_%2_%3").arg(color.name()).arg(dpi).arg(latex.trimmed());
    if (m_cache.contains(cacheKey)) {
        return m_cache.value(cacheKey);
    }

    if (ensureWorkerStarted()) {
        QJsonObject req;
        req["latex"] = latex.trimmed();
        req["color"] = color.name();
        req["dpi"] = dpi;

        QByteArray reqData = QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n";
        m_worker->write(reqData);
        if (m_worker->waitForBytesWritten(1000) && m_worker->waitForReadyRead(2500)) {
            QByteArray responseLine = m_worker->readLine();
            QJsonDocument respDoc = QJsonDocument::fromJson(responseLine);
            if (!respDoc.isNull() && respDoc.isObject()) {
                QJsonObject respObj = respDoc.object();
                if (respObj["status"].toString() == "ok") {
                    QString b64 = respObj["png"].toString();
                    QByteArray pngBytes = QByteArray::fromBase64(b64.toLatin1());
                    QPixmap pixmap;
                    if (pixmap.loadFromData(pngBytes, "PNG")) {
                        // Limit cache size to 150 items
                        if (m_cache.size() > 150) {
                            m_cache.clear();
                        }
                        m_cache.insert(cacheKey, pixmap);
                        return pixmap;
                    }
                }
            }
        }
    }

    // Graceful fallback to rich text rasterization
    QPixmap fallback = renderFallback(latex, color);
    if (!fallback.isNull()) {
        m_cache.insert(cacheKey, fallback);
    }
    return fallback;
}

QPixmap LatexRenderer::renderFallback(const QString &latex, const QColor &color) {
    QTextDocument doc;
    doc.setDefaultFont(QFont("STIX Two Math", 16));
    doc.setHtml(convertLatexToHtml(latex));

    QSize size = doc.size().toSize();
    if (size.width() <= 0 || size.height() <= 0) {
        size = QSize(200, 60);
    }

    QPixmap pixmap(size * 2); // 2x DPI
    pixmap.setDevicePixelRatio(2.0);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    doc.drawContents(&painter);

    return pixmap;
}

QString LatexRenderer::convertLatexToHtml(const QString &latex) {
    QString s = latex.trimmed();
    if (s.startsWith("$$") && s.endsWith("$$") && s.length() >= 4) {
        s = s.mid(2, s.length() - 4).trimmed();
    } else if (s.startsWith("$") && s.endsWith("$") && s.length() >= 2) {
        s = s.mid(1, s.length() - 2).trimmed();
    }

    s.replace("&", "&amp;");
    s.replace("<", "&lt;");
    s.replace(">", "&gt;");

    // Math functions
    s.replace(QRegularExpression(R"(\\sin(?![a-zA-Z]))"), "<b>sin</b>");
    s.replace(QRegularExpression(R"(\\cos(?![a-zA-Z]))"), "<b>cos</b>");
    s.replace(QRegularExpression(R"(\\tan(?![a-zA-Z]))"), "<b>tan</b>");
    s.replace(QRegularExpression(R"(\\ln(?![a-zA-Z]))"), "<b>ln</b>");
    s.replace(QRegularExpression(R"(\\log(?![a-zA-Z]))"), "<b>log</b>");
    s.replace(QRegularExpression(R"(\\exp(?![a-zA-Z]))"), "<b>exp</b>");
    s.replace(QRegularExpression(R"(\\lim(?![a-zA-Z]))"), "<b>lim</b>");
    s.replace(QRegularExpression(R"(\\det(?![a-zA-Z]))"), "<b>det</b>");

    // Greek letters
    s.replace(QRegularExpression(R"(\\alpha(?![a-zA-Z]))"), "&#945;");
    s.replace(QRegularExpression(R"(\\beta(?![a-zA-Z]))"), "&#946;");
    s.replace(QRegularExpression(R"(\\gamma(?![a-zA-Z]))"), "&#947;");
    s.replace(QRegularExpression(R"(\\delta(?![a-zA-Z]))"), "&#948;");
    s.replace(QRegularExpression(R"(\\epsilon(?![a-zA-Z]))"), "&#949;");
    s.replace(QRegularExpression(R"(\\theta(?![a-zA-Z]))"), "&#952;");
    s.replace(QRegularExpression(R"(\\lambda(?![a-zA-Z]))"), "&#955;");
    s.replace(QRegularExpression(R"(\\mu(?![a-zA-Z]))"), "&#956;");
    s.replace(QRegularExpression(R"(\\pi(?![a-zA-Z]))"), "&#960;");
    s.replace(QRegularExpression(R"(\\sigma(?![a-zA-Z]))"), "&#963;");
    s.replace(QRegularExpression(R"(\\omega(?![a-zA-Z]))"), "&#969;");
    s.replace(QRegularExpression(R"(\\Delta(?![a-zA-Z]))"), "&#916;");
    s.replace(QRegularExpression(R"(\\Theta(?![a-zA-Z]))"), "&#920;");
    s.replace(QRegularExpression(R"(\\Sigma(?![a-zA-Z]))"), "&#931;");

    // Operators: using lookahead (?![a-zA-Z]) so \int_ or \sum_ match correctly!
    s.replace(QRegularExpression(R"(\\iint(?![a-zA-Z]))"), "&#8748;");
    s.replace(QRegularExpression(R"(\\iiint(?![a-zA-Z]))"), "&#8749;");
    s.replace(QRegularExpression(R"(\\oint(?![a-zA-Z]))"), "&#8750;");
    s.replace(QRegularExpression(R"(\\int(?![a-zA-Z]))"), "<span style='font-size:130%; color:#7dcfff;'>&#8747;</span>");
    s.replace(QRegularExpression(R"(\\sum(?![a-zA-Z]))"), "<span style='font-size:130%; color:#7dcfff;'>&#8721;</span>");
    s.replace(QRegularExpression(R"(\\prod(?![a-zA-Z]))"), "<span style='font-size:130%; color:#7dcfff;'>&#8719;</span>");
    s.replace(QRegularExpression(R"(\\infty(?![a-zA-Z]))"), "&#8734;");
    s.replace(QRegularExpression(R"(\\nabla(?![a-zA-Z]))"), "<span style='color:#bb9af7;'>&#8711;</span>");
    s.replace(QRegularExpression(R"(\\partial(?![a-zA-Z]))"), "<span style='color:#7dcfff;'>&#8706;</span>");
    s.replace(QRegularExpression(R"(\\pm(?![a-zA-Z]))"), "&#177;");
    s.replace(QRegularExpression(R"(\\cdot(?![a-zA-Z]))"), "&#183;");
    s.replace(QRegularExpression(R"(\\times(?![a-zA-Z]))"), "&#215;");
    s.replace(QRegularExpression(R"(\\neq(?![a-zA-Z]))"), "&#8800;");
    s.replace(QRegularExpression(R"(\\leq(?![a-zA-Z])|\\le(?![a-zA-Z]))"), "&#8804;");
    s.replace(QRegularExpression(R"(\\geq(?![a-zA-Z])|\\ge(?![a-zA-Z]))"), "&#8805;");
    s.replace(QRegularExpression(R"(\\to(?![a-zA-Z])|\\rightarrow(?![a-zA-Z]))"), "&#8594;");
    s.replace(QRegularExpression(R"(\\Rightarrow(?![a-zA-Z]))"), "&#8658;");

    // Vectors
    s.replace(QRegularExpression(R"(\\vec\{([^}]+)\})"), "<b>\\1&#8407;</b>");

    // Square roots: \sqrt{...}
    s.replace(QRegularExpression(R"(\\sqrt\{([^}]+)\})"), "&radic;<span style='text-decoration:overline;'>&nbsp;\\1&nbsp;</span>");

    // Simple fractions: \frac{a}{b}
    s.replace(QRegularExpression(R"(\\frac\{([^}]+)\}\{([^}]+)\})"), "(\\1 / \\2)");

    // Subscripts & Superscripts
    s.replace(QRegularExpression(R"(\^\{([^}]+)\})"), "<sup><span style='font-size:75%; color:#bb9af7;'>\\1</span></sup>");
    s.replace(QRegularExpression(R"(\^([0-9a-zA-Z]))"), "<sup><span style='font-size:75%; color:#bb9af7;'>\\1</span></sup>");
    s.replace(QRegularExpression(R"(_\{([^}]+)\})"), "<sub><span style='font-size:75%; color:#7dcfff;'>\\1</span></sub>");
    s.replace(QRegularExpression(R"(_([0-9a-zA-Z]))"), "<sub><span style='font-size:75%; color:#7dcfff;'>\\1</span></sub>");

    return QString(
        "<html><head><style>"
        "body { color: #f0f3ff; font-family: 'STIX Two Math', 'DejaVu Serif', serif; font-size: 18px; }"
        "</style></head><body align='center'><div>%1</div></body></html>"
    ).arg(s);
}

} // namespace OmniNote
