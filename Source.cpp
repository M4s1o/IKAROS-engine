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

static constexpr size_t FPS_SMOOTH_WINDOW = 60;
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

int main() {
    glfwInit();
	timeSetup();
    Window window;
	window.setVsync(false);
	ikEcsInit();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window.getContext(), true);
	ImGui_ImplOpenGL3_Init("#version 460");
	ImGui::StyleColorsDark();

    Camera camera;
	float cameraSpeed = 1;
	float sensitivity = 130;

	Mesh triangleMesh(3);
	triangleMesh.vertex(glm::vec3(0, 1, 0), glm::vec3(0, 0, 1), glm::vec4(1, 0, 0, 1));
	triangleMesh.vertex(glm::vec3(1, -1, 0), glm::vec3(0, 0, 1), glm::vec4(0, 1, 0, 1));
	triangleMesh.vertex(glm::vec3(-1, -1, 0), glm::vec3(0, 0, 1), glm::vec4(0, 0, 1, 1));

	Mesh sphereMesh(3000);
	sphereMesh.sphere(glm::vec3(0, 0, 0), glm::angleAxis(0.0f, glm::vec3(0, 1, 0)), glm::vec3(1, 1, 1), glm::vec4(0.8, 0.1, 0.1, 1), 10);

	Part trianglePart;
	trianglePart.setMesh(sphereMesh);

    while (!window.shouldClose()) {
		window.updateFormat();
        window.setBackground(0.1, 0.1, 0.1, 1);
        window.setViewportSize(1, 1, 0, 0);
        window.setViewport();

		camera.aspectRatio = (float)window.getFormat()->width / (float)window.getFormat()->height;

		// translation
		if (glfwGetKey(window.getContext(), GLFW_KEY_W) == GLFW_PRESS)
			camera.move(glm::vec3(0, 0, -(cameraSpeed * deltaTime())));

		if (glfwGetKey(window.getContext(), GLFW_KEY_S) == GLFW_PRESS)
			camera.move(glm::vec3(0, 0, cameraSpeed * deltaTime()));

		if (glfwGetKey(window.getContext(), GLFW_KEY_D) == GLFW_PRESS)
			camera.move(glm::vec3(cameraSpeed * deltaTime(), 0, 0));

		if (glfwGetKey(window.getContext(), GLFW_KEY_A) == GLFW_PRESS)
			camera.move(glm::vec3(-(cameraSpeed * deltaTime()), 0, 0));

		if (glfwGetKey(window.getContext(), GLFW_KEY_SPACE) == GLFW_PRESS)
			camera.move(glm::vec3(0, cameraSpeed * deltaTime(), 0));

		if (glfwGetKey(window.getContext(), GLFW_KEY_C) == GLFW_PRESS)
			camera.move(glm::vec3(0, -(cameraSpeed * deltaTime()), 0));
		// rotation
		float rotX = 0, rotY = 0, rotZ = 0;
		if (glfwGetKey(window.getContext(), GLFW_KEY_I) == GLFW_PRESS)
			rotX += sensitivity * deltaTime();

		if (glfwGetKey(window.getContext(), GLFW_KEY_K) == GLFW_PRESS)
			rotX -= sensitivity * deltaTime();

		if (glfwGetKey(window.getContext(), GLFW_KEY_J) == GLFW_PRESS)
			rotY += sensitivity * deltaTime();

		if (glfwGetKey(window.getContext(), GLFW_KEY_L) == GLFW_PRESS)
			rotY -= sensitivity * deltaTime();

		if (glfwGetKey(window.getContext(), GLFW_KEY_U) == GLFW_PRESS)
			rotZ += sensitivity * deltaTime();

		if (glfwGetKey(window.getContext(), GLFW_KEY_O) == GLFW_PRESS)
			rotZ -= sensitivity * deltaTime();

		glm::quat quatX = glm::angleAxis(glm::radians(rotX), glm::vec3(1, 0, 0));
		glm::quat quatY = glm::angleAxis(glm::radians(rotY), glm::vec3(0, 1, 0));
		glm::quat quatZ = glm::angleAxis(glm::radians(rotZ), glm::vec3(0, 0, 1));

		camera.transform.rotation = glm::normalize(camera.transform.rotation);
		quatX = glm::normalize(quatX);
		quatY = glm::normalize(quatY);
		quatZ = glm::normalize(quatZ);

		camera.transform.rotation = camera.transform.rotation * quatX;
		camera.transform.rotation = camera.transform.rotation * quatY;
		camera.transform.rotation = camera.transform.rotation * quatZ;
		camera.transform.rotation = glm::normalize(camera.transform.rotation);

        render(camera);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("test");
		ImGui::Text("camera position: %f, %f, %f", camera.transform.position.x, camera.transform.position.y, camera.transform.position.z);
		ImGui::Text("fps: %f", FPS());
		ImGui::End();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwPollEvents();
		timeUpdate();
    }
    return 1;
}