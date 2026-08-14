#include "vulkan_viewport.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QVector4D>
#include <QWheelEvent>
#include <algorithm>
#include <limits>

#include "scene_renderer.h"

VulkanViewport::VulkanViewport(QVulkanInstance *instance) {
    setVulkanInstance(instance);
}

QVulkanWindowRenderer *VulkanViewport::createRenderer() {
    return new SceneRenderer(this);
}

QMatrix4x4 VulkanViewport::projectionMatrix() {
    const QSize sz = swapChainImageSize();
    const float aspect = sz.height() > 0
        ? static_cast<float>(sz.width()) / static_cast<float>(sz.height())
        : 1.0f;

    QMatrix4x4 proj;
    proj.perspective(45.0f, aspect, 0.01f, 1000.0f);
    // clipCorrectionMatrix() adapts perspective()'s standard OpenGL-style
    // output to Vulkan's Y-down/0..1-depth clip-space conventions -- the
    // documented Qt pattern, same one SceneRenderer already used inline
    // before this was pulled out into a shared helper.
    return clipCorrectionMatrix() * proj;
}

void VulkanViewport::updateMovement() {
    if (!m_movementTimer.isValid()) {
        // First call: nothing to measure yet, just start the clock.
        m_movementTimer.start();
        return;
    }
    const float dt = static_cast<float>(m_movementTimer.restart()) / 1000.0f;

    constexpr float kMoveSpeed = 4.0f; // world units/second
    float forwardAmount = 0.0f;
    float rightAmount = 0.0f;
    if (m_moveForwardHeld) forwardAmount += kMoveSpeed * dt;
    if (m_moveBackHeld) forwardAmount -= kMoveSpeed * dt;
    if (m_moveRightHeld) rightAmount += kMoveSpeed * dt;
    if (m_moveLeftHeld) rightAmount -= kMoveSpeed * dt;

    if (forwardAmount != 0.0f) {
        m_camera.moveForward(forwardAmount);
    }
    if (rightAmount != 0.0f) {
        m_camera.strafeRight(rightAmount);
    }
}

int VulkanViewport::pickPart(const QPointF &pos) {
    const QSize sz = swapChainImageSize();
    if (sz.width() <= 0 || sz.height() <= 0) {
        return -1;
    }

    // event->position() is in device-independent (logical) pixels, but
    // swapChainImageSize() is the actual framebuffer size in physical
    // pixels -- on any display with scaling above 100%, dividing one by the
    // other directly would silently misalign clicks from what's on screen.
    const qreal dpr = devicePixelRatio();
    const float physX = static_cast<float>(pos.x() * dpr);
    const float physY = static_cast<float>(pos.y() * dpr);

    const float ndcX = (2.0f * physX) / static_cast<float>(sz.width()) - 1.0f;
    const float ndcY = (2.0f * physY) / static_cast<float>(sz.height()) - 1.0f;

    const QMatrix4x4 invViewProj = (projectionMatrix() * m_camera.viewMatrix()).inverted();

    // Unproject the click point at the near and far planes -- a world-space
    // ray follows directly, self-consistent with whatever the camera is
    // doing without needing to separately reason about eye position/basis
    // vectors.
    QVector4D nearPoint4 = invViewProj * QVector4D(ndcX, ndcY, 0.0f, 1.0f);
    QVector4D farPoint4 = invViewProj * QVector4D(ndcX, ndcY, 1.0f, 1.0f);
    if (qFuzzyIsNull(nearPoint4.w()) || qFuzzyIsNull(farPoint4.w())) {
        return -1;
    }
    nearPoint4 /= nearPoint4.w();
    farPoint4 /= farPoint4.w();

    const QVector3D rayOrigin = nearPoint4.toVector3D();
    const QVector3D rayDir = (farPoint4.toVector3D() - nearPoint4.toVector3D()).normalized();

    int bestIndex = -1;
    float bestT = std::numeric_limits<float>::max();

    for (int i = 0; i < m_scene.count(); ++i) {
        // Ray, transformed into the Part's local space via the inverse of
        // its own modelMatrix() -- correct even once Parts can be rotated/
        // scaled, not just translated, since the test itself always runs
        // against the same canonical -0.5..0.5 unit-cube bounds.
        const QMatrix4x4 invModel = m_scene.part(i).modelMatrix().inverted();
        const QVector3D localOrigin = invModel.map(rayOrigin);
        // Directions transform as vectors (w=0), not points -- map() on a
        // QVector3D always treats it as a point (w=1), so this goes through
        // the 4-component form directly instead.
        const QVector3D localDir = (invModel * QVector4D(rayDir, 0.0f)).toVector3D();

        constexpr float kBoundsMin = -0.5f;
        constexpr float kBoundsMax = 0.5f;
        float tMin = 0.0f;
        float tMax = std::numeric_limits<float>::max();
        bool hit = true;

        const float origins[3] = { localOrigin.x(), localOrigin.y(), localOrigin.z() };
        const float dirs[3] = { localDir.x(), localDir.y(), localDir.z() };
        for (int axis = 0; axis < 3 && hit; ++axis) {
            if (qFuzzyIsNull(dirs[axis])) {
                if (origins[axis] < kBoundsMin || origins[axis] > kBoundsMax) {
                    hit = false;
                }
                continue;
            }
            float t0 = (kBoundsMin - origins[axis]) / dirs[axis];
            float t1 = (kBoundsMax - origins[axis]) / dirs[axis];
            if (t0 > t1) {
                std::swap(t0, t1);
            }
            tMin = std::max(tMin, t0);
            tMax = std::min(tMax, t1);
            if (tMin > tMax) {
                hit = false;
            }
        }

        if (hit && tMin < bestT) {
            bestT = tMin;
            bestIndex = i;
        }
    }

    return bestIndex;
}

void VulkanViewport::mousePressEvent(QMouseEvent *event) {
    m_activeButton = event->button();
    m_lastMousePos = event->position();

    if (event->button() == Qt::LeftButton) {
        m_scene.setSelectedIndex(pickPart(event->position()));
    }
}

void VulkanViewport::mouseMoveEvent(QMouseEvent *event) {
    const QPointF delta = event->position() - m_lastMousePos;
    m_lastMousePos = event->position();

    if (m_activeButton == Qt::RightButton) {
        m_camera.look(static_cast<float>(delta.x()), static_cast<float>(delta.y()));
    }
    // SceneRenderer::startNextFrame() already requests a repaint every frame
    // (the M1 continuous-present loop), so updated camera/selection state
    // just shows up on the next frame with no extra invalidation needed.
}

void VulkanViewport::mouseReleaseEvent(QMouseEvent * /*event*/) {
    m_activeButton = Qt::NoButton;
}

void VulkanViewport::wheelEvent(QWheelEvent *event) {
    m_camera.dolly(static_cast<float>(event->angleDelta().y()));
}

void VulkanViewport::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
    case Qt::Key_W:
    case Qt::Key_Up:
        m_moveForwardHeld = true;
        break;
    case Qt::Key_S:
    case Qt::Key_Down:
        m_moveBackHeld = true;
        break;
    case Qt::Key_A:
    case Qt::Key_Left:
        m_moveLeftHeld = true;
        break;
    case Qt::Key_D:
    case Qt::Key_Right:
        m_moveRightHeld = true;
        break;
    default:
        QVulkanWindow::keyPressEvent(event);
        break;
    }
}

void VulkanViewport::keyReleaseEvent(QKeyEvent *event) {
    switch (event->key()) {
    case Qt::Key_W:
    case Qt::Key_Up:
        m_moveForwardHeld = false;
        break;
    case Qt::Key_S:
    case Qt::Key_Down:
        m_moveBackHeld = false;
        break;
    case Qt::Key_A:
    case Qt::Key_Left:
        m_moveLeftHeld = false;
        break;
    case Qt::Key_D:
    case Qt::Key_Right:
        m_moveRightHeld = false;
        break;
    default:
        QVulkanWindow::keyReleaseEvent(event);
        break;
    }
}
