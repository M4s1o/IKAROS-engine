#version 460 core

in vec3 vertNormal;
in vec4 vertColor;

out vec4 color;

void main() {
	color = vertColor;
}