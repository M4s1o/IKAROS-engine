/*
============ IKAROS Engine - Entity component system ===========

Author: Branislav Wilhelm
Started : May 17 2026 (17.5.2026)
Finished : 

Versions :
	C++17

note :
*/

#pragma once

#include "graphics_API.h"
#include "graphics_API_extension.h"
#include "error_wrapper.h"

#include <glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <gtx/quaternion.hpp>
#include <gtc/type_ptr.hpp>

// Components

struct Transform3D {
	glm::vec3 scale = { 1, 1, 1 }; // coeficient
	glm::vec3 position = { 0, 0, 0 };
	glm::quat rotation;

	glm::mat4 getTransformMatrix() {
		glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);

		glm::mat4 rotationMatrix = glm::toMat4(rotation);

		glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);

		return translationMatrix * rotationMatrix * scaleMatrix;
	}
};
struct Transform2D {
	glm::vec2 scale; // coeficient
	glm::vec2 position;
	float rotation;
};

struct Motion {
	glm::vec3 velocity;
	glm::quat angularVelocity;
};

struct OutsideForces {
	glm::vec3 acceleration;
	float drag;
};

struct MaterialProperties {
	float density; // kg/m^3
	float staticFriction; // coeficient
	float dynamicFriction; // coeficient
};

struct RigidBody {
	float mass; // kg
	float restitution; // coeficient
};

// something

class Mesh {
private:
	GLsizeiptr offset;
	GLsizeiptr size;
public:
	void triangle(glm::vec3 vertex1, glm::vec3 vertex2, glm::vec3 vertex3, glm::vec4 color);
	void colorTriangle(glm::vec3 vertexPos1, glm::vec3 vertexPos2, glm::vec3 vertexPos3, glm::vec4 color1, glm::vec4 color2, glm::vec4 color3);

	void rectangle(glm::vec3 position, glm::quat rotation, glm::vec2 size, glm::vec4 color);
	void circle(glm::vec3 position, glm::quat rotation, glm::vec2 size, glm::vec4 color, int segments);

	void cube(glm::vec3 position, glm::quat rotation, glm::vec3 size, glm::vec4 color);
	void sphere(glm::vec3 position, glm::quat rotation, glm::vec3 size, glm::vec4 color, int segments);
};

class Camera {
public:
	Transform3D transform;
	float fov = 90;
	float aspectRatio = 1;
	float nearPlane = 0.02;
	float farPlane = 10000;

	void sendUniform(GLuint shaderProgram) {
		glm::mat4 tranformMatrix = glm::inverse(transform.getTransformMatrix());
		glm::mat4 projectionMatrix = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "cameraTransformMatrix"), 1, GL_FALSE, glm::value_ptr(tranformMatrix));
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "cameraProjectionMatrix"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));
	};
	void move(glm::vec3 translation) {
		translation = glm::rotate(transform.rotation, translation);
		transform.position += translation;
	}
};