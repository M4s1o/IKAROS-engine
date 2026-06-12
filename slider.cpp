#include "graphics_API.h"
#include "graphics_API_extension.h"
#include "ecs.h"

#include <chrono>
#include <random>
#include <ctime>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

std::chrono::duration<float> deltaTimeT;
float FPST;
std::chrono::duration<float> elapsedT;
std::chrono::duration<float> lastElapsedT;
std::chrono::steady_clock::time_point start;

static constexpr size_t FPS_SMOOTH_WINDOW = 1000;
static std::vector<float> frameTimes;
static size_t framePos = 0;
static float frameTimeSum = 0.0f;

void timeSetup() {
	start = std::chrono::steady_clock::now();
	elapsedT = start - start;
	lastElapsedT = start - start;
	deltaTimeT = elapsedT - lastElapsedT;

	// initialize smoothing buffer with a reasonable default (60 FPS)
	const float defaultDt = 1.0f / 60.0f;
	frameTimes.assign(FPS_SMOOTH_WINDOW, defaultDt);
	framePos = 0;
	frameTimeSum = defaultDt * static_cast<float>(FPS_SMOOTH_WINDOW);
	FPST = 1.0f / defaultDt;
}
void timeUpdate() {
	lastElapsedT = elapsedT;
	elapsedT = std::chrono::steady_clock::now() - start;

	// instantaneous delta for simulation
	deltaTimeT = elapsedT - lastElapsedT;

	// update smoothing ring buffer and sum
	float newDt = deltaTimeT.count();
	frameTimeSum -= frameTimes[framePos];
	frameTimes[framePos] = newDt;
	frameTimeSum += frameTimes[framePos];
	framePos = (framePos + 1) % FPS_SMOOTH_WINDOW;

	// smoothed FPS (average of last N frame times)
	float avgDt = frameTimeSum / static_cast<float>(FPS_SMOOTH_WINDOW);
	if (avgDt > 0.0f)
		FPST = 1.0f / avgDt;
	else
		FPST = 0.0f;
}
float FPS() {
	return FPST;
}
float deltaTime() {
	return deltaTimeT.count();
}
float elapsed() {
	return elapsedT.count();
}
float lastFrameElapsed() {
	return lastElapsedT.count();
}

Mesh* sliderMesh = nullptr;
Mesh* handleMesh = nullptr;

struct Slider {
	Part handle;
	Part slide;
	glm::vec2 position;
	float maximum = 0.6;
	float value = 0;

	bool dragged = false;

	Slider() {
		slide.setMesh(*sliderMesh);
		handle.setMesh(*handleMesh);

		position = { -0.3, 0 };

		slide.transform.position = { position.x, position.y, 0 };
		handle.transform.position = { position.x, position.y, 0.1 };

		slide.transform.scale = { maximum, 0.1, 1 };
		handle.transform.scale = { 0.04, 0.04, 1 };

		slide.syncToBuffer();
		handle.syncToBuffer();
	}

	void exec(Window &window) {
		glm::dvec2 mousePos = {};
		glfwGetCursorPos(window.getContext(), &mousePos.x, &mousePos.y);
		mousePos /= glm::vec2(window.getFormat()->width, window.getFormat()->height);

		mousePos.y = (1.0f - mousePos.y) * 2 - 1;
		mousePos.x = mousePos.x * 2 - 1;

		if (glfwGetMouseButton(window.getContext(), GLFW_MOUSE_BUTTON_LEFT) != GLFW_PRESS)
			dragged = false;
		else if (glfwGetMouseButton(window.getContext(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
			if (mousePos.x > handle.transform.position.x - handle.transform.scale.x &&
				mousePos.x < handle.transform.position.x + handle.transform.scale.x &&
				mousePos.y > handle.transform.position.y - handle.transform.scale.y &&
				mousePos.y < handle.transform.position.y + handle.transform.scale.y) {
				dragged = true;
			}
		}

		if (dragged) {
			float distance = mousePos.x - position.x;

			if (distance < 0)
				value = 0;
			else if (distance > maximum)
				value = 1;
			else
				value = distance / maximum;

			handle.transform.position.x = position.x + value * maximum;
			handle.syncToBuffer();
		}
	}
};

int main() {
	glfwInit();
	timeSetup();
	Window window;
	ikEcsInit();

	Camera camera;
	camera.transform.position = { 0, 0, 1 };
	camera.transform.rotation = glm::angleAxis(0.0f, glm::vec3(0, 0, 1));
	camera.fov = 74;

	sliderMesh = new Mesh(100);
	handleMesh = new Mesh(100);

	sliderMesh->vertex({ 0, -0.1, 0 }, { 1, 1, 1 }, { 0.8, 0.8, 0.8, 1 });
	sliderMesh->vertex({ 0, 0.1, 0 }, { 1, 1, 1 }, { 0.8, 0.8, 0.8, 1 });
	sliderMesh->vertex({ 1, -0.1, 0 }, { 1, 1, 1 }, { 0.8, 0.8, 0.8, 1 });

	sliderMesh->vertex({ 1, 0.1, 0 }, { 1, 1, 1 }, { 0.8, 0.8, 0.8, 1 });
	sliderMesh->vertex({ 0, 0.1, 0 }, { 1, 1, 1 }, { 0.8, 0.8, 0.8, 1 });
	sliderMesh->vertex({ 1, -0.1, 0 }, { 1, 1, 1 }, { 0.8, 0.8, 0.8, 1 });


	handleMesh->vertex({ -1, -1, 0 }, { 1, 1, 1 }, { 0.4, 0.4, 0.4, 1 });
	handleMesh->vertex({ -1, 1, 0 }, { 1, 1, 1 },  { 0.4, 0.4, 0.4, 1 });
	handleMesh->vertex({ 1, -1, 0 }, { 1, 1, 1 },  { 0.4, 0.4, 0.4, 1 });

	handleMesh->vertex({ 1, 1, 0 }, { 1, 1, 1 },   { 0.4, 0.4, 0.4, 1 });
	handleMesh->vertex({ -1, 1, 0 }, { 1, 1, 1 },  { 0.4, 0.4, 0.4, 1 });
	handleMesh->vertex({ 1, -1, 0 }, { 1, 1, 1 },  { 0.4, 0.4, 0.4, 1 });

	Slider slider;

	Fence fence;

	while (!window.shouldClose()) {
		window.updateFormat();
		window.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		window.setBackground(0.1, 0.1, 0.1, 1);
		window.setViewportSize(1, 1, 0, 0);
		window.setViewport();

		camera.aspectRatio = (float)window.getFormat()->width / (float)window.getFormat()->height;

		slider.exec(window);
		std::cout << slider.value << std::endl;

		fence.wait(1000000000);

		render(camera);

		fence.place();

		window.swapBuffers();
		glfwPollEvents();
		timeUpdate();
	}
	return 1;
}