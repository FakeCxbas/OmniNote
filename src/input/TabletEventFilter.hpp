#pragma once

#include <QObject>
#include <QEvent>
#include <QTabletEvent>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QPointF>
#include <QDebug>

namespace OmniNote {

class TabletEventFilter : public QObject {
    Q_OBJECT

public:
    explicit TabletEventFilter(QObject *parent = nullptr)
        : QObject(parent), m_inProximity(false), m_isEraserTip(false), m_currentPressure(0.0) {}

    bool isTabletInProximity() const { return m_inProximity; }
    bool isPhysicalEraser() const { return m_isEraserTip; }
    qreal currentPressure() const { return m_currentPressure; }

signals:
    void tabletPressed(const QPointF &pos, qreal pressure, qreal xTilt, qreal yTilt, bool isEraser);
    void tabletMoved(const QPointF &pos, qreal pressure, qreal xTilt, qreal yTilt, bool isEraser);
    void tabletReleased(const QPointF &pos, qreal pressure, qreal xTilt, qreal yTilt, bool isEraser);
    void pressureChanged(qreal pressure);
    void proximityChanged(bool inProximity, bool isEraser);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        switch (event->type()) {
        case QEvent::TabletEnterProximity: {
            auto *te = static_cast<QTabletEvent *>(event);
            m_inProximity = true;
            checkPointerType(te);
            emit proximityChanged(true, m_isEraserTip);
            return false;
        }

        case QEvent::TabletLeaveProximity: {
            m_inProximity = false;
            m_currentPressure = 0.0;
            emit pressureChanged(0.0);
            emit proximityChanged(false, m_isEraserTip);
            return false;
        }

        case QEvent::TabletPress: {
            auto *te = static_cast<QTabletEvent *>(event);
            m_inProximity = true;
            checkPointerType(te);
            m_currentPressure = te->pressure();
            emit pressureChanged(m_currentPressure);
            emit tabletPressed(te->position(), te->pressure(), te->xTilt(), te->yTilt(), m_isEraserTip);
            // Allow event to propagate to the focused widget / viewport
            return false;
        }

        case QEvent::TabletMove: {
            auto *te = static_cast<QTabletEvent *>(event);
            checkPointerType(te);
            m_currentPressure = te->pressure();
            emit pressureChanged(m_currentPressure);
            emit tabletMoved(te->position(), te->pressure(), te->xTilt(), te->yTilt(), m_isEraserTip);
            return false;
        }

        case QEvent::TabletRelease: {
            auto *te = static_cast<QTabletEvent *>(event);
            checkPointerType(te);
            m_currentPressure = 0.0;
            emit pressureChanged(0.0);
            emit tabletReleased(te->position(), te->pressure(), te->xTilt(), te->yTilt(), m_isEraserTip);
            return false;
        }

        // Palm rejection: Only suppress synthetic mouse events when stylus is actively pressing
        case QEvent::MouseButtonPress:
        case QEvent::MouseMove:
        case QEvent::MouseButtonRelease: {
            auto *me = static_cast<QMouseEvent *>(event);
            if (m_inProximity && m_currentPressure > 0.05 && me->source() == Qt::MouseEventSynthesizedBySystem) {
                event->accept();
                return true;
            }
            break;
        }

        default:
            break;
        }

        return QObject::eventFilter(watched, event);
    }

private:
    void checkPointerType(const QTabletEvent *te) {
        if (!te) return;
        const auto *dev = te->pointingDevice();
        if (dev) {
            m_isEraserTip = (dev->pointerType() == QPointingDevice::PointerType::Eraser);
        }
    }

    bool m_inProximity;
    bool m_isEraserTip;
    qreal m_currentPressure;
};

} // namespace OmniNote
