/*
IKAROS Engine - Graphics API Implementation
Author: Branislav Wilhelm
*/

#include "error_wrapper.h"
#include "graphics_API.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// =========== Buffer Manager ===========

// =========== ShaderManager ===========
static GLuint currentShader = 0;

// =========== VaoManager ===========

// =========== ContextManager ===========
static std::vector<Window*> contexts;
static Window* primaryContext = nullptr;
static Window* currentContext = nullptr;
static void replacePrimaryContext(Window* avoid) {
	for (Window* context : contexts) {
		if (context->getContext() != nullptr && context != avoid) {
			primaryContext = context;
			return;
		}
	}
}
static size_t getContextIndex() {
	size_t contextCount = contexts.size();
	for (size_t i = 0; i < contextCount; i++) {
		if (contexts[i] == nullptr)
			return i;
	}
	contexts.push_back(nullptr);
	return contextCount;
}
Window* getPrimaryContext() {
	return primaryContext;
}
Window* getCurrentContext() {
	return currentContext;
}

// =========== Fence ===========
Fence::~Fence() {
	discard();
}
void Fence::place() {
	errorMark;

	if (fence)
		return;

	fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}
bool Fence::signaled() {
	errorMark;

	if (!fence)
		return true;

	GLenum result = glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
	if (result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED) {
		glDeleteSync(fence);
		fence = 0;

		return true;
	}
	return false;
}
bool Fence::wait(GLuint64 timeout) {
	errorMark;

	if (!fence)
		return true;

	GLenum result = glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, timeout);
	if (result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED) {
		glDeleteSync(fence);
		fence = 0;

		return true;
	}
	return false;
}
void Fence::discard() {
	errorMark;

	if (!fence)
		return;

	glDeleteSync(fence);
	fence = 0;
}

// =========== Buffer ===========
Buffer::Buffer(GLsizeiptr size, GLbitfield flags) : flags(flags) {
	errorMark;
	if (size < 1) {
		Error(ENGINE_GRAPHICS, BUFFER, WARNING, SEVERITY_HIGH, OUT_OF_RANGE_VALUE, "invalid buffer size");
		return;
	}

	this->size = size;
	glCreateBuffers(1, &id);
	glNamedBufferStorage(id, size, nullptr, flags);
	ptr = glMapNamedBufferRange(id, 0, size, flags);
}
Buffer::~Buffer() {
	errorMark;

	glUnmapNamedBuffer(id);
	glDeleteBuffers(1, &id);
	id = 0;
	ptr = nullptr;
	size = 0;
}
void Buffer::write(const void* source, GLsizeiptr offset, GLsizeiptr size) const {
	errorMark;
	if (offset + size > this->size || size < 1 || offset < 0) {
		Error(ENGINE_GRAPHICS, BUFFER, WARNING, SEVERITY_LOW, OUT_OF_RANGE_VALUE, "attempted write to out of range location");
		return;
	}
	if (source)
		memcpy(static_cast<char*>(ptr) + offset, source, size);
}
void Buffer::read(void* destination, GLsizeiptr offset, GLsizeiptr size) const {
	errorMark;
	if (offset + size > this->size || offset < 0 || size < 1) {
		Error(ENGINE_GRAPHICS, BUFFER, WARNING, SEVERITY_LOW, OUT_OF_RANGE_VALUE, "attempted read from out of range location");
		return;
	}
	memcpy(destination, static_cast<char*>(ptr) + offset, size);
}
void Buffer::bind(GLenum target, GLuint index, GLsizeiptr offset, GLsizeiptr size) const {
	errorMark;
	glBindBufferRange(target, index, id, offset, size);
}
void Buffer::bind(GLenum target) const {
	errorMark;
	glBindBuffer(target, id);
}
void Buffer::unbind(GLenum target, GLuint index) const {
	errorMark;
	glBindBufferRange(target, index, 0, 0, 0);
}
void Buffer::unbind(GLenum target) const {
	errorMark;
	glBindBuffer(target, 0);
}
void Buffer::resize(GLsizeiptr size) {
	errorMark;
	if (size < 1) {
		Error(ENGINE_GRAPHICS, BUFFER, WARNING, SEVERITY_HIGH, OUT_OF_RANGE_VALUE, "invalid buffer size");
		return;
	}
	if (size != this->size) {
		GLuint newid;
		glCreateBuffers(1, &newid);

		glNamedBufferStorage(newid, size, nullptr, flags);

		void* newptr = glMapNamedBufferRange(newid, 0, size, flags);

		if (ptr && newptr)
			memcpy(newptr, ptr, std::min(this->size, size));

		if (id) {
			glUnmapNamedBuffer(id);
			glDeleteBuffers(1, &id);
		}

		ptr = newptr;
		id = newid;
		this->size = size;
	}
}
GLuint Buffer::getID() const {
	errorMark;
	return id;
}
GLsizeiptr Buffer::getSize() const {
	errorMark;
	return size;
}
void* Buffer::getPtr() const {
	errorMark;
	return ptr;
}

