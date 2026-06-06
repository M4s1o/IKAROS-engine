#version 460 core

layout(std430, binding = 4) buffer partMatrixBuffer {
	mat4 part_transform_matrix[];
};
layout(std430, binding = 6) buffer referenceBuffer {
	uint actual_index[];
};

layout(location = 0) in vec3 vertex_position;
layout(location = 1) in vec3 vertex_normal;
layout(location = 2) in vec4 vertex_color;

uniform mat4 camera_view_matrix;
uniform mat4 camera_projection_matrix;

out vec3 vertNormal;
out vec4 vertColor;

void main() {
	uint id = actual_index[gl_BaseInstance + gl_InstanceID];

	gl_Position = camera_projection_matrix * camera_view_matrix * part_transform_matrix[id] * vec4(vertex_position, 1.0);
	vertNormal = vertex_normal;
	vertColor = vertex_color;
}