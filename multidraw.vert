#version 460 core

layout(std430, binding = 0) buffer offsetBuffer {
    vec2 offset[];
};

layout(location = 0) in vec2 aPos;

void main() {
    uint id = gl_BaseInstance + gl_InstanceID;
    gl_Position = vec4(aPos + offset[id], 0.0, 1.0);
    gl_PointSize = 10.0;
}