// =========== ShaderProgram ===========
ShaderProgram::ShaderProgram() {
	errorMark;

	id = glCreateProgram();
}
ShaderProgram::~ShaderProgram() {
	errorMark;

	glDeleteProgram(id);
	id = 0;
	shaders.clear();
	shaders.shrink_to_fit();
}
void ShaderProgram::addShader(GLenum shaderType, std::string shaderText) {
	errorMark;

	Shader shader;
	shader.type = shaderType;
	shader.id = glCreateShader(shaderType);

	const char* source = shaderText.c_str();
	glShaderSource(shader.id, 1, &source, nullptr);
	glCompileShader(shader.id);

	GLint compileStatus = GL_FALSE;
	glGetShaderiv(shader.id, GL_COMPILE_STATUS, &compileStatus);
	if (compileStatus != GL_TRUE) {
		GLint logLength = 0;
		glGetShaderiv(shader.id, GL_INFO_LOG_LENGTH, &logLength);
		std::string infoLog;
		if (logLength > 0) {
			infoLog.resize(static_cast<size_t>(logLength));
			glGetShaderInfoLog(shader.id, logLength, nullptr, &infoLog[0]);
		} else {
			infoLog = "Unknown compile error";
		}
		Error(ENGINE_GRAPHICS, SHADER_PROGRAM, ERROR, SEVERITY_HIGH, INVALID_VALUE, "shader compile failed: " + infoLog);
		glDeleteShader(shader.id);
		return;
	}

	glAttachShader(id, shader.id);
	shaders.push_back(shader);
}
void ShaderProgram::compile() {
	errorMark;

	glLinkProgram(id);

	GLint linkStatus = GL_FALSE;
	glGetProgramiv(id, GL_LINK_STATUS, &linkStatus);
	if (linkStatus != GL_TRUE) {
		GLint logLength = 0;
		glGetProgramiv(id, GL_INFO_LOG_LENGTH, &logLength);
		std::string infoLog;
		if (logLength > 0) {
			infoLog.resize(static_cast<size_t>(logLength));
			glGetProgramInfoLog(id, logLength, nullptr, &infoLog[0]);
		} else {
			infoLog = "Unknown link error";
		}
		Error(ENGINE_GRAPHICS, SHADER_PROGRAM, ERROR, SEVERITY_HIGH, INVALID_VALUE, "program link failed: " + infoLog);
	}

	for (auto& shader : shaders) {
		if (shader.id)
			glDeleteShader(shader.id);
		shader.id = 0;
	}
	shaders.clear();
	shaders.shrink_to_fit();
}
void ShaderProgram::useProgram() const {
	errorMark;

	if (currentShader == id)
		return;
	currentShader = id;
	glUseProgram(id);
}
void ShaderProgram::runCompute(GLuint x, GLuint y, GLuint z, GLbitfield memoryBarrier) const {
	errorMark;

	useProgram();
	glDispatchCompute(x, y, z);
	if (memoryBarrier)
		glMemoryBarrier(memoryBarrier);
}
GLuint ShaderProgram::getID() const {
	errorMark;
	return id;
}

