#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QColor>

namespace OmniNote {

class FormulaBlockItem;

class LatexEditorOverlay : public QWidget {
    Q_OBJECT

public:
    explicit LatexEditorOverlay(QWidget *parent = nullptr);
    ~LatexEditorOverlay() override = default;

    void openForFormula(FormulaBlockItem *item = nullptr);

signals:
    void formulaSaved(FormulaBlockItem *item, const QString &latex, const QColor &color);
    void newFormulaRequested(const QString &latex, const QColor &color);
    void cancelled();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void setupUi();
    void updateLivePreview();

    FormulaBlockItem *m_currentItem{nullptr};
    QColor m_selectedColor{QColor("#ffffff")};

    QWidget *m_dialogCard{nullptr};
    QPlainTextEdit *m_latexEditor{nullptr};
    QLabel *m_previewLabel{nullptr};
    QLabel *m_statusLabel{nullptr};
    QTimer *m_debounceTimer{nullptr};
};

} // namespace OmniNote
