#pragma once
#include "scene/Scene.h"

namespace n3d {
// 40 x 25, alternating boxes and 64-sided cylinders, all within a 12 x 7.5 m site.
inline Scene performanceScene() {
    Scene scene;
    for(int z=0;z<25;++z) for(int x=0;x<40;++x) {
        ObjectData data;
        data.name="Benchmark "+std::to_string(z*40+x+1);
        const float height=200+float((x+z)%5)*50;
        if((x+z)%2) data.shape=CylinderParameters{200,height};
        else data.shape=BoxParameters{200,height,200};
        data.positionMm={float(x)*300-5850,0,float(z)*300-3600};
        scene.add(std::move(data));
    }
    return scene;
}
}