// =========== VAO ===========
VAO::VAO() {
	errorMark;
	glCreateVertexArrays(1, &id);
}
VAO::~VAO() {
	errorMark;
	glDeleteVertexArrays(1, &id);
}
void VAO::attributeI(GLuint index, GLint size, GLenum type, GLuint offset) const {
	errorMark;
	glVertexArrayAttribIFormat(id, index, size, type, offset);
}
void VAO::attribute(GLuint index, GLint size, GLenum type, GLboolean normalized, GLuint offset) const {
	errorMark;
	glVertexArrayAttribFormat(id, index, size, type, normalized, offset);
}
void VAO::binding(GLuint index, GLuint buffer, GLintptr offset, GLsizei stride) const {
	errorMark;
	glVertexArrayVertexBuffer(id, index, buffer, offset, stride);
}
void VAO::link(GLuint attributeIndex, GLuint bindingIndex) const {
	errorMark;
	glVertexArrayAttribBinding(id, attributeIndex, bindingIndex);
	glEnableVertexArrayAttrib(id, attributeIndex);
}
void VAO::divisor(GLuint bindingIndex, GLuint divisor) const {
	errorMark;
	glVertexArrayBindingDivisor(id, bindingIndex, divisor);
}
void VAO::draw(GLenum mode, GLsizei count) const {
	errorMark;
	glBindVertexArray(id);
	glDrawArrays(mode, 0, count);
	glBindVertexArray(0);
}
void VAO::drawElements(GLenum mode, GLsizei count, GLenum type, const void* offset, GLuint ebo) const {
	errorMark;
	glVertexArrayElementBuffer(id, ebo);
	glBindVertexArray(id);
	glDrawElements(mode, count, type, offset);
	glBindVertexArray(0);
	glVertexArrayElementBuffer(id, 0);
}
void VAO::drawInstanced(GLenum mode, GLsizei vertexCount, GLsizei instanceCount) const {
	errorMark;
	glBindVertexArray(id);
	glDrawArraysInstanced(mode, 0, vertexCount, instanceCount);
	glBindVertexArray(0);
}
void VAO::drawMultiIndirect(GLenum mode, GLsizei drawCount, const void* offset, GLsizei stride) const {
	errorMark;
	glBindVertexArray(id);
	glMultiDrawArraysIndirect(mode, offset, drawCount, stride);
	glBindVertexArray(0);
}
GLuint VAO::getID() const {
	errorMark;
	return id;
}

// =========== Window ===========
GLFWmonitor* Window::getMonitor() {
	makeCurrentContext();
	errorMark;
	int monitorCount;
	GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

	for (int i = 0; i < monitorCount; i++) {
		int monitorX, monitorY;
		glfwGetMonitorPos(monitors[i], &monitorX, &monitorY);
		const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);

		if (format.posX >= monitorX && format.posX < (monitorX + mode->width) &&
			format.posY >= monitorY && format.posY < (monitorY + mode->height)) {
			return monitors[i];
		}
	}
	Error(ENGINE_GRAPHICS, WINDOW, ERROR, SEVERITY_HIGH, VALUE_NOT_FOUND, errorPosition, "monitor not found");
	return nullptr;
}
Window::Window() {
	reload();
}
Window::Window(WindowFormat format) : format(format) {
	reload();
}
Window::~Window() {
	errorMark;
	if (context) {
		if (currentContext == this)
			currentContext = nullptr;
		if (primaryContext == this) {
			Error(ENGINE_GRAPHICS, WINDOW, NOTIFICATION, SEVERITY_NONE, CHANGED_VALUE, errorPosition, "primary window reloaded");
			replacePrimaryContext(this);
		}
		glfwDestroyWindow(context);
	}
}
void Window::setName(std::string name) {
	format.name = name;
	errorMark;
	glfwSetWindowTitle(context, name.c_str());
}
void Window::setPosition(float localX, float localY, int pixelX, int pixelY) {
	errorMark;
	const GLFWvidmode* mode = glfwGetVideoMode(getMonitor());
	format.posX = pixelX + localX * mode->width;
	format.posY = pixelY + localY * mode->height;
	errorMark;
	glfwSetWindowPos(context, format.posX, format.posY);
}
void Window::setSize(float localWidth, float localHeight, int pixelWidth, int pixelHeight) {
	errorMark;
	const GLFWvidmode* mode = glfwGetVideoMode(getMonitor());
	int width = pixelWidth + localWidth * mode->width;
	int height = pixelHeight + localHeight * mode->height;
	if (width < 0 || height < 0) {
		Error(ENGINE_GRAPHICS, WINDOW, WARNING, SEVERITY_LOW, OUT_OF_RANGE_VALUE, errorPosition, "size too small");
		return;
	}
	format.width = width;
	format.height = height;
	errorMark;
	glfwSetWindowSize(context, format.width, format.height);
}
void Window::setMinSize(float localWidth, float localHeight, int pixelWidth, int pixelHeight) {
	errorMark;
	const GLFWvidmode* mode = glfwGetVideoMode(getMonitor());
	format.minWidth = pixelWidth + localWidth * mode->width;
	format.minHeight = pixelHeight + localHeight * mode->height;
	errorMark;
	glfwSetWindowSizeLimits(context,
		format.minWidth, format.minHeight,
		format.maxWidth, format.maxHeight);
}
void Window::setMaxSize(float localWidth, float localHeight, int pixelWidth, int pixelHeight) {
	errorMark;
	const GLFWvidmode* mode = glfwGetVideoMode(getMonitor());
	format.maxWidth = pixelWidth + localWidth * mode->width;
	format.maxHeight = pixelHeight + localHeight * mode->height;
	errorMark;
	glfwSetWindowSizeLimits(context,
		format.minWidth, format.minHeight,
		format.maxWidth, format.maxHeight);
}
void Window::setMaximized(bool maximized) {
	if (format.maximized == maximized)
		return;
	format.maximized = maximized;
	errorMark;
	glfwSetWindowAttrib(context, GLFW_MAXIMIZED, maximized ? GLFW_TRUE : GLFW_FALSE);
}
void Window::setFullscreen(bool fullscreen) {
	if (format.fullscreen == fullscreen)
		return;
	format.fullscreen = fullscreen;
	GLFWmonitor* monitor = getMonitor();
	errorMark;
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);
	glfwSetWindowMonitor(context,
		fullscreen ? monitor : nullptr,
		format.posX,
		format.posY,
		fullscreen ? mode->width : format.width,
		fullscreen ? mode->height : format.height,
		fullscreen ? mode->refreshRate : GL_DONT_CARE);
}
void Window::setFloating(bool floating) {
	if (format.floating == floating)
		return;
	format.floating = floating;
	errorMark;
	glfwSetWindowAttrib(context, GLFW_FLOATING, floating ? GLFW_TRUE : GLFW_FALSE);
}
void Window::setVisible(bool visible) {
	if (format.visible == visible)
		return;
	format.visible = visible;
	errorMark;
	glfwSetWindowAttrib(context, GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);
}
void Window::setFocused(bool focused) {
	if (format.focused == focused)
		return;
	format.focused = focused;
	errorMark;
	glfwSetWindowAttrib(context, GLFW_FOCUSED, focused ? GLFW_TRUE : GLFW_FALSE);
}
void Window::setVsync(bool Vsync) {
	if (format.Vsync == Vsync)
		return;
	format.Vsync = Vsync;
	makeCurrentContext();
	errorMark;
	glfwSwapInterval(format.Vsync);
}
void Window::setResizable(bool resizable) {
	if (format.resizable == resizable)
		return;
	format.resizable = resizable;
	errorMark;
	glfwSetWindowAttrib(context, GLFW_RESIZABLE, resizable ? GLFW_TRUE : GLFW_FALSE);
}
void Window::setBorderless(bool borderless) {
	if (format.borderless == borderless)
		return;
	format.borderless = borderless;
	errorMark;
	glfwSetWindowAttrib(context, GLFW_DECORATED, borderless ? GLFW_FALSE : GLFW_TRUE);
}
void Window::setSamples(uint8_t samples) {
	format.samples = samples;
}
void Window::setDepthBits(uint8_t depthBits) {
	format.depthBits = depthBits;
}
void Window::setStencilBits(uint8_t stencilBits) {
	format.stencilBits = stencilBits;
}
void Window::setColorBits(uint8_t redBits, uint8_t greenBits, uint8_t blueBits, uint8_t alphaBits) {
	format.redBits = redBits;
	format.greenBits = greenBits;
	format.blueBits = blueBits;
	format.alphaBits = alphaBits;
}
void Window::reload() {
	errorMark;
	if (!primaryContext)
		primaryContext = this;
	if (context) {
		if (currentContext->getContext() == context)
			currentContext = nullptr;
		if (primaryContext->getContext() == context) {
			Error(ENGINE_GRAPHICS, WINDOW, NOTIFICATION, SEVERITY_NONE, CHANGED_VALUE, errorPosition, "primary window reloaded");
			replacePrimaryContext(this);
		}
		glfwDestroyWindow(context);
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, format.resizable ? GLFW_TRUE : GLFW_FALSE);
	glfwWindowHint(GLFW_DECORATED, format.borderless ? GLFW_FALSE : GLFW_TRUE);
	glfwWindowHint(GLFW_SAMPLES, format.samples);
	glfwWindowHint(GLFW_DEPTH_BITS, format.depthBits);
	glfwWindowHint(GLFW_STENCIL_BITS, format.stencilBits);
	glfwWindowHint(GLFW_RED_BITS, format.redBits);
	glfwWindowHint(GLFW_GREEN_BITS, format.greenBits);
	glfwWindowHint(GLFW_BLUE_BITS, format.blueBits);
	glfwWindowHint(GLFW_ALPHA_BITS, format.alphaBits);

	context = glfwCreateWindow(
		format.width,
		format.height,
		format.name.c_str(),
		format.fullscreen ? getMonitor() : nullptr,
		primaryContext->getContext());

	makeCurrentContext();
	errorMark;
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		Error(ENGINE_GRAPHICS, WINDOW, ERROR, SEVERITY_HIGH, UNINITIALIZED_SOURCE, errorPosition, "GLAD not inicialized");
		return;
	}

	glfwSetWindowAttrib(context, GLFW_MAXIMIZED, format.maximized ? GLFW_TRUE : GLFW_FALSE);
	glfwSetWindowAttrib(context, GLFW_FLOATING, format.floating ? GLFW_TRUE : GLFW_FALSE);
	glfwSetWindowAttrib(context, GLFW_VISIBLE, format.visible ? GLFW_TRUE : GLFW_FALSE);
	glfwSetWindowAttrib(context, GLFW_FOCUSED, format.focused ? GLFW_TRUE : GLFW_FALSE);
	glfwSwapInterval(format.Vsync);
}
void Window::updateFormat() {
	errorMark;
	glfwGetWindowPos(context, &format.posX, &format.posY);
	glfwGetWindowSize(context, &format.width, &format.height);
	format.maximized = glfwGetWindowAttrib(context, GLFW_MAXIMIZED) == GLFW_TRUE;
	format.floating = glfwGetWindowAttrib(context, GLFW_FLOATING) == GLFW_TRUE;
	format.visible = glfwGetWindowAttrib(context, GLFW_VISIBLE) == GLFW_TRUE;
	format.focused = glfwGetWindowAttrib(context, GLFW_FOCUSED) == GLFW_TRUE;
	format.resizable = glfwGetWindowAttrib(context, GLFW_RESIZABLE) == GLFW_TRUE;
	format.borderless = glfwGetWindowAttrib(context, GLFW_DECORATED) == GLFW_FALSE;
	format.fullscreen = glfwGetWindowMonitor(context) != nullptr;
}
void Window::setFormat(WindowFormat format) {
	if (format.width < 0 || format.height < 0) {
		Error(ENGINE_GRAPHICS, WINDOW, WARNING, SEVERITY_LOW, OUT_OF_RANGE_VALUE, errorPosition, "invalid format");
		return;
	}
	this->format = format;
	reload();
}
void Window::setViewportPos(float localX, float localY, int pixelX, int pixelY) {
	format.viewPosX = pixelX + localX * format.width;
	format.viewPosY = pixelY + localY * format.height;
}
void Window::setViewportSize(float localWidth, float localHeight, int pixelWidth, int pixelHeight) {
	int width = pixelWidth + localWidth * format.width;
	int height = pixelHeight + localHeight * format.height;
	if (width < 0 || height < 0) {
		Error(ENGINE_GRAPHICS, WINDOW, WARNING, SEVERITY_LOW, OUT_OF_RANGE_VALUE, errorPosition, "viewport too small");
		return;
	}
	format.viewWidth = width;
	format.viewHeight = height;
}
void Window::setViewport() {
	makeCurrentContext();
	errorMark;
	glViewport(
		format.viewPosX,
		format.viewPosY,
		format.viewWidth,
		format.viewHeight);
}
void Window::setBackground(float r, float g, float b, float a) {
	makeCurrentContext();
	errorMark;
	glClearColor(r, g, b, a);
}
void Window::clear(GLbitfield bitfield) {
	makeCurrentContext();
	errorMark;
	glClear(bitfield);
}
void Window::makeCurrentContext() {
	errorMark;
	if (currentContext == this)
		return;
	currentContext = this;
	glfwMakeContextCurrent(context);
}
void Window::swapBuffers() {
	errorMark;
	glfwSwapBuffers(context);
}
bool Window::shouldClose() {
	errorMark;
	return glfwWindowShouldClose(context);
}
const WindowFormat* Window::getFormat() const {
	return &format;
}
GLFWwindow* Window::getContext() {
	return context;
}

// =========== Texture ===========
Texture::Texture(GLenum target, GLsizei levels, GLenum format, GLsizei width, GLsizei height) {
	glCreateTextures(GL_TEXTURE_2D, 1, &id);
	glTextureStorage2D(id, 1, GL_RGBA8, width, height);
	handle = glGetTextureHandleARB(id);
}
Texture::~Texture() {
	glDeleteTextures(1, &id);
}
void Texture::write(GLint level, GLint offsetX, GLint offsetY, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels) {
	glTextureSubImage2D(id, 0, 0, 0, width, height,
		GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}
void Texture::setFilter(GLenum minFilter, GLenum magFilter) {
	glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, minFilter);
	glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, magFilter);
}
void Texture::setWrap(GLenum s, GLenum t) {
	glTextureParameteri(id, GL_TEXTURE_WRAP_S, s);
	glTextureParameteri(id, GL_TEXTURE_WRAP_T, t);
}
void Texture::load() {
	glMakeTextureHandleResidentARB(handle);
}
void Texture::unload() {
	glMakeTextureHandleNonResidentARB(handle);
}
GLuint Texture::getID() {
	return id;
}
GLuint64 Texture::getHandle() {
	return handle;
}
int Texture::getWidth() {
	return width;
}
int Texture::getHeight() {
	return height;
}