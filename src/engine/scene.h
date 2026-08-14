#pragma once

#include <QColor>
#include <QMatrix4x4>
#include <QObject>
#include <QString>
#include <QVector3D>
#include <QVector>

struct Part {
    QString name;
    QVector3D position;
    QVector3D rotation; // Euler XYZ, degrees
    QVector3D scale{ 1.0f, 1.0f, 1.0f };
    // Neutral gray default -- a generic placeholder color, not a copied asset.
    QColor color = QColor(163, 162, 165);

    // Shared by SceneRenderer (to draw the Part) and the viewport's
    // click-to-select ray-picker (to invert), so the two can never disagree
    // about where a Part actually is.
    QMatrix4x4 modelMatrix() const {
        QMatrix4x4 m;
        m.translate(position);
        m.rotate(rotation.x(), 1.0f, 0.0f, 0.0f);
        m.rotate(rotation.y(), 0.0f, 1.0f, 0.0f);
        m.rotate(rotation.z(), 0.0f, 0.0f, 1.0f);
        m.scale(scale);
        return m;
    }
};

// Owned by VulkanViewport (mirrors M2's Camera member). MainWindow reaches it
// through the viewport it already owns; SceneRenderer reads it fresh every
// frame, so edits need no explicit "changed" notification -- only additions
// and selection changes need to reach the Explorer tree/Properties dock,
// which live in a different object, hence the QObject/signals. Selection
// lives here rather than in MainWindow because two independent input paths
// (the Explorer tree and viewport clicks) both need to drive and observe it.
class Scene : public QObject {
    Q_OBJECT

public:
    explicit Scene(QObject *parent = nullptr);

    int addPart(const QVector3D &position);

    int count() const { return m_parts.size(); }
    Part &part(int index) { return m_parts[index]; }
    const Part &part(int index) const { return m_parts[index]; }

    int selectedIndex() const { return m_selectedIndex; }
    void setSelectedIndex(int index);

signals:
    void partAdded(int index);
    void selectionChanged(int index);

private:
    QVector<Part> m_parts;
    int m_selectedIndex = -1;
};
