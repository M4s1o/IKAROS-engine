#version 460 core

in vec3 vertNormal;
in vec4 vertColor;

out vec4 color;

void main() {
	vec3 normal = normalize(vertNormal);
	color = vec4(vertColor.xyz * (dot(vec3(0, 1, 0), normal) + 1) / 2, vertColor.a);
}