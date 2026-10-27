#pragma once

#include <QUndoCommand>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QList>
#include "StrokeItem.hpp"
#include "../stem/TextBlockItem.hpp"

namespace OmniNote {

class AddStrokeCommand : public QUndoCommand {
public:
    AddStrokeCommand(QGraphicsScene *scene, StrokeItem *item, QUndoCommand *parent = nullptr)
        : QUndoCommand("Añadir Trazo", parent), m_scene(scene), m_item(item), m_ownsItem(false) {}

    ~AddStrokeCommand() override {
        if (m_ownsItem) {
            delete m_item;
        }
    }

    void undo() override {
        if (m_scene && m_item && m_item->scene() == m_scene) {
            m_scene->removeItem(m_item);
            m_ownsItem = true;
        }
    }

    void redo() override {
        if (m_scene && m_item) {
            if (m_item->scene() != m_scene) {
                m_scene->addItem(m_item);
            }
            m_ownsItem = false;
        }
    }

private:
    QGraphicsScene *m_scene;
    StrokeItem *m_item;
    bool m_ownsItem;
};

class DeleteStrokeCommand : public QUndoCommand {
public:
    DeleteStrokeCommand(QGraphicsScene *scene, StrokeItem *item, QUndoCommand *parent = nullptr)
        : QUndoCommand("Borrar Trazo", parent), m_scene(scene), m_item(item), m_ownsItem(false) {}

    ~DeleteStrokeCommand() override {
        if (m_ownsItem) {
            delete m_item;
        }
    }

    void undo() override {
        if (m_scene && m_item) {
            if (m_item->scene() != m_scene) {
                m_scene->addItem(m_item);
            }
            m_ownsItem = false;
        }
    }

    void redo() override {
        if (m_scene && m_item && m_item->scene() == m_scene) {
            m_scene->removeItem(m_item);
            m_ownsItem = true;
        }
    }

private:
    QGraphicsScene *m_scene;
    StrokeItem *m_item;
    bool m_ownsItem;
};



class MoveItemsCommand : public QUndoCommand {
public:
    MoveItemsCommand(const QList<QGraphicsItem*> &items, const QPointF &delta, QUndoCommand *parent = nullptr)
        : QUndoCommand("Mover Elementos", parent), m_items(items), m_delta(delta) {}

    void undo() override {
        for (auto *item : m_items) {
            if (item->type() == StrokeItem::Type) {
                static_cast<StrokeItem *>(item)->translateStroke(-m_delta);
            } else {
                item->setPos(item->pos() - m_delta);
            }
        }
    }

    void redo() override {
        for (auto *item : m_items) {
            if (item->type() == StrokeItem::Type) {
                static_cast<StrokeItem *>(item)->translateStroke(m_delta);
            } else {
                item->setPos(item->pos() + m_delta);
            }
        }
    }

private:
    QList<QGraphicsItem*> m_items;
    QPointF m_delta;
};

class ScaleStrokesCommand : public QUndoCommand {
public:
    ScaleStrokesCommand(const QList<QGraphicsItem*> &items, qreal factor, const QPointF &pivot, QUndoCommand *parent = nullptr)
        : QUndoCommand("Escalar Elementos", parent), m_items(items), m_factor(factor), m_pivot(pivot) {}

    void undo() override {
        if (std::abs(m_factor) > 1e-4) {
            for (auto *item : m_items) {
                if (item->type() == StrokeItem::Type) {
                    static_cast<StrokeItem *>(item)->scaleStroke(1.0 / m_factor, m_pivot);
                } else {
                    item->setPos(m_pivot + (item->pos() - m_pivot) * (1.0 / m_factor));
                }
            }
        }
    }

    void redo() override {
        for (auto *item : m_items) {
            if (item->type() == StrokeItem::Type) {
                static_cast<StrokeItem *>(item)->scaleStroke(m_factor, m_pivot);
            } else {
                item->setPos(m_pivot + (item->pos() - m_pivot) * m_factor);
            }
        }
    }

private:
    QList<QGraphicsItem*> m_items;
    qreal m_factor;
    QPointF m_pivot;
};

class ChangeColorCommand : public QUndoCommand {
public:
    struct ItemColor {
        StrokeItem *item;
        QColor oldColor;
        QColor newColor;
    };

    struct TextBlockColor {
        TextBlockItem *item;
        QColor oldColor;
        QColor newColor;
    };

    ChangeColorCommand(const QVector<ItemColor> &changes,
                       const QVector<TextBlockColor> &textChanges = {},
                       QUndoCommand *parent = nullptr)
        : QUndoCommand("Cambiar Color", parent), m_changes(changes), m_textChanges(textChanges) {}

    void undo() override {
        for (const auto &c : m_changes) {
            c.item->setColor(c.oldColor);
        }
        for (const auto &c : m_textChanges) {
            c.item->setTextColor(c.oldColor);
        }
    }

    void redo() override {
        for (const auto &c : m_changes) {
            c.item->setColor(c.newColor);
        }
        for (const auto &c : m_textChanges) {
            c.item->setTextColor(c.newColor);
        }
    }

private:
    QVector<ItemColor> m_changes;
    QVector<TextBlockColor> m_textChanges;
};

class AddStemBlockCommand : public QUndoCommand {
public:
    AddStemBlockCommand(QGraphicsScene *scene, QGraphicsItem *block, QUndoCommand *parent = nullptr)
        : QUndoCommand("Añadir Bloque STEM", parent), m_scene(scene), m_block(block), m_ownsBlock(false) {}

    ~AddStemBlockCommand() override {
        if (m_ownsBlock) delete m_block;
    }

    void undo() override {
        if (m_scene && m_block && m_block->scene() == m_scene) {
            m_scene->removeItem(m_block);
            m_ownsBlock = true;
        }
    }

    void redo() override {
        if (m_scene && m_block) {
            if (m_block->scene() != m_scene) {
                m_scene->addItem(m_block);
            }
            m_ownsBlock = false;
        }
    }

private:
    QGraphicsScene *m_scene;
    QGraphicsItem *m_block;
    bool m_ownsBlock;
};

class DeleteItemsCommand : public QUndoCommand {
public:
    DeleteItemsCommand(QGraphicsScene *scene, const QList<QGraphicsItem*> &items, QUndoCommand *parent = nullptr)
        : QUndoCommand("Borrar Elementos", parent), m_scene(scene), m_items(items), m_ownsItems(false) {}

    ~DeleteItemsCommand() override {
        if (m_ownsItems) {
            for (auto *it : m_items) delete it;
        }
    }

    void undo() override {
        for (auto *item : m_items) {
            if (item->scene() != m_scene) {
                m_scene->addItem(item);
            }
        }
        m_ownsItems = false;
    }

    void redo() override {
        for (auto *item : m_items) {
            if (item->scene() == m_scene) {
                m_scene->removeItem(item);
            }
        }
        m_ownsItems = true;
    }

private:
    QGraphicsScene *m_scene;
    QList<QGraphicsItem*> m_items;
    bool m_ownsItems;
};

} // namespace OmniNote
