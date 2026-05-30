/*
IKAROS Engine - Graphics API extension Implementation
Author: Branislav Wilhelm
*/

#include "graphics_API_extension.h"
#include "error_wrapper.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

std::string readFileToString(const std::string& shaderFilePath) {
	std::ifstream file(shaderFilePath);
	if (!file.is_open()) {
		Error(MISC, HELPER_FUNC, ERROR, SEVERITY_LOW, FILE_NOT_OPEN, "couldn't open file");
		return std::string();
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

// =========== Allocation Pool ===========
bool AllocationPool::isAllocValid(AllocHandle handle) {
	if (handle.index > allocations.size() - 1)
		return false;
	return handle.generation == allocations[handle.index].generation && allocations[handle.index].alive;
}
size_t AllocationPool::getFreeAllocIndex() {
	for (size_t i = 0; i < allocations.size(); i++) {
		if (!allocations[i].alive) {
			return i;
		}
	}
	size_t index = allocations.size();
	allocations.push_back(Allocation());
	return index;
}
GLsizeiptr AllocationPool::align(GLsizeiptr head, GLsizeiptr alignment) {
	return (head + alignment - 1) / alignment * alignment;
}
AllocationPool::AllocationPool(GLsizeiptr size, GLbitfield flags)
	: buffer(size, flags), allocations(1), head(0), used(0) {}
AllocationPool::AllocationPool(GLsizeiptr size)
	: buffer(size), allocations(1), head(0), used(0) {}
AllocationPool::~AllocationPool() {
	for (Allocation allocation : allocations) {
		if (!allocation.alive)
			continue;
		allocation.alive = false;
		allocation.generation++;
		allocation.alignment = 1;
		allocation.offset = 0;
		allocation.size = 0;
	}
}
void AllocationPool::rebuild() {
	errorMark;
	// IMPLEMENT
}
void AllocationPool::createAlloc(AllocHandle &handle, GLsizeiptr size, GLsizeiptr alignment) {
	errorMark;
	if (isAllocValid(handle)) {
		Error(ENGINE_GRAPHICS, ALLOCATION_POOL, WARNING, SEVERITY_MEDIUM, INVALID_HANDLE, "cannot rewrite valid handle");
		return;
	}
	if (size < 1) {
		Error(ENGINE_GRAPHICS, ALLOCATION_POOL, WARNING, SEVERITY_HIGH, OUT_OF_RANGE_VALUE, "size cannot be smaller than 1 byte");
		return;
	}
	if (alignment < 1) {
		Error(ENGINE_GRAPHICS, ALLOCATION_POOL, WARNING, SEVERITY_HIGH, OUT_OF_RANGE_VALUE, "alignment cannot be smaller than 1 byte");
		return;
	}
	handle.index = getFreeAllocIndex();
	handle.generation = allocations[handle.index].generation;

	allocations[handle.index].alive = true;
	allocations[handle.index].alignment = alignment;
	allocations[handle.index].offset = align(head, alignment);
	allocations[handle.index].size = size;

	head = allocations[handle.index].offset + allocations[handle.index].size;
}
void AllocationPool::freeAlloc(AllocHandle &handle) {
	errorMark;
	if (!isAllocValid(handle)) {
		Error(ENGINE_GRAPHICS, ALLOCATION_POOL, WARNING, SEVERITY_MEDIUM, INVALID_HANDLE, "cannot free invalid allocation");
		return;
	}
	allocations[handle.index].alive = false;
	allocations[handle.index].generation++;
	allocations[handle.index].alignment = 1;
	allocations[handle.index].offset = 0;
	allocations[handle.index].size = 0;

	handle = {};
}
void AllocationPool::bind(AllocHandle handle, GLenum target, GLuint index, GLsizeiptr offset, GLsizeiptr size) {
	errorMark;
	if (!isAllocValid(handle)) {
		Error(ENGINE_GRAPHICS, ALLOCATION_POOL, ERROR, SEVERITY_HIGH, INVALID_HANDLE, "data handle invalid");
		return;
	}
	if (offset + size > allocations[handle.index].size || size < 1 || offset < 0) {
		Error(ENGINE_GRAPHICS, ALLOCATION_POOL, WARNING, SEVERITY_LOW, OUT_OF_RANGE_VALUE, "attempted write to out of range location");
		return;
	}
	buffer.bind(target, index, allocations[handle.index].offset + offset, size);
}
void AllocationPool::bind(AllocHandle handle, GLenum target, GLuint index) {
	errorMark;
	if (!isAllocValid(handle)) {
		Error(ENGINE_GRAPHICS, ALLOCATION_POOL, ERROR, SEVERITY_HIGH, INVALID_HANDLE, "data handle invalid");
		return;
	}
	buffer.bind(target, index, allocations[handle.index].offset, allocations[handle.index].size);
}
void AllocationPool::unbind(AllocHandle handle, GLenum target, GLuint index) {
	errorMark;
	if (!isAllocValid(handle)) {
		Error(ENGINE_GRAPHICS, ALLOCATION_POOL, ERROR, SEVERITY_HIGH, INVALID_HANDLE, "data handle invalid");
		return;
	}
	buffer.unbind(target, index);
}
void AllocationPool::writeAlloc(AllocHandle handle, const void* source, GLsizeiptr offset, GLsizeiptr size) {
	errorMark;
	if (!isAllocValid(handle)) {
		Error(ENGINE_GRAPHICS, ALLOCATION_POOL, ERROR, SEVERITY_HIGH, INVALID_HANDLE, "data handle invalid");
		return;
	}
	if (offset + size > allocations[handle.index].size || size < 1 || offset < 0) {
		Error(ENGINE_GRAPHICS, ALLOCATION_POOL, WARNING, SEVERITY_LOW, OUT_OF_RANGE_VALUE, "attempted write to out of range location");
		return;
	}
	buffer.write(source, allocations[handle.index].offset + offset, size);
}
void AllocationPool::readAlloc(AllocHandle handle, void* destination, GLsizeiptr offset, GLsizeiptr size) {
	errorMark;
	if (!isAllocValid(handle)) {
		Error(ENGINE_GRAPHICS, ALLOCATION_POOL, ERROR, SEVERITY_HIGH, INVALID_HANDLE, "data handle invalid");
		return;
	}
	if (offset + size > allocations[handle.index].size || offset < 0 || size < 1) {
		Error(ENGINE_GRAPHICS, ALLOCATION_POOL, WARNING, SEVERITY_LOW, OUT_OF_RANGE_VALUE, "attempted read from out of range location");
		return;
	}
	buffer.read(destination, allocations[handle.index].offset + offset, size);
}
GLsizeiptr AllocationPool::getAllocSize(AllocHandle handle) {
	errorMark;
	if (!isAllocValid(handle)) {
		Error(ENGINE_GRAPHICS, ALLOCATION_POOL, ERROR, SEVERITY_HIGH, INVALID_HANDLE, "data handle invalid");
		return GL_INVALID_VALUE;
	}
	return allocations[handle.index].size;
}
GLsizeiptr AllocationPool::getAllocOffset(AllocHandle handle) {
	errorMark;
	if (!isAllocValid(handle)) {
		Error(ENGINE_GRAPHICS, ALLOCATION_POOL, ERROR, SEVERITY_HIGH, INVALID_HANDLE, "data handle invalid");
		return GL_INVALID_VALUE;
	}
	return allocations[handle.index].offset;
}
void* AllocationPool::getAllocPtr(AllocHandle handle) {
	errorMark;
	if (!isAllocValid(handle)) {
		Error(ENGINE_GRAPHICS, ALLOCATION_POOL, ERROR, SEVERITY_HIGH, INVALID_HANDLE, "data handle invalid");
		return nullptr;
	}
	return static_cast<char*>(buffer.getPtr()) + allocations[handle.index].offset;
}
GLsizeiptr AllocationPool::getSize() const {
	return buffer.getSize();
}
GLsizeiptr AllocationPool::getUsed() const {
	return used;
}
GLuint AllocationPool::getID() const {
	return buffer.getID();
}