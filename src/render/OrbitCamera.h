#pragma once
#include <QMatrix4x4>
#include <QtMath>
#include <algorithm>
#include <cmath>

class OrbitCamera {
public:
    enum class StandardView { Top, Front, Right };
    bool orthographic = false;
    float minimumDistance=2.5f,maximumDistance=30.0f,framingRadius=0;
    float yaw = 35, pitch = 25, distance = 5.5f;
    QVector3D target{0, 0.75f, 0};
    void reset() { *this = OrbitCamera{}; }
    void standardView(StandardView view) {
        orthographic = true;
        yaw = view == StandardView::Right ? 90.0f : 0.0f;
        pitch = view == StandardView::Top ? 90.0f : 0.0f;
    }
    QVector3D upDirection() const {
        const float y = qDegreesToRadians(yaw), p = qDegreesToRadians(pitch);
        return {-std::sin(y)*std::sin(p), std::cos(p), -std::cos(y)*std::sin(p)};
    }
    float visibleHeight() const { return 2*distance*std::tan(qDegreesToRadians(22.5f)); }
    QVector3D direction() const {
        const float y = qDegreesToRadians(yaw), p = qDegreesToRadians(pitch);
        return {std::sin(y)*std::cos(p), std::sin(p), std::cos(y)*std::cos(p)};
    }
    void orbit(float dx, float dy) {
        yaw = std::remainder(yaw-dx*0.4f, 360.0f);
        pitch = std::clamp(pitch+dy*0.4f, -85.0f, 85.0f);
    }
    void zoom(float steps) { distance = std::clamp(distance*std::pow(0.85f, steps), minimumDistance, maximumDistance); }
    void pan(float dx, float dy, int height) {
        const auto forward = -direction();
        const auto right = QVector3D::crossProduct(forward, upDirection()).normalized();
        const auto up = QVector3D::crossProduct(right, forward).normalized();
        const float scale = 2*distance*std::tan(qDegreesToRadians(45.0f/2))/float(std::max(1,height));
        target += (-right*dx+up*dy)*scale;
    }
    QMatrix4x4 view() const {
        QMatrix4x4 result;
        result.lookAt(target+direction()*distance, target, upDirection());
        return result;
    }
    QMatrix4x4 projection(float aspect) const {
        QMatrix4x4 result;
        const float nearPlane=framingRadius>0 ? std::max(1e-7f,std::min(distance*.01f,framingRadius*.001f)) : .1f;
        const float farPlane=framingRadius>0 ? distance+framingRadius*4 : 100.0f;
        if (orthographic) {
            const float halfHeight = visibleHeight()/2;
            const float halfWidth = halfHeight*std::max(aspect,0.01f);
            result.ortho(-halfWidth,halfWidth,-halfHeight,halfHeight,nearPlane,farPlane);
        } else result.perspective(45.0f, std::max(aspect,0.01f), nearPlane, farPlane);
        return result;
    }
};
