#include "scene.h"

Scene::Scene(QObject *parent)
    : QObject(parent) {
}

int Scene::addPart(const QVector3D &position) {
    Part p;
    p.name = QStringLiteral("Part");
    p.position = position;
    m_parts.append(p);

    const int index = m_parts.size() - 1;
    if (index > 0) {
        m_parts[index].name = QStringLiteral("Part.%1").arg(index);
    }

    emit partAdded(index);
    return index;
}

void Scene::setSelectedIndex(int index) {
    if (index < 0 || index >= m_parts.size()) {
        index = -1;
    }
    if (index == m_selectedIndex) {
        return;
    }
    m_selectedIndex = index;
    emit selectionChanged(m_selectedIndex);
}
