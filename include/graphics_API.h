/*
=========== IKAROS Engine - Graphics API ===========

Author: Branislav Wilhelm
Started: April 5 2026 (5.4.2026)
Finished: April 5 2026 (5.4.2026)

Versions:
C++17
openGL 4.5
GLFW 3.x

note:
- using Direct State Acces (DSA)
*/

#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <array>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// Buffers
class Fence {
private:
	GLsync fence = 0;
public:
	~Fence();

	void place();
	bool signaled();
	bool wait(GLuint64 timeout);
	void discard();
};

struct Buffer {
private:
	GLuint id = 0;
	GLsizeiptr size = 0;
	void* ptr = nullptr;
	GLbitfield flags;
public:
	Buffer(const Buffer&) = delete;
	Buffer& operator=(const Buffer&) = delete;

	Buffer(GLsizeiptr size, GLbitfield flags = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
	~Buffer();

	void write(const void* source, GLsizeiptr offset, GLsizeiptr size) const;
	void read(void* destination, GLsizeiptr offset, GLsizeiptr size) const;

	void bind(GLenum target, GLuint index, GLsizeiptr offset, GLsizeiptr size) const;
	void bind(GLenum target) const;
	void unbind(GLenum target, GLuint index) const;
	void unbind(GLenum target) const;

	void resize(GLsizeiptr size);

	GLuint getID() const;
	GLsizeiptr getSize() const;
	void* getPtr() const;
};

// Shader Programs
namespace {
	struct Shader {
		GLuint id;
		GLenum type;
	};
}

class ShaderProgram {
private:
	GLuint id;
	std::vector<Shader> shaders;
public:
	ShaderProgram(const ShaderProgram&) = delete;
	ShaderProgram& operator=(const ShaderProgram&) = delete;

	ShaderProgram();
	~ShaderProgram();

	void addShader(GLenum shaderType, std::string shader);
	void compile();

	void useProgram() const;

	void runCompute(GLuint x, GLuint y, GLuint z, GLbitfield memoryBarrier = 0) const;

	GLuint getID() const;
};

// Vertex Arrays (VAOs)
class VAO {
private:
	GLuint id;
public:
	VAO(const VAO&) = delete;
	VAO& operator=(const VAO&) = delete;

	VAO();
	~VAO();

	void attributeI(GLuint index, GLint size, GLenum type, GLuint offset) const;
	void attribute(GLuint index, GLint size, GLenum type, GLboolean normalized, GLuint offset) const;
	void binding(GLuint index, GLuint buffer, GLintptr offset, GLsizei stride) const;
	void link(GLuint attributeIndex, GLuint bindingIndex) const;
	void divisor(GLuint bindingIndex, GLuint divisor) const;

	void draw(GLenum mode, GLsizei count) const;
	void drawElements(GLenum mode, GLsizei count, GLenum type, const void* offset, GLuint ebo) const;
	void drawInstanced(GLenum mode, GLsizei vertexCount, GLsizei instanceCount) const;
	void drawMultiIndirect(GLenum mode, GLsizei drawCount, const void* offset, GLsizei stride) const;
	GLuint getID() const;
};

// Windows
struct WindowFormat {
	std::string name = "IKAROS";
	int posX = 800;
	int posY = 600;
	int width = 800;
	int height = 600;

	int viewPosX = 0;
	int viewPosY = 0;
	int viewWidth = 800;
	int viewHeight = 600;

	int minWidth = GLFW_DONT_CARE;
	int minHeight = GLFW_DONT_CARE;
	int maxWidth = GLFW_DONT_CARE;
	int maxHeight = GLFW_DONT_CARE;

	bool maximized = false;
	bool fullscreen = false;
	bool floating = false;
	bool visible = true;
	bool focused = true;
	bool Vsync = true;

	bool resizable = true;
	bool borderless = false;

	uint8_t samples = 0;
	uint8_t depthBits = 24;
	uint8_t stencilBits = 8;
	uint8_t redBits = 8;
	uint8_t greenBits = 8;
	uint8_t blueBits = 8;
	uint8_t alphaBits = 8;
};

class Window {
private:
	GLFWwindow* context = nullptr;
	WindowFormat format;

	GLFWmonitor* getMonitor();
public:
	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;

	Window();
	Window(WindowFormat format);
	~Window();

	void setName(std::string name);
	void setPosition(float localX, float localY, int pixelX, int pixelY);
	void setSize(float localWidth, float localHeight, int pixelWidth, int pixelHeight);
	void setMinSize(float localWidth, float localHeight, int pixelWidth, int pixelHeight);
	void setMaxSize(float localWidth, float localHeight, int pixelWidth, int pixelHeight);

	void setMaximized(bool maximized);
	void setFullscreen(bool fullscreen);
	void setFloating(bool floating);
	void setVisible(bool visible);
	void setFocused(bool focused);
	void setVsync(bool Vsync);

	void setResizable(bool resizable);
	void setBorderless(bool borderless);

	void setSamples(uint8_t samples);
	void setDepthBits(uint8_t depthBits);
	void setStencilBits(uint8_t stencilBits);
	void setColorBits(uint8_t redBits, uint8_t greenBits, uint8_t blueBits, uint8_t alphaBits);

	void reload();
	void updateFormat();

	void setFormat(WindowFormat format);
	void setViewportPos(float localX, float localY, int pixelX, int pixelY);
	void setViewportSize(float localWidth, float localHeight, int pixelWidth, int pixelHeight);
	void setViewport();
	void setBackground(float r, float g, float b, float a);

	void clear(GLbitfield bitfield);
	void makeCurrentContext();
	void swapBuffers();

	bool shouldClose();
	const WindowFormat* getFormat() const;
	GLFWwindow* getContext();
};

Window* getPrimaryContext();
Window* getCurrentContext();

class Texture {
private:
	GLuint id = 0;
	GLuint64 handle = 0;
	int width = 0;
	int height = 0;
public:
	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;

	Texture(GLenum target, GLsizei levels, GLenum format, GLsizei width, GLsizei height);
	~Texture();

	void write(GLint level, GLint offsetX, GLint offsetY, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
	void setFilter(GLenum minFilter, GLenum magFilter);
	void setWrap(GLenum s, GLenum t);
	void load();
	void unload();

	GLuint getID();
	GLuint64 getHandle();
	int getWidth();
	int getHeight();
};