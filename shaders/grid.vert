#version 450
layout(location=0) in vec3 position;
layout(location=0) out vec3 nearPoint;
layout(location=1) out vec3 farPoint;
layout(push_constant) uniform Camera { mat4 inverseVP; mat4 vp; } camera;
void main() {
    gl_Position=vec4(position.xy,0,1);
    vec4 a=camera.inverseVP*vec4(position.xy,0,1);
    vec4 b=camera.inverseVP*vec4(position.xy,1,1);
    nearPoint=a.xyz/a.w; farPoint=b.xyz/b.w;
}
