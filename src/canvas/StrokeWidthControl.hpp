#pragma once

#include <QWidget>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QPainter>
#include <QColor>
#include <QWheelEvent>

namespace OmniNote {

class StrokeWidthPreview : public QWidget {
    Q_OBJECT

public:
    explicit StrokeWidthPreview(QWidget *parent = nullptr);

    void setWidth(double w);
    void setColor(const QColor &c);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    double m_width{2.5};
    QColor m_color{QColor("#7aa2f7")};
};

class StrokeWidthControl : public QWidget {
    Q_OBJECT

public:
    explicit StrokeWidthControl(QWidget *parent = nullptr);

    double currentWidth() const;

public slots:
    void setWidth(double w);
    void setColor(const QColor &c);
    void stepUp();
    void stepDown();

signals:
    void widthChanged(double w);

protected:
    void wheelEvent(QWheelEvent *event) override;

private:
    void updatePresetStates(double w);

    QPushButton *m_btnFino{nullptr};
    QPushButton *m_btnNormal{nullptr};
    QPushButton *m_btnGrueso{nullptr};
    QPushButton *m_btnMarcador{nullptr};

    QSlider *m_slider{nullptr};
    QLabel *m_valLabel{nullptr};
    StrokeWidthPreview *m_preview{nullptr};
};

} // namespace OmniNote
