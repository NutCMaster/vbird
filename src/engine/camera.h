#pragma once

#include <QMatrix4x4>
#include <QVector3D>

// Fly camera: an eye position plus a yaw/pitch facing direction. Pure math,
// no Vulkan/Qt-window dependency -- VulkanViewport owns one and drives it
// from mouse/keyboard events.
class Camera {
public:
    void look(float dx, float dy);
    void dolly(float delta);
    void moveForward(float amount);
    void strafeRight(float amount);

    QVector3D forward() const;
    QMatrix4x4 viewMatrix() const;

private:
    float m_yaw = -90.0f;
    float m_pitch = -15.0f;
    QVector3D m_eyePosition{ 0.0f, 1.5f, 5.0f };

    static constexpr float kMinPitch = -89.0f;
    static constexpr float kMaxPitch = 89.0f;
};
