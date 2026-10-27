#pragma once

#include <QGraphicsProxyWidget>
#include <QPlainTextEdit>
#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QTextBlock>
#include "CppSyntaxHighlighter.hpp"

namespace OmniNote {

class CodeEditorWidget;

class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(CodeEditorWidget *editor);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    CodeEditorWidget *m_editor;
};

class CodeEditorWidget : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit CodeEditorWidget(QWidget *parent = nullptr);
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    QWidget *m_lineNumberArea;
};

class CodeBlockContainerWidget : public QWidget {
    Q_OBJECT

public:
    explicit CodeBlockContainerWidget(QWidget *parent = nullptr);

    QString code() const;
    void setCode(const QString &code);

    CppSyntaxHighlighter::Language language() const;
    void setLanguage(CppSyntaxHighlighter::Language lang);

signals:
    void deleteRequested();
    void codeChanged();
    void moveDeltaRequested(const QPointF &delta);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QComboBox *m_languageCombo;
    QPushButton *m_copyButton;
    QPushButton *m_closeButton;
    CodeEditorWidget *m_editor;
    CppSyntaxHighlighter *m_highlighter;

    bool m_resizing{false};
    bool m_draggingHeader{false};
    QPoint m_dragStartPos;
    QSize m_dragStartSize;
};

class CodeBlockItem : public QGraphicsProxyWidget {
    Q_OBJECT

public:
    enum { Type = UserType + 20 };
    int type() const override { return Type; }

    explicit CodeBlockItem(const QPointF &pos = QPointF(100, 100), QGraphicsItem *parent = nullptr);
    ~CodeBlockItem() override = default;

    QString code() const;
    void setCode(const QString &code);

    CppSyntaxHighlighter::Language language() const;
    void setLanguage(CppSyntaxHighlighter::Language lang);

    QString serialize() const;
    void deserialize(const QString &data);

signals:
    void itemDeleted(CodeBlockItem *item);
    void blockModified();

private:
    CodeBlockContainerWidget *m_containerWidget;
};

} // namespace OmniNote
