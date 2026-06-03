#version 460 core
out vec4 color;

void main() {
    vec2 pointCoord = gl_PointCoord - vec2(0.5);
    if (dot(pointCoord, pointCoord) > 0.25) {
        discard;
    }

    color = vec4(0.2, 0.8, 1.0, 1.0);
}
