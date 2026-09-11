#version 450
layout(constant_id=0) const int gridMode=0; // 0 both, 1 grid only, 2 axes only
layout(location=0) in vec3 nearPoint;
layout(location=1) in vec3 farPoint;
layout(location=0) out vec4 outColor;
layout(push_constant) uniform Camera { mat4 inverseVP; mat4 vp; } camera;
float lines(vec2 p,float spacing) {
    vec2 q=p/spacing;
    vec2 footprint=max(fwidth(q),vec2(0.00001));
    vec2 distance=abs(fract(q-0.5)-0.5)/footprint;
    float coverage=1.0-min(min(distance.x,distance.y),1.0);
    return coverage*(1.0-smoothstep(0.35,1.0,max(footprint.x,footprint.y)));
}
void main() {
    float dy=farPoint.y-nearPoint.y;
    if(abs(dy)<0.000001) discard;
    float t=-nearPoint.y/dy;
    if(t<=0.0 || t>=1.0) discard;
    vec3 p=mix(nearPoint,farPoint,t);
    vec4 clip=camera.vp*vec4(p,1);
    float depth=clip.z/clip.w;
    if(depth<0.0 || depth>1.0) discard;
    gl_FragDepth=depth;
    float small=lines(p.xz,0.1);
    float medium=lines(p.xz,1.0);
    float large=lines(p.xz,10.0);
    float alpha=max(small*0.16,max(medium*0.32,large*0.48));
    if(gridMode==2) alpha=0;
    vec3 color=vec3(0.42,0.49,0.58);
    vec2 width=max(fwidth(p.xz),vec2(0.00001));
    float xAxis=1.0-smoothstep(0.5,1.5,abs(p.z)/width.y);
    float zAxis=1.0-smoothstep(0.5,1.5,abs(p.x)/width.x);
    if(gridMode==1) { xAxis=0; zAxis=0; }
    if(xAxis>0) { color=mix(color,vec3(0.95,0.22,0.22),xAxis); alpha=max(alpha,xAxis); }
    if(zAxis>0) { color=mix(color,vec3(0.25,0.55,1),zAxis); alpha=max(alpha,zAxis); }
    // No world-space boundary: fade by view distance and screen-space density.
    alpha*=1.0-smoothstep(35.0,95.0,length(p-nearPoint));
    outColor=vec4(color,alpha);
}
