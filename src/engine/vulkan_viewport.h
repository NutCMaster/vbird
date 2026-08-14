#pragma once

#include <QElapsedTimer>
#include <QMatrix4x4>
#include <QPointF>
#include <QVulkanWindow>

#include "camera.h"
#include "scene.h"

class VulkanViewport : public QVulkanWindow {
public:
    explicit VulkanViewport(QVulkanInstance *instance);

    QVulkanWindowRenderer *createRenderer() override;

    const Camera &camera() const { return m_camera; }
    Scene &scene() { return m_scene; }
    const Scene &scene() const { return m_scene; }

    // (perspective(45, aspect, ...) corrected for Vulkan's clip-space
    // conventions) -- shared by SceneRenderer's per-frame draw and the
    // click-to-select ray-picker below, so the two can never disagree about
    // what's actually on screen. Not const: QVulkanWindow::clipCorrectionMatrix()
    // isn't const either.
    QMatrix4x4 projectionMatrix();

    // Called once per frame by SceneRenderer::startNextFrame(), before it
    // reads the camera, to apply any held WASD/arrow-key movement scaled by
    // real elapsed time.
    void updateMovement();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    // Ray-casts from a click position against every Part's oriented bounding
    // box (via the inverse of its own modelMatrix(), so this stays correct
    // once Parts can be rotated/scaled). Returns the closest hit index, or
    // -1 if nothing was hit.
    int pickPart(const QPointF &pos);

    Camera m_camera;
    Scene m_scene;
    QPointF m_lastMousePos;
    Qt::MouseButton m_activeButton = Qt::NoButton;

    bool m_moveForwardHeld = false;
    bool m_moveBackHeld = false;
    bool m_moveLeftHeld = false;
    bool m_moveRightHeld = false;
    QElapsedTimer m_movementTimer;
};
