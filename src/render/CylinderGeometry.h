#pragma once
#include "render/BoxGeometry.h"
#include <vector>
#include <cmath>

// Closed demo cylinder: 800 mm diameter, 1400 mm height, Y-up.
inline std::vector<BoxVertex> cylinderVertices() {
    constexpr int segments=64;
    constexpr float radius=0.4f,height=1.4f;
    std::vector<BoxVertex> mesh;
    mesh.reserve(segments*12);
    auto triangle=[&](BoxVertex a,BoxVertex b,BoxVertex c) { mesh.insert(mesh.end(),{a,b,c}); };
    for(int i=0;i<segments;++i) {
        const float a=float(i)*6.283185307f/segments,b=float(i+1)*6.283185307f/segments;
        // Fixed illustrative face shading, not a lighting system.
        const float shade=0.65f+0.25f*std::cos((a+b)*0.5f-0.8f);
        auto point=[&](float angle,float y,float r,float g,float blue) {
            return BoxVertex{{radius*std::cos(angle),y,radius*std::sin(angle)},{r,g,blue}};
        };
        const auto p=point(a,0,0.20f*shade,0.80f*shade,0.87f*shade);
        const auto q=point(b,0,0.20f*shade,0.80f*shade,0.87f*shade);
        const auto r=point(a,height,0.20f*shade,0.80f*shade,0.87f*shade);
        const auto s=point(b,height,0.20f*shade,0.80f*shade,0.87f*shade);
        triangle(p,r,s); triangle(p,s,q);
        triangle({{0,height,0},{0.48f,0.88f,0.92f}},point(b,height,0.48f,0.88f,0.92f),point(a,height,0.48f,0.88f,0.92f));
        triangle({{0,0,0},{0.12f,0.38f,0.43f}},point(a,0,0.12f,0.38f,0.43f),point(b,0,0.12f,0.38f,0.43f));
    }
    return mesh;
}
inline std::vector<BoxVertex> demoVertices() {
    const auto box=boxVertices();
    std::vector<BoxVertex> mesh(box.begin(),box.end());
    auto cylinder=cylinderVertices();
    for(auto& vertex : cylinder) vertex.position[0]-=1.6f;
    mesh.insert(mesh.end(),cylinder.begin(),cylinder.end());
    return mesh;
}
