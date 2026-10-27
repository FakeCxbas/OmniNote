#include "CodeBlockItem.hpp"
#include "../app/AppIcons.hpp"
#include <QGuiApplication>
#include <QClipboard>
#include <QScrollBar>
#include <QJsonDocument>
#include <QJsonObject>
#include <QGraphicsScene>
#include <QTimer>

namespace OmniNote {

// ==================== LineNumberArea ====================

LineNumberArea::LineNumberArea(CodeEditorWidget *editor) : QWidget(editor), m_editor(editor) {}

QSize LineNumberArea::sizeHint() const {
    return QSize(m_editor->lineNumberAreaWidth(), 0);
}

void LineNumberArea::paintEvent(QPaintEvent *event) {
    m_editor->lineNumberAreaPaintEvent(event);
}

// ==================== CodeEditorWidget ====================

CodeEditorWidget::CodeEditorWidget(QWidget *parent) : QPlainTextEdit(parent) {
    m_lineNumberArea = new LineNumberArea(this);

    connect(this, &CodeEditorWidget::blockCountChanged, this, &CodeEditorWidget::updateLineNumberAreaWidth);
    connect(this, &CodeEditorWidget::updateRequest, this, &CodeEditorWidget::updateLineNumberArea);
    connect(this, &CodeEditorWidget::cursorPositionChanged, this, &CodeEditorWidget::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();

    QFont font("JetBrains Mono");
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(10);
    setFont(font);

    setStyleSheet(
        "QPlainTextEdit {"
        "   background-color: #1a1b26;"
        "   color: #c0caf5;"
        "   border: none;"
        "   selection-background-color: #364a82;"
        "   selection-color: #c0caf5;"
        "}"
    );

    setTabStopDistance(fontMetrics().horizontalAdvance(' ') * 4);
}

int CodeEditorWidget::lineNumberAreaWidth() {
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    int space = 12 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void CodeEditorWidget::updateLineNumberAreaWidth(int /* newBlockCount */) {
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditorWidget::updateLineNumberArea(const QRect &rect, int dy) {
    if (dy) {
        m_lineNumberArea->scroll(0, dy);
    } else {
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
    }

    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth(0);
    }
}

void CodeEditorWidget::resizeEvent(QResizeEvent *e) {
    QPlainTextEdit::resizeEvent(e);
    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditorWidget::highlightCurrentLine() {
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        QColor lineColor = QColor("#24283b");
        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    setExtraSelections(extraSelections);
}

void CodeEditorWidget::lineNumberAreaPaintEvent(QPaintEvent *event) {
    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), QColor("#16161e"));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(QColor("#565f89"));
            painter.setFont(font());
            painter.drawText(0, top, m_lineNumberArea->width() - 6, fontMetrics().height(),
                             Qt::AlignRight | Qt::AlignVCenter, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

// ==================== CodeBlockContainerWidget ====================

CodeBlockContainerWidget::CodeBlockContainerWidget(QWidget *parent) : QWidget(parent) {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Header bar
    auto *headerWidget = new QWidget(this);
    headerWidget->setObjectName("headerWidget");
    headerWidget->setStyleSheet(
        "QWidget#headerWidget {"
        "   background-color: #24283b;"
        "   border-top-left-radius: 6px;"
        "   border-top-right-radius: 6px;"
        "   border-bottom: 1px solid #3b4261;"
        "}"
    );

    auto *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(8, 4, 8, 4);
    headerLayout->setSpacing(6);

    auto *iconLabel = new QLabel(headerWidget);
    iconLabel->setPixmap(AppIcons::getPixmap(AppIcons::IconType::Code, QColor("#7aa2f7"), 14));
    iconLabel->setStyleSheet("background: transparent;");

    m_languageCombo = new QComboBox(headerWidget);
    m_languageCombo->addItem("C++20", static_cast<int>(CppSyntaxHighlighter::Language::Cpp));
    m_languageCombo->addItem("Python", static_cast<int>(CppSyntaxHighlighter::Language::Python));
    m_languageCombo->addItem("Pseudocódigo", static_cast<int>(CppSyntaxHighlighter::Language::Pseudocode));
    m_languageCombo->setStyleSheet(
        "QComboBox {"
        "   background-color: #1f2335;"
        "   color: #c0caf5;"
        "   border: 1px solid #3b4261;"
        "   border-radius: 4px;"
        "   padding: 2px 8px;"
        "   font-size: 9pt;"
        "}"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView {"
        "   background-color: #1f2335;"
        "   color: #c0caf5;"
        "   selection-background-color: #3b4261;"
        "}"
    );

    m_copyButton = new QPushButton("Copiar", headerWidget);
    m_copyButton->setIcon(AppIcons::get(AppIcons::IconType::Copy, QColor("#c0caf5"), 13));
    m_copyButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #3b4261;"
        "   color: #c0caf5;"
        "   border: none;"
        "   border-radius: 4px;"
        "   padding: 2px 8px;"
        "   font-size: 9pt;"
        "}"
        "QPushButton:hover { background-color: #7aa2f7; color: #15161e; }"
    );

    m_closeButton = new QPushButton(headerWidget);
    m_closeButton->setIcon(AppIcons::get(AppIcons::IconType::Close, QColor("#f7768e"), 12));
    m_closeButton->setFixedSize(20, 20);
    m_closeButton->setStyleSheet(
        "QPushButton {"
        "   background-color: transparent;"
        "   border: none;"
        "   border-radius: 4px;"
        "}"
        "QPushButton:hover { background-color: rgba(247, 118, 142, 40); border-radius: 4px; }"
    );

    headerLayout->addWidget(iconLabel);
    headerLayout->addWidget(m_languageCombo);
    headerLayout->addStretch();
    headerLayout->addWidget(m_copyButton);
    headerLayout->addWidget(m_closeButton);

    mainLayout->addWidget(headerWidget);

    // Editor Area
    m_editor = new CodeEditorWidget(this);
    m_highlighter = new CppSyntaxHighlighter(m_editor->document(), CppSyntaxHighlighter::Language::Cpp);
    mainLayout->addWidget(m_editor);

    resize(420, 240);

    connect(m_languageCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        auto lang = static_cast<CppSyntaxHighlighter::Language>(m_languageCombo->itemData(index).toInt());
        m_highlighter->setLanguage(lang);
    });

    connect(m_copyButton, &QPushButton::clicked, this, [this]() {
        QClipboard *clipboard = QGuiApplication::clipboard();
        if (clipboard) {
            clipboard->setText(m_editor->toPlainText());
            m_copyButton->setText("¡Copiado!");
            QTimer::singleShot(1500, this, [this]() {
                m_copyButton->setText("Copiar");
            });
        }
    });

    connect(m_closeButton, &QPushButton::clicked, this, &CodeBlockContainerWidget::deleteRequested);
    connect(m_editor, &QPlainTextEdit::textChanged, this, &CodeBlockContainerWidget::codeChanged);
}

QString CodeBlockContainerWidget::code() const {
    return m_editor->toPlainText();
}

void CodeBlockContainerWidget::setCode(const QString &code) {
    m_editor->setPlainText(code);
}

CppSyntaxHighlighter::Language CodeBlockContainerWidget::language() const {
    return static_cast<CppSyntaxHighlighter::Language>(m_languageCombo->currentData().toInt());
}

void CodeBlockContainerWidget::setLanguage(CppSyntaxHighlighter::Language lang) {
    int index = m_languageCombo->findData(static_cast<int>(lang));
    if (index >= 0) {
        m_languageCombo->setCurrentIndex(index);
    }
    m_highlighter->setLanguage(lang);
}

void CodeBlockContainerWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (event->pos().x() >= width() - 18 && event->pos().y() >= height() - 18) {
            m_resizing = true;
            m_dragStartPos = event->globalPosition().toPoint();
            m_dragStartSize = size();
            event->accept();
            return;
        }
        if (event->pos().y() <= 32) {
            m_draggingHeader = true;
            m_dragStartPos = event->globalPosition().toPoint();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void CodeBlockContainerWidget::mouseMoveEvent(QMouseEvent *event) {
    if (m_resizing) {
        QPoint delta = event->globalPosition().toPoint() - m_dragStartPos;
        int newW = qMax(260, m_dragStartSize.width() + delta.x());
        int newH = qMax(130, m_dragStartSize.height() + delta.y());
        resize(newW, newH);
        emit codeChanged();
        event->accept();
        return;
    }

    if (m_draggingHeader) {
        QPoint delta = event->globalPosition().toPoint() - m_dragStartPos;
        m_dragStartPos = event->globalPosition().toPoint();
        emit moveDeltaRequested(QPointF(delta.x(), delta.y()));
        event->accept();
        return;
    }

    if (event->pos().x() >= width() - 18 && event->pos().y() >= height() - 18) {
        setCursor(Qt::SizeFDiagCursor);
    } else if (event->pos().y() <= 32) {
        setCursor(Qt::OpenHandCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
    QWidget::mouseMoveEvent(event);
}

void CodeBlockContainerWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (m_resizing) {
        m_resizing = false;
        event->accept();
        return;
    }
    if (m_draggingHeader) {
        m_draggingHeader = false;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void CodeBlockContainerWidget::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
    QPainter p(this);
    // Draw subtle border around entire container
    p.setPen(QColor("#3b4261"));
    p.drawRect(0, 0, width() - 1, height() - 1);

    // Draw resize grip in bottom right
    p.setPen(QColor("#7aa2f7"));
    int w = width();
    int h = height();
    p.drawLine(w - 12, h - 4, w - 4, h - 12);
    p.drawLine(w - 8, h - 4, w - 4, h - 8);
    p.drawLine(w - 4, h - 4, w - 4, h - 4);
}

// ==================== CodeBlockItem ====================

CodeBlockItem::CodeBlockItem(const QPointF &pos, QGraphicsItem *parent)
    : QGraphicsProxyWidget(parent) {
    m_containerWidget = new CodeBlockContainerWidget();
    setWidget(m_containerWidget);
    setPos(pos);

    setFlag(QGraphicsItem::ItemIsMovable, false);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);

    connect(m_containerWidget, &CodeBlockContainerWidget::moveDeltaRequested, this, [this](const QPointF &delta) {
        setPos(this->pos() + delta);
        emit blockModified();
    });

    connect(m_containerWidget, &CodeBlockContainerWidget::codeChanged, this, &CodeBlockItem::blockModified);

    connect(m_containerWidget, &CodeBlockContainerWidget::deleteRequested, this, [this]() {
        emit itemDeleted(this);
        if (scene()) {
            scene()->removeItem(this);
        }
        deleteLater();
    });
}

QString CodeBlockItem::code() const {
    return m_containerWidget->code();
}

void CodeBlockItem::setCode(const QString &code) {
    m_containerWidget->setCode(code);
}

CppSyntaxHighlighter::Language CodeBlockItem::language() const {
    return m_containerWidget->language();
}

void CodeBlockItem::setLanguage(CppSyntaxHighlighter::Language lang) {
    m_containerWidget->setLanguage(lang);
}

QString CodeBlockItem::serialize() const {
    QJsonObject obj;
    obj["x"] = pos().x();
    obj["y"] = pos().y();
    obj["width"] = m_containerWidget->width();
    obj["height"] = m_containerWidget->height();
    obj["language"] = static_cast<int>(language());
    obj["code"] = code();

    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void CodeBlockItem::deserialize(const QString &data) {
    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
    if (!doc.isObject()) return;
    QJsonObject obj = doc.object();

    if (obj.contains("x") && obj.contains("y")) {
        setPos(obj["x"].toDouble(), obj["y"].toDouble());
    }
    if (obj.contains("width") && obj.contains("height")) {
        m_containerWidget->resize(obj["width"].toInt(), obj["height"].toInt());
    }
    if (obj.contains("language")) {
        setLanguage(static_cast<CppSyntaxHighlighter::Language>(obj["language"].toInt()));
    }
    if (obj.contains("code")) {
        setCode(obj["code"].toString());
    }
}

} // namespace OmniNote
