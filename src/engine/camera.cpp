#include "camera.h"

#include <QtMath>
#include <algorithm>

void Camera::look(float dx, float dy) {
    static constexpr float kSensitivity = 0.3f;
    m_yaw += dx * kSensitivity;
    m_pitch = std::clamp(m_pitch - dy * kSensitivity, kMinPitch, kMaxPitch);
}

QVector3D Camera::forward() const {
    const float yawRad = qDegreesToRadians(m_yaw);
    const float pitchRad = qDegreesToRadians(m_pitch);
    return QVector3D(
        std::cos(pitchRad) * std::cos(yawRad),
        std::sin(pitchRad),
        std::cos(pitchRad) * std::sin(yawRad)
    );
}

void Camera::moveForward(float amount) {
    m_eyePosition += forward() * amount;
}

void Camera::strafeRight(float amount) {
    const QVector3D worldUp(0.0f, 1.0f, 0.0f);
    const QVector3D right = QVector3D::crossProduct(forward(), worldUp).normalized();
    m_eyePosition += right * amount;
}

void Camera::dolly(float delta) {
    static constexpr float kSensitivity = 0.0025f;
    moveForward(delta * kSensitivity);
}

QMatrix4x4 Camera::viewMatrix() const {
    QMatrix4x4 view;
    view.lookAt(m_eyePosition, m_eyePosition + forward(), QVector3D(0.0f, 1.0f, 0.0f));
    return view;
}
