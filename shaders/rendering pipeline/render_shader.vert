#version 460 core

layout(location = 0) in vec3 vertex_position;
layout(location = 1) in vec3 vertex_normal;
layout(location = 2) in vec4 vertex_color;
layout(location = 3) in mat4 part_transform_matrix;

uniform mat4 camera_view_matrix;
uniform mat4 camera_projection_matrix;

out vec3 vertNormal;
out vec4 vertColor;

void main() {
	gl_Position = camera_projection_matrix * camera_view_matrix * part_transform_matrix * vec4(vertex_position, 1.0);
	vertNormal = vertex_normal;
	vertColor = vertex_color;
}