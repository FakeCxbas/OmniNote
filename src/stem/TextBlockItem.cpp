#include "TextBlockItem.hpp"
#include "../app/AppIcons.hpp"
#include <QColorDialog>
#include <QMenu>
#include <QAction>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QCursor>
#include <QTextBlock>
#include <QTextDocument>

namespace OmniNote {

// ============================================================================
// TextEditorWidget
// ============================================================================

TextEditorWidget::TextEditorWidget(QWidget *parent) : QTextEdit(parent) {
    setFrameShape(QFrame::NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setLineWrapMode(QTextEdit::WidgetWidth);
    setAcceptRichText(false);
    setPlaceholderText("Escribe un título o texto aquí...");
    setStyleSheet("QTextEdit { background: transparent; border: none; padding: 0px 4px; }");

    connect(this, &QTextEdit::textChanged, this, &TextEditorWidget::contentChanged);
}

void TextEditorWidget::focusInEvent(QFocusEvent *event) {
    QTextEdit::focusInEvent(event);
    emit focusChanged(true);
}

void TextEditorWidget::focusOutEvent(QFocusEvent *event) {
    QTextEdit::focusOutEvent(event);
    emit focusChanged(false);
}

// ============================================================================
// TextBlockContainerWidget
// ============================================================================

TextBlockContainerWidget::TextBlockContainerWidget(QWidget *parent) : QWidget(parent) {
    setMinimumSize(160, 36);
    resize(380, 75);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // Pure zero-margin layout: editor is permanently locked at (0, 0)
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_editor = new TextEditorWidget(this);
    m_editor->installEventFilter(this);
    mainLayout->addWidget(m_editor);

    connect(m_editor, &TextEditorWidget::contentChanged, this, [this]() {
        if (m_editor && m_editor->document()) {
            qreal rowHeight = 31.625;
            if (m_stylePreset == TitleH1) rowHeight = 63.25;
            else if (m_stylePreset == SubtitleH2) rowHeight = 47.4;

            QTextCursor cursor = m_editor->textCursor();
            QTextBlock block = cursor.block();
            if (block.isValid()) {
                QTextBlockFormat bf = block.blockFormat();
                if (std::abs(bf.lineHeight() - rowHeight) > 0.1) {
                    bf.setLineHeight(rowHeight, QTextBlockFormat::FixedHeight);
                    bf.setTopMargin(0);
                    bf.setBottomMargin(0);
                    cursor.setBlockFormat(bf);
                }
            }
        }
        emit textChanged();
    });

    connect(m_editor, &TextEditorWidget::focusChanged, this, [this](bool f) {
        m_isFocused = f;
        update();
        emit interactionStateChanged();
    });

    applyTextStyle();
}

QString TextBlockContainerWidget::text() const {
    return m_editor ? m_editor->toPlainText() : QString();
}

void TextBlockContainerWidget::setText(const QString &text) {
    if (m_editor) {
        m_editor->setPlainText(text);
        applyTextStyle();
    }
}

void TextBlockContainerWidget::focusEditorAndSelectAll() {
    m_isFocused = true;
    update();
    if (m_editor) {
        m_editor->setFocus();
        m_editor->selectAll();
    }
    emit interactionStateChanged();
}

void TextBlockContainerWidget::clearEditorFocus() {
    if (m_editor) {
        m_editor->clearFocus();
    }
    m_isFocused = false;
    update();
}

void TextBlockContainerWidget::setStylePreset(StylePreset preset) {
    m_stylePreset = preset;
    applyTextStyle();
    emit textChanged();
}

void TextBlockContainerWidget::setTextColor(const QColor &color) {
    m_textColor = color;
    applyTextStyle();
    emit textChanged();
}

void TextBlockContainerWidget::setContainerSize(const QSize &s) {
    resize(s.expandedTo(minimumSize()));
}

void TextBlockContainerWidget::setInteractiveMode(bool enable) {
    m_isInteractiveMode = enable;
    if (!enable) {
        m_isHovered = false;
        m_isFocused = false;
        m_activeResizeHandle = NoHandle;
        m_hoverResizeHandle = NoHandle;
        setCursor(Qt::ArrowCursor);
    }
    update();
}

void TextBlockContainerWidget::applyTextStyle() {
    if (!m_editor) return;

    QFont f("Inter", 14);
    f.setStyleHint(QFont::SansSerif);

    qreal rowHeight = 31.625; // Exact 5mm notebook square height (CanvaGrid / CalculusGrid)

    switch (m_stylePreset) {
    case TitleH1:
        f.setPointSize(28);
        f.setBold(true);
        f.setWeight(QFont::Bold);
        rowHeight = 63.25; // 2 grid rows
        break;
    case SubtitleH2:
        f.setPointSize(20);
        f.setBold(true);
        f.setWeight(QFont::DemiBold);
        rowHeight = 47.4; // 1.5 grid rows
        break;
    case HeadingH3:
        f.setPointSize(16);
        f.setBold(true);
        f.setWeight(QFont::Medium);
        rowHeight = 31.625; // 1 grid row
        break;
    case BodyText:
    default:
        f.setPointSize(14);
        f.setBold(false);
        f.setWeight(QFont::Normal);
        rowHeight = 31.625; // Exact 1 grid row (5mm notebook square)
        break;
    }

    m_editor->setFont(f);

    QTextDocument *doc = m_editor->document();
    if (doc) {
        doc->setDefaultFont(f);
        doc->setDocumentMargin(0);
        QTextBlockFormat blockFormat;
        blockFormat.setLineHeight(rowHeight, QTextBlockFormat::FixedHeight);
        blockFormat.setTopMargin(0);
        blockFormat.setBottomMargin(0);
        for (QTextBlock it = doc->begin(); it != doc->end(); it = it.next()) {
            QTextCursor cursor(it);
            cursor.setBlockFormat(blockFormat);
        }
    }

    m_editor->setStyleSheet(QString(
        "QTextEdit {"
        "   background-color: transparent;"
        "   color: %1;"
        "   border: none;"
        "   padding: 0px 4px;"
        "   line-height: %2px;"
        "   selection-background-color: #2b5c9e;"
        "   selection-color: #ffffff;"
        "}"
        "QScrollBar:vertical {"
        "   background: transparent;"
        "   width: 4px;"
        "}"
        "QScrollBar::handle:vertical {"
        "   background: rgba(59, 66, 97, 180);"
        "   border-radius: 2px;"
        "}"
    ).arg(m_textColor.name()).arg(rowHeight));
}

TextBlockContainerWidget::ResizeHandle TextBlockContainerWidget::hitTestResizeHandle(const QPoint &pos) const {
    const int w = width();
    const int h = height();
    const int cornerSize = 20;

    if (pos.x() >= w - cornerSize && pos.y() >= h - cornerSize) {
        return CornerHandle;
    }
    return NoHandle;
}

void TextBlockContainerWidget::enterEvent(QEnterEvent *event) {
    if (m_isInteractiveMode) {
        m_isHovered = true;
        update();
        emit interactionStateChanged();
    }
    QWidget::enterEvent(event);
}

void TextBlockContainerWidget::leaveEvent(QEvent *event) {
    m_isHovered = false;
    m_hoverResizeHandle = NoHandle;
    setCursor(Qt::ArrowCursor);
    update();
    emit interactionStateChanged();
    QWidget::leaveEvent(event);
}

bool TextBlockContainerWidget::eventFilter(QObject *obj, QEvent *event) {
    if (!m_isInteractiveMode) {
        return QWidget::eventFilter(obj, event);
    }

    if (obj == m_editor) {
        if (event->type() == QEvent::Enter) {
            m_isHovered = true;
            update();
            emit interactionStateChanged();
        } else if (event->type() == QEvent::Leave) {
            if (!rect().contains(mapFromGlobal(QCursor::pos()))) {
                m_isHovered = false;
                update();
                emit interactionStateChanged();
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void TextBlockContainerWidget::mousePressEvent(QMouseEvent *event) {
    if (!m_isInteractiveMode) {
        event->ignore();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        ResizeHandle handle = hitTestResizeHandle(event->pos());
        if (handle != NoHandle) {
            m_activeResizeHandle = handle;
            m_dragStartPos = event->globalPosition().toPoint();
            m_dragStartSize = size();
            update();
            event->accept();
            return;
        }

        m_draggingItem = true;
        m_dragStartPos = event->globalPosition().toPoint();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void TextBlockContainerWidget::mouseMoveEvent(QMouseEvent *event) {
    if (!m_isInteractiveMode) {
        event->ignore();
        return;
    }

    if (m_activeResizeHandle == CornerHandle) {
        QPoint delta = event->globalPosition().toPoint() - m_dragStartPos;
        int newW = qMax(minimumWidth(), m_dragStartSize.width() + delta.x());
        int newH = qMax(minimumHeight(), m_dragStartSize.height() + delta.y());
        resize(newW, newH);
        update();
        emit textChanged();
        event->accept();
        return;
    }

    if (m_draggingItem) {
        QPoint delta = event->globalPosition().toPoint() - m_dragStartPos;
        m_dragStartPos = event->globalPosition().toPoint();
        emit moveDeltaRequested(QPointF(delta.x(), delta.y()));
        event->accept();
        return;
    }

    ResizeHandle h = hitTestResizeHandle(event->pos());
    if (h != m_hoverResizeHandle) {
        m_hoverResizeHandle = h;
        if (h == CornerHandle) {
            setCursor(Qt::SizeFDiagCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
        update();
    }

    QWidget::mouseMoveEvent(event);
}

void TextBlockContainerWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (m_activeResizeHandle != NoHandle || m_draggingItem) {
            m_activeResizeHandle = NoHandle;
            m_draggingItem = false;
            emit textChanged();
            update();
            event->accept();
            return;
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void TextBlockContainerWidget::paintEvent(QPaintEvent * /*event*/) {
    const bool isInteractive = m_isInteractiveMode && (m_isHovered || m_isFocused || (m_activeResizeHandle != NoHandle));
    if (!isInteractive) {
        return; // Zero painting: no box, no frame, no grips! Completely clean on the canvas.
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();

    // Corner resize grip only
    const bool cornerHovered = (m_hoverResizeHandle == CornerHandle) || (m_activeResizeHandle == CornerHandle);
    QColor accentColor = cornerHovered ? QColor("#7dcfff") : QColor(122, 162, 247, 160);

    QPen gripPen(accentColor, 1.8, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(gripPen);
    painter.drawLine(QPointF(w - 14, h - 4), QPointF(w - 4, h - 14));
    painter.drawLine(QPointF(w - 9, h - 4), QPointF(w - 4, h - 9));
    painter.drawLine(QPointF(w - 4, h - 4), QPointF(w - 4, h - 4));
}

QString TextBlockContainerWidget::serialize() const {
    QJsonObject obj;
    obj["text"] = text();
    obj["preset"] = static_cast<int>(m_stylePreset);
    obj["color"] = m_textColor.name();
    obj["w"] = width();
    obj["h"] = height();
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void TextBlockContainerWidget::deserialize(const QString &jsonStr) {
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (!doc.isObject()) return;
    QJsonObject obj = doc.object();

    if (obj.contains("preset")) {
        setStylePreset(static_cast<StylePreset>(obj["preset"].toInt()));
    }
    if (obj.contains("color")) {
        setTextColor(QColor(obj["color"].toString()));
    }
    if (obj.contains("w") && obj.contains("h")) {
        setContainerSize(QSize(obj["w"].toInt(), obj["h"].toInt()));
    }
    if (obj.contains("text")) {
        setText(obj["text"].toString());
    }
}

// ============================================================================
// TextBlockItem
// ============================================================================

TextBlockItem::TextBlockItem(const QPointF &initialPos, QGraphicsItem *parent)
    : QGraphicsProxyWidget(parent) {
    m_containerWidget = new TextBlockContainerWidget();
    setWidget(m_containerWidget);
    setPos(initialPos);
    setZValue(15.0);
    setFlags(ItemIsSelectable | ItemIsFocusable | ItemSendsGeometryChanges);

    setupFloatingHeader();

    connect(m_containerWidget, &TextBlockContainerWidget::textChanged, this, &TextBlockItem::blockModified);
    connect(m_containerWidget, &TextBlockContainerWidget::interactionStateChanged, this, &TextBlockItem::updateHeaderVisibility);
    connect(m_containerWidget, &TextBlockContainerWidget::moveDeltaRequested, this, [this](const QPointF &delta) {
        setPos(this->pos() + delta);
        emit blockModified();
    });

    setInteractiveMode(false);
}

TextBlockItem::~TextBlockItem() {
    if (m_headerWidget) {
        m_headerWidget->deleteLater();
        m_headerWidget = nullptr;
    }
}

void TextBlockItem::setupFloatingHeader() {
    m_headerWidget = new QWidget();
    m_headerWidget->setAttribute(Qt::WA_TranslucentBackground, true);
    m_headerWidget->setFixedHeight(28);
    m_headerWidget->setStyleSheet(
        "QWidget#HeaderBar {"
        "   background-color: rgba(22, 26, 40, 245);"
        "   border: 1px solid rgba(59, 66, 97, 230);"
        "   border-radius: 6px;"
        "}"
    );
    m_headerWidget->setObjectName("HeaderBar");

    auto *headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(6, 2, 6, 2);
    headerLayout->setSpacing(6);

    auto *iconLabel = new QLabel(m_headerWidget);
    iconLabel->setPixmap(AppIcons::getPixmap(AppIcons::IconType::Text, QColor("#7aa2f7"), 14));
    iconLabel->setStyleSheet("background: transparent; border: none;");
    headerLayout->addWidget(iconLabel);

    m_styleCombo = new QComboBox(m_headerWidget);
    m_styleCombo->addItem("Texto Normal", TextBlockContainerWidget::BodyText);
    m_styleCombo->addItem("Título (H1)", TextBlockContainerWidget::TitleH1);
    m_styleCombo->addItem("Subtítulo (H2)", TextBlockContainerWidget::SubtitleH2);
    m_styleCombo->addItem("Encabezado (H3)", TextBlockContainerWidget::HeadingH3);
    m_styleCombo->setStyleSheet(
        "QComboBox {"
        "   background-color: rgba(36, 40, 59, 200);"
        "   color: #c0caf5;"
        "   border: 1px solid rgba(59, 66, 97, 180);"
        "   border-radius: 4px;"
        "   padding: 1px 6px;"
        "   font-size: 8pt;"
        "   font-weight: 500;"
        "}"
        "QComboBox:hover { background-color: rgba(47, 53, 73, 230); border-color: #7aa2f7; }"
        "QComboBox::drop-down { border: none; width: 14px; }"
    );
    headerLayout->addWidget(m_styleCombo);

    m_colorBtn = new QPushButton("Color", m_headerWidget);
    m_colorBtn->setIcon(AppIcons::get(AppIcons::IconType::Palette, m_containerWidget->textColor(), 13));
    m_colorBtn->setStyleSheet(
        "QPushButton {"
        "   background-color: rgba(36, 40, 59, 200);"
        "   color: #7aa2f7;"
        "   border: 1px solid rgba(59, 66, 97, 180);"
        "   border-radius: 4px;"
        "   padding: 2px 8px;"
        "   font-size: 8pt;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: rgba(47, 53, 73, 230); border-color: #7aa2f7; }"
    );

    auto *colorMenu = new QMenu(m_colorBtn);
    const QVector<QPair<QString, QColor>> colors = {
        {"Tinta Negra (#111827)", QColor("#111827")},
        {"Azul Cuaderno (#1e3a8a)", QColor("#1e3a8a")},
        {"Gris Grafito (#374151)", QColor("#374151")},
        {"Azul Tokyo (#7aa2f7)", QColor("#7aa2f7")},
        {"Rojo (#dc2626)", QColor("#dc2626")},
        {"Verde (#16a34a)", QColor("#16a34a")},
        {"Blanco Nieve (#ffffff)", QColor("#ffffff")},
        {"Lavanda (#c0caf5)", QColor("#c0caf5")}
    };

    for (const auto &c : colors) {
        auto *act = colorMenu->addAction(c.first);
        connect(act, &QAction::triggered, this, [this, c]() {
            setTextColor(c.second);
        });
    }

    auto *customAct = colorMenu->addAction("Personalizado...");
    connect(customAct, &QAction::triggered, this, [this]() {
        auto *dlg = new QColorDialog(textColor(), m_headerWidget->window());
        dlg->setWindowTitle("Color del Texto");
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setOption(QColorDialog::DontUseNativeDialog, true);
        connect(dlg, &QColorDialog::colorSelected, this, [this](const QColor &c) {
            if (c.isValid()) setTextColor(c);
        });
        dlg->show();
        dlg->raise();
        dlg->activateWindow();
    });

    m_colorBtn->setMenu(colorMenu);
    headerLayout->addWidget(m_colorBtn);

    headerLayout->addStretch(1);

    m_closeBtn = new QPushButton(m_headerWidget);
    m_closeBtn->setIcon(AppIcons::get(AppIcons::IconType::Close, QColor("#565f89"), 12));
    m_closeBtn->setFixedSize(20, 20);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setStyleSheet(
        "QPushButton {"
        "   background: transparent;"
        "   border: none;"
        "   border-radius: 4px;"
        "}"
        "QPushButton:hover { background-color: rgba(247, 118, 142, 200); color: #ffffff; }"
    );
    connect(m_closeBtn, &QPushButton::clicked, this, [this]() {
        if (m_headerWidget) m_headerWidget->hide();
        if (scene()) {
            scene()->removeItem(this);
        }
        emit itemDeleted(this);
    });
    headerLayout->addWidget(m_closeBtn);

    connect(m_styleCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        if (m_containerWidget) {
            m_containerWidget->setStylePreset(static_cast<TextBlockContainerWidget::StylePreset>(m_styleCombo->itemData(idx).toInt()));
        }
    });

    m_headerWidget->hide();
}

void TextBlockItem::setInteractiveMode(bool enable) {
    m_isInteractiveMode = enable;
    if (m_containerWidget) {
        m_containerWidget->setInteractiveMode(enable);
    }

    setAcceptedMouseButtons(enable ? (Qt::LeftButton | Qt::RightButton) : Qt::NoButton);
    setAcceptHoverEvents(enable);

    if (!enable) {
        if (m_headerWidget) m_headerWidget->hide();
        if (m_containerWidget) m_containerWidget->clearEditorFocus();
    } else {
        updateHeaderVisibility();
    }
}

void TextBlockItem::updateHeaderVisibility() {
    if (!m_headerWidget || !m_containerWidget) return;

    if (!m_isInteractiveMode) {
        m_headerWidget->hide();
        return;
    }

    bool shouldShow = isSelected() || hasFocus() || m_containerWidget->isEditorFocused();
    if (!shouldShow) {
        m_headerWidget->hide();
        return;
    }

    if (scene() && !scene()->views().isEmpty()) {
        auto *view = scene()->views().first();
        if (view && view->viewport()) {
            if (m_headerWidget->parent() != view->viewport()) {
                m_headerWidget->setParent(view->viewport());
            }

            QPoint viewPos = view->mapFromScene(pos());
            int targetX = viewPos.x();
            int targetY = viewPos.y() - m_headerWidget->height() - 4;
            if (targetY < 8) {
                targetY = viewPos.y() + (m_containerWidget ? m_containerWidget->height() : 60) + 4;
            }

            int targetW = qMax(260, qMin(view->viewport()->width() - 20, m_containerWidget->width()));
            m_headerWidget->setFixedWidth(targetW);
            m_headerWidget->move(qBound(10, targetX, view->viewport()->width() - targetW - 10), targetY);
            m_headerWidget->show();
            m_headerWidget->raise();
            return;
        }
    }

    m_headerWidget->hide();
}

QVariant TextBlockItem::itemChange(GraphicsItemChange change, const QVariant &value) {
    if (change == ItemPositionHasChanged || change == ItemSelectedHasChanged) {
        updateHeaderVisibility();
    }
    return QGraphicsProxyWidget::itemChange(change, value);
}

void TextBlockItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    if (m_isInteractiveMode) {
        updateHeaderVisibility();
    }
    QGraphicsProxyWidget::hoverEnterEvent(event);
}

void TextBlockItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    if (m_isInteractiveMode) {
        updateHeaderVisibility();
    }
    QGraphicsProxyWidget::hoverLeaveEvent(event);
}

QString TextBlockItem::text() const {
    return m_containerWidget ? m_containerWidget->text() : QString();
}

void TextBlockItem::setText(const QString &text) {
    if (m_containerWidget) {
        m_containerWidget->setText(text);
    }
}

void TextBlockItem::focusEditorAndSelectAll() {
    setFocus();
    if (m_containerWidget) {
        m_containerWidget->focusEditorAndSelectAll();
    }
    updateHeaderVisibility();
}

QColor TextBlockItem::textColor() const {
    return m_containerWidget ? m_containerWidget->textColor() : QColor();
}

void TextBlockItem::setTextColor(const QColor &color) {
    if (m_containerWidget) {
        m_containerWidget->setTextColor(color);
    }
    if (m_colorBtn) {
        m_colorBtn->setIcon(AppIcons::get(AppIcons::IconType::Palette, color, 13));
    }
}

TextBlockContainerWidget::StylePreset TextBlockItem::stylePreset() const {
    return m_containerWidget ? m_containerWidget->stylePreset() : TextBlockContainerWidget::BodyText;
}

void TextBlockItem::setStylePreset(TextBlockContainerWidget::StylePreset preset) {
    if (m_containerWidget) {
        m_containerWidget->setStylePreset(preset);
    }
    if (m_styleCombo) {
        int idx = m_styleCombo->findData(preset);
        if (idx >= 0 && m_styleCombo->currentIndex() != idx) {
            m_styleCombo->setCurrentIndex(idx);
        }
    }
}

QString TextBlockItem::serialize() const {
    return m_containerWidget ? m_containerWidget->serialize() : QString();
}

void TextBlockItem::deserialize(const QString &data) {
    if (m_containerWidget) {
        m_containerWidget->deserialize(data);
    }
    if (m_styleCombo && m_containerWidget) {
        int idx = m_styleCombo->findData(m_containerWidget->stylePreset());
        if (idx >= 0) m_styleCombo->setCurrentIndex(idx);
    }
    if (m_colorBtn && m_containerWidget) {
        m_colorBtn->setIcon(AppIcons::get(AppIcons::IconType::Palette, m_containerWidget->textColor(), 13));
    }
}

} // namespace OmniNote
