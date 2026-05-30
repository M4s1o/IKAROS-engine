/*
============ IKAROS Engine - OpenGL Wrapper ===========

Author: Branislav Wilhelm
Started : March 30 2026 (25.3.2026)
Finished : unknown

	Versions :
	C++17
	openGL 4.5
	GLFW 3.x

	note :
-using Direct State Acces(DSA)
*/

#pragma once
#include "graphics_API.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <vector>
#include <array>
#include <string>
#include <iostream>
#include <fstream>

// Helpers
std::string readFileToString(const std::string& shaderFilePath);

// Allocation
class AllocHandle {
	size_t index = 0;
	size_t generation = 0;
	friend class AllocationPool;
};

struct Allocation {
	bool alive = false;
	size_t generation = 1;

	GLsizeiptr alignment = 1;
	GLsizeiptr offset = 0;
	GLsizeiptr size = 0;
};

class AllocationPool {
private:
	Buffer buffer;
	std::vector<Allocation> allocations;
	GLsizeiptr head;
	GLsizeiptr used;

	bool isAllocValid(AllocHandle handle);
	size_t getFreeAllocIndex();
	GLsizeiptr align(GLsizeiptr head, GLsizeiptr alignment);
public:
	AllocationPool(GLsizeiptr size, GLbitfield flags);
	AllocationPool(GLsizeiptr size);
	~AllocationPool();

	void rebuild();

	void createAlloc(AllocHandle &handle, GLsizeiptr size, GLsizeiptr alignment = 1);
	void freeAlloc(AllocHandle &handle);

	void bind(AllocHandle handle, GLenum target, GLuint index, GLsizeiptr offset, GLsizeiptr size);
	void bind(AllocHandle handle, GLenum target, GLuint index);
	void unbind(AllocHandle handle, GLenum target, GLuint index);

	void writeAlloc(AllocHandle handle, const void* source, GLsizeiptr offset, GLsizeiptr size);
	void readAlloc(AllocHandle handle, void* destination, GLsizeiptr offset, GLsizeiptr size);

	GLsizeiptr getAllocSize(AllocHandle handle);
	GLsizeiptr getAllocOffset(AllocHandle handle);
	void* getAllocPtr(AllocHandle handle);

	GLsizeiptr getSize() const;
	GLsizeiptr getUsed() const;
	GLuint getID() const;
};

