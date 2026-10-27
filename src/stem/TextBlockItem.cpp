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
#include <QCursor>

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
    setAttribute(Qt::WA_TranslucentBackground, true);
    setTextInteractionFlags(Qt::TextEditorInteraction);
    if (viewport()) {
        viewport()->setCursor(Qt::IBeamCursor);
    }

    setStyleSheet(
        "QTextEdit {"
        "   background-color: transparent;"
        "   border: none;"
        "   padding: 4px;"
        "   selection-background-color: #2b5c9e;"
        "   selection-color: #ffffff;"
        "}"
        "QScrollBar:vertical {"
        "   background: transparent;"
        "   width: 5px;"
        "}"
        "QScrollBar::handle:vertical {"
        "   background: rgba(59, 66, 97, 180);"
        "   border-radius: 2px;"
        "}"
    );

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
    setMinimumSize(180, 60);
    resize(380, 95);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    auto *mainLayout = new QVBoxLayout(this);
    // Margins leave space for borders and resize grips outside the editor
    mainLayout->setContentsMargins(6, 4, 10, 10);
    mainLayout->setSpacing(2);

    // Header bar container widget (retains size when hidden so text never jumps)
    m_headerWidget = new QWidget(this);
    m_headerWidget->installEventFilter(this);
    m_headerWidget->setCursor(Qt::SizeAllCursor);
    auto sp = m_headerWidget->sizePolicy();
    sp.setRetainSizeWhenHidden(true);
    m_headerWidget->setSizePolicy(sp);

    auto *headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(6);

    auto *iconLabel = new QLabel(m_headerWidget);
    iconLabel->setPixmap(AppIcons::getPixmap(AppIcons::IconType::Text, QColor("#7aa2f7"), 14));
    iconLabel->setStyleSheet("background: transparent;");
    headerLayout->addWidget(iconLabel);

    m_styleCombo = new QComboBox(m_headerWidget);
    m_styleCombo->addItem("Título (H1)", TitleH1);
    m_styleCombo->addItem("Subtítulo (H2)", SubtitleH2);
    m_styleCombo->addItem("Encabezado (H3)", HeadingH3);
    m_styleCombo->addItem("Texto Normal", BodyText);
    m_styleCombo->setStyleSheet(
        "QComboBox {"
        "   background-color: rgba(36, 40, 59, 180);"
        "   color: #c0caf5;"
        "   border: 1px solid rgba(59, 66, 97, 180);"
        "   border-radius: 4px;"
        "   padding: 2px 8px;"
        "   font-size: 8pt;"
        "   font-weight: 500;"
        "}"
        "QComboBox:hover { background-color: rgba(47, 53, 73, 220); border-color: #7aa2f7; }"
        "QComboBox::drop-down { border: none; width: 14px; }"
        "QComboBox QAbstractItemView {"
        "   background-color: #1f2335;"
        "   color: #c0caf5;"
        "   border: 1px solid #3b4261;"
        "   selection-background-color: #3d59a1;"
        "   selection-color: #ffffff;"
        "}"
    );
    headerLayout->addWidget(m_styleCombo);

    m_colorBtn = new QPushButton("Color", m_headerWidget);
    m_colorBtn->setIcon(AppIcons::get(AppIcons::IconType::Palette, QColor("#7aa2f7"), 13));
    m_colorBtn->setStyleSheet(
        "QPushButton {"
        "   background-color: rgba(36, 40, 59, 180);"
        "   color: #7aa2f7;"
        "   border: 1px solid rgba(59, 66, 97, 180);"
        "   border-radius: 4px;"
        "   padding: 2px 10px;"
        "   font-size: 8pt;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: rgba(47, 53, 73, 220); border-color: #7aa2f7; }"
    );

    auto *colorMenu = new QMenu(m_colorBtn);
    const QVector<QPair<QString, QColor>> colors = {
        {"Azul Tokyo (#7aa2f7)", QColor("#7aa2f7")},
        {"Blanco Nieve (#ffffff)", QColor("#ffffff")},
        {"Lavanda (#c0caf5)", QColor("#c0caf5")},
        {"Verde Neón (#9ece6a)", QColor("#9ece6a")},
        {"Ámbar (#e0af68)", QColor("#e0af68")},
        {"Cian (#7dcfff)", QColor("#7dcfff")},
        {"Púrpura (#bb9af7)", QColor("#bb9af7")},
        {"Coral (#f7768e)", QColor("#f7768e")}
    };

    for (const auto &c : colors) {
        auto *act = colorMenu->addAction(c.first);
        connect(act, &QAction::triggered, this, [this, c]() {
            setTextColor(c.second);
        });
    }

    auto *customAct = colorMenu->addAction("Personalizado...");
    connect(customAct, &QAction::triggered, this, [this]() {
        QColor chosen = QColorDialog::getColor(m_textColor, this, "Color del Texto");
        if (chosen.isValid()) {
            setTextColor(chosen);
        }
    });

    m_colorBtn->setMenu(colorMenu);
    headerLayout->addWidget(m_colorBtn);

    headerLayout->addStretch(1);

    m_closeBtn = new QPushButton(m_headerWidget);
    m_closeBtn->setIcon(AppIcons::get(AppIcons::IconType::Close, QColor("#565f89"), 12));
    m_closeBtn->setFixedSize(22, 20);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setStyleSheet(
        "QPushButton {"
        "   background: transparent;"
        "   border: none;"
        "   border-radius: 4px;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: rgba(247, 118, 142, 200); color: #ffffff; }"
    );
    connect(m_closeBtn, &QPushButton::clicked, this, &TextBlockContainerWidget::deleteRequested);
    headerLayout->addWidget(m_closeBtn);

    mainLayout->addWidget(m_headerWidget);

    // Text editor area
    m_editor = new TextEditorWidget(this);
    m_editor->installEventFilter(this);
    mainLayout->addWidget(m_editor, 1);

    connect(m_styleCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        setStylePreset(static_cast<StylePreset>(m_styleCombo->itemData(idx).toInt()));
    });
    connect(m_editor, &TextEditorWidget::contentChanged, this, &TextBlockContainerWidget::textChanged);
    connect(m_editor, &TextEditorWidget::focusChanged, this, [this](bool f) {
        m_isFocused = f;
        updateControlsVisibility();
    });

    applyTextStyle();
    updateControlsVisibility();
}

QString TextBlockContainerWidget::text() const {
    return m_editor ? m_editor->toPlainText() : QString();
}

void TextBlockContainerWidget::setText(const QString &text) {
    if (m_editor) {
        m_editor->setPlainText(text);
    }
}

void TextBlockContainerWidget::focusEditorAndSelectAll() {
    m_isFocused = true;
    updateControlsVisibility();
    if (m_editor) {
        m_editor->setFocus();
        m_editor->selectAll();
    }
}

void TextBlockContainerWidget::setStylePreset(StylePreset preset) {
    m_stylePreset = preset;
    int idx = m_styleCombo->findData(preset);
    if (idx >= 0 && m_styleCombo->currentIndex() != idx) {
        m_styleCombo->setCurrentIndex(idx);
    }
    applyTextStyle();
    emit textChanged();
}

void TextBlockContainerWidget::setTextColor(const QColor &color) {
    m_textColor = color;
    m_colorBtn->setStyleSheet(QString(
        "QPushButton {"
        "   background-color: rgba(36, 40, 59, 180);"
        "   color: %1;"
        "   border: 1px solid rgba(59, 66, 97, 180);"
        "   border-radius: 4px;"
        "   padding: 2px 10px;"
        "   font-size: 8pt;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: rgba(47, 53, 73, 220); border-color: %1; }"
    ).arg(color.name()));
    applyTextStyle();
    emit textChanged();
}

void TextBlockContainerWidget::setContainerSize(const QSize &s) {
    resize(s.expandedTo(minimumSize()));
}

