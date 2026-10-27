#pragma once

#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>

namespace OmniNote {

class SelectionActionBar : public QWidget {
    Q_OBJECT

public:
    explicit SelectionActionBar(QWidget *parent = nullptr);

    void setCurrentColor(const QColor &color);

signals:
    void recolorRequested();
    void duplicateRequested();
    void deleteRequested();
    void clearRequested();

private:
    QPushButton *m_colorBtn{nullptr};
    QPushButton *m_dupBtn{nullptr};
    QPushButton *m_delBtn{nullptr};
    QPushButton *m_closeBtn{nullptr};
    QColor m_color{QColor("#7aa2f7")};
};

} // namespace OmniNote
