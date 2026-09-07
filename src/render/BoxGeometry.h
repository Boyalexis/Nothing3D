#pragma once
#include <array>

struct BoxVertex { float position[3]; float color[3]; };

// Rendering units are metres: 2000 × 1500 × 1000 mm, bottom centre at origin.
// Each face has its own vertices so its colour stays flat at shared edges.
inline std::array<BoxVertex, 36> boxVertices()
{
    constexpr float p[8][3] = {
        {-1,0,-0.5f}, {1,0,-0.5f}, {1,1.5f,-0.5f}, {-1,1.5f,-0.5f},
        {-1,0,0.5f}, {1,0,0.5f}, {1,1.5f,0.5f}, {-1,1.5f,0.5f}
    };
    constexpr int faces[6][4] = {
        {4,5,6,7}, {1,0,3,2}, {5,1,2,6}, {0,4,7,3}, {7,6,2,3}, {0,1,5,4}
    };
    constexpr float colors[6][3] = {
        {0.90f,0.48f,0.16f}, {0.15f,0.65f,0.65f}, {0.22f,0.46f,0.78f},
        {0.55f,0.30f,0.72f}, {0.85f,0.75f,0.42f}, {0.32f,0.65f,0.28f}
    };
    constexpr int corners[6] = {0,1,2,0,2,3};
    std::array<BoxVertex,36> result{};
    for (int face=0; face<6; ++face)
        for (int i=0; i<6; ++i)
            for (int axis=0; axis<3; ++axis) {
                result[face*6+i].position[axis] = p[faces[face][corners[i]]][axis];
                result[face*6+i].color[axis] = colors[face][axis];
            }
    return result;
}