void TextBlockContainerWidget::applyTextStyle() {
    if (!m_editor) return;

    QFont f("Inter", 12);
    f.setStyleHint(QFont::SansSerif);

    switch (m_stylePreset) {
    case TitleH1:
        f.setPointSize(22);
        f.setBold(true);
        f.setWeight(QFont::Bold);
        break;
    case SubtitleH2:
        f.setPointSize(16);
        f.setBold(true);
        f.setWeight(QFont::DemiBold);
        break;
    case HeadingH3:
        f.setPointSize(13);
        f.setBold(true);
        f.setWeight(QFont::Medium);
        break;
    case BodyText:
    default:
        f.setPointSize(11);
        f.setBold(false);
        f.setWeight(QFont::Normal);
        break;
    }

    m_editor->setFont(f);
    m_editor->setStyleSheet(QString(
        "QTextEdit {"
        "   background-color: transparent;"
        "   color: %1;"
        "   border: none;"
        "   padding: 4px;"
        "   selection-background-color: #2b5c9e;"
        "   selection-color: #ffffff;"
        "}"
        "QScrollBar:vertical {"
        "   background: transparent;"
        "   width: 5px;"
        "}"
        "QScrollBar::handle:vertical {"
        "   background: rgba(59, 66, 97, 180);"
        "   border-radius: 2px;"
        "}"
    ).arg(m_textColor.name()));
}

TextBlockContainerWidget::ResizeHandle TextBlockContainerWidget::hitTestResizeHandle(const QPoint &pos) const {
    const int w = width();
    const int h = height();
    const int cornerSize = 22;
    const int edgeMargin = 8;

    // Corner handle (bottom-right 22x22 area)
    if (pos.x() >= w - cornerSize && pos.y() >= h - cornerSize) {
        return CornerHandle;
    }
    // Right edge handle (horizontal width adjuster)
    if (pos.x() >= w - edgeMargin && pos.y() >= 32 && pos.y() < h - cornerSize) {
        return RightHandle;
    }
    // Bottom edge handle (vertical height adjuster)
    if (pos.y() >= h - edgeMargin && pos.x() >= 10 && pos.x() < w - cornerSize) {
        return BottomHandle;
    }
    return NoHandle;
}

void TextBlockContainerWidget::enterEvent(QEnterEvent *event) {
    m_isHovered = true;
    updateControlsVisibility();
    QWidget::enterEvent(event);
}

void TextBlockContainerWidget::leaveEvent(QEvent *event) {
    if (!rect().contains(mapFromGlobal(QCursor::pos()))) {
        m_isHovered = false;
        m_hoverResizeHandle = NoHandle;
        setCursor(Qt::ArrowCursor);
        updateControlsVisibility();
    }
    QWidget::leaveEvent(event);
}

