#pragma once
#include <QMatrix4x4>
#include <QtMath>
#include <algorithm>
#include <cmath>

class OrbitCamera {
public:
    float yaw = 35, pitch = 25, distance = 5.5f;
    QVector3D target{0, 0.75f, 0};
    void reset() { *this = OrbitCamera{}; }
    QVector3D direction() const {
        const float y = qDegreesToRadians(yaw), p = qDegreesToRadians(pitch);
        return {std::sin(y)*std::cos(p), std::sin(p), std::cos(y)*std::cos(p)};
    }
    void orbit(float dx, float dy) {
        yaw = std::remainder(yaw-dx*0.4f, 360.0f);
        pitch = std::clamp(pitch+dy*0.4f, -85.0f, 85.0f);
    }
    void zoom(float steps) { distance = std::clamp(distance*std::pow(0.85f, steps), 2.5f, 30.0f); }
    void pan(float dx, float dy, int height) {
        const auto forward = -direction();
        const auto right = QVector3D::crossProduct(forward, {0,1,0}).normalized();
        const auto up = QVector3D::crossProduct(right, forward).normalized();
        const float scale = 2*distance*std::tan(qDegreesToRadians(45.0f/2))/float(std::max(1,height));
        target += (-right*dx+up*dy)*scale;
    }
    QMatrix4x4 view() const {
        QMatrix4x4 result;
        result.lookAt(target+direction()*distance, target, {0,1,0});
        return result;
    }
    QMatrix4x4 projection(float aspect) const {
        QMatrix4x4 result;
        result.perspective(45.0f, std::max(aspect,0.01f), 0.1f, 100.0f);
        return result;
    }
};