bool TextBlockContainerWidget::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_editor) {
        if (event->type() == QEvent::Enter) {
            m_isHovered = true;
            updateControlsVisibility();
        } else if (event->type() == QEvent::Leave) {
            if (!rect().contains(mapFromGlobal(QCursor::pos()))) {
                m_isHovered = false;
                m_hoverResizeHandle = NoHandle;
                updateControlsVisibility();
            }
        }
    } else if (obj == m_headerWidget) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                QWidget *child = m_headerWidget->childAt(me->pos());
                if (!child || child == m_headerWidget || qobject_cast<QLabel *>(child)) {
                    m_draggingHeader = true;
                    m_dragStartPos = me->globalPosition().toPoint();
                    event->accept();
                    return true;
                }
            }
        } else if (event->type() == QEvent::MouseMove) {
            if (m_draggingHeader) {
                auto *me = static_cast<QMouseEvent *>(event);
                QPoint delta = me->globalPosition().toPoint() - m_dragStartPos;
                m_dragStartPos = me->globalPosition().toPoint();
                emit moveDeltaRequested(QPointF(delta.x(), delta.y()));
                event->accept();
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            if (m_draggingHeader) {
                m_draggingHeader = false;
                emit textChanged();
                event->accept();
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void TextBlockContainerWidget::updateControlsVisibility() {
    const bool isInteractive = m_isHovered || m_isFocused || (m_activeResizeHandle != NoHandle) || m_draggingHeader;
    if (m_headerWidget) {
        m_headerWidget->setVisible(isInteractive);
    }
    update();
}

void TextBlockContainerWidget::mousePressEvent(QMouseEvent *event) {
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

        // Header or background drag (any click outside editor on container)
        m_draggingHeader = true;
        m_dragStartPos = event->globalPosition().toPoint();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void TextBlockContainerWidget::mouseMoveEvent(QMouseEvent *event) {
    if (m_activeResizeHandle != NoHandle) {
        QPoint delta = event->globalPosition().toPoint() - m_dragStartPos;
        int newW = m_dragStartSize.width();
        int newH = m_dragStartSize.height();

        if (m_activeResizeHandle == CornerHandle || m_activeResizeHandle == RightHandle) {
            newW = qMax(180, m_dragStartSize.width() + delta.x());
        }
        if (m_activeResizeHandle == CornerHandle || m_activeResizeHandle == BottomHandle) {
            newH = qMax(60, m_dragStartSize.height() + delta.y());
        }

        resize(newW, newH);
        emit textChanged();
        update();
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

    // Update hover state and cursor
    ResizeHandle handle = hitTestResizeHandle(event->pos());
    if (handle != m_hoverResizeHandle) {
        m_hoverResizeHandle = handle;
        update();
    }

    switch (m_hoverResizeHandle) {
    case CornerHandle:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case RightHandle:
        setCursor(Qt::SizeHorCursor);
        break;
    case BottomHandle:
        setCursor(Qt::SizeVerCursor);
        break;
    default:
        if (event->pos().y() <= 32) {
            setCursor(Qt::SizeAllCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
        break;
    }

    QWidget::mouseMoveEvent(event);
}

void TextBlockContainerWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (m_activeResizeHandle != NoHandle || m_draggingHeader) {
            m_activeResizeHandle = NoHandle;
            m_draggingHeader = false;
            emit textChanged();
            update();
            event->accept();
            return;
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void TextBlockContainerWidget::paintEvent(QPaintEvent * /*event*/) {
    const bool isInteractive = m_isHovered || m_isFocused || (m_activeResizeHandle != NoHandle) || m_draggingHeader;
    if (!isInteractive) {
        return; // Zero painting when idle: no recuadro, no frame, no grip! Completely clean on the canvas.
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();

    // Corner resize grip only (NO bounding box / NO recuadro!)
    const bool cornerHovered = (m_hoverResizeHandle == CornerHandle) || (m_activeResizeHandle == CornerHandle);
    QColor accentColor = cornerHovered ? QColor("#7dcfff") : QColor(122, 162, 247, 160);

    QPen gripPen(accentColor, 1.8, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(gripPen);
    painter.drawLine(QPointF(w - 14, h - 4), QPointF(w - 4, h - 14));
    painter.drawLine(QPointF(w - 9, h - 4), QPointF(w - 4, h - 9));
    painter.drawLine(QPointF(w - 4, h - 4), QPointF(w - 4, h - 4));

    // Live dimension badge ONLY while actively dragging to resize
    if (m_activeResizeHandle != NoHandle) {
        QString dimText = QString("%1 × %2 px").arg(w).arg(h);
        QFont badgeFont("Inter", 8, QFont::DemiBold);
        painter.setFont(badgeFont);

        QFontMetrics fm(badgeFont);
        int badgeW = fm.horizontalAdvance(dimText) + 14;
        int badgeH = fm.height() + 6;
        QRectF badgeRect(w - badgeW - 6, h - badgeH - 6, badgeW, badgeH);

        painter.setPen(QPen(QColor("#7aa2f7"), 1.0));
        painter.setBrush(QColor(22, 26, 40, 230));
        painter.drawRoundedRect(badgeRect, 4.0, 4.0);

        painter.setPen(QColor("#7dcfff"));
        painter.drawText(badgeRect, Qt::AlignCenter, dimText);
    }
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

    connect(m_containerWidget, &TextBlockContainerWidget::deleteRequested, this, [this]() {
        if (scene()) {
            scene()->removeItem(this);
        }
        emit itemDeleted(this);
    });

    connect(m_containerWidget, &TextBlockContainerWidget::textChanged, this, &TextBlockItem::blockModified);

    connect(m_containerWidget, &TextBlockContainerWidget::moveDeltaRequested, this, [this](const QPointF &delta) {
        setPos(this->pos() + delta);
        emit blockModified();
    });
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
}

QColor TextBlockItem::textColor() const {
    return m_containerWidget ? m_containerWidget->textColor() : QColor();
}

void TextBlockItem::setTextColor(const QColor &color) {
    if (m_containerWidget) {
        m_containerWidget->setTextColor(color);
    }
}

QString TextBlockItem::serialize() const {
    return m_containerWidget ? m_containerWidget->serialize() : QString();
}

void TextBlockItem::deserialize(const QString &data) {
    if (m_containerWidget) {
        m_containerWidget->deserialize(data);
    }
}

} // namespace OmniNote
