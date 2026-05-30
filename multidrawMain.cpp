#include "graphics_API.h"
#include "graphics_API_extension.h"
#include "ecs.h"

#include <chrono>
#include <random>
#include <ctime>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

struct DrawArraysIndirectCommand {
    GLuint count;
    GLuint instanceCount;
    GLuint first;
    GLuint baseInstance;
};

int main() {
    glfwInit();
    Window window;

    // triangle (3 verts)
    float triangle[] = {
        0.0f * 0.1,  0.5f * 0.1,
       -0.5f * 0.1, -0.5f * 0.1,
        0.5f * 0.1, -0.5f * 0.1
    };

    // square (2 triangles = 6 verts)
    float square[] = {
       -0.5f * 0.1,  0.5f * 0.1,
       -0.0f * 0.1,  0.5f * 0.1,
       -0.5f * 0.1, -0.0f * 0.1,

       -0.5f * 0.1, -0.0f * 0.1,
        0.5f * 0.1,  0.5f * 0.1,
        0.5f * 0.1, -0.5f * 0.1
    };
    std::vector<float> vertices;
    vertices.insert(vertices.end(), std::begin(triangle), std::end(triangle));
    vertices.insert(vertices.end(), std::begin(square), std::end(square));

    Buffer vertexBuffer(vertices.size() * sizeof(float));

    vertexBuffer.write(vertices.data(), 0, vertices.size() * sizeof(float));

    constexpr int GRID_W = 10;
    constexpr int GRID_H = 10;
    constexpr int TOTAL = GRID_W * GRID_H * 2;

    std::vector<DrawArraysIndirectCommand> cmds;
    cmds.reserve(TOTAL);

    GLuint triangleFirst = 0;
    GLuint squareFirst = 3;

    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {

            bool isTriangle = (x + y) % 2 == 0;

            DrawArraysIndirectCommand cmd{};
            cmd.instanceCount = 1;
            cmd.baseInstance = cmds.size();

            if (isTriangle) {
                cmd.first = triangleFirst;
                cmd.count = 3;
            }
            else {
                cmd.first = squareFirst;
                cmd.count = 6;
            }

            cmds.push_back(cmd);
        }
    }

    Buffer indirectBuffer(cmds.size() * sizeof(DrawArraysIndirectCommand));
    indirectBuffer.write(cmds.data(), 0, cmds.size() * sizeof(DrawArraysIndirectCommand));

    std::vector<glm::vec2> offsets;

    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            offsets.emplace_back(
                x * 0.2f - 1.0f,
                y * 0.2f - 1.0f
            );
        }
    }

    Buffer offsetBuffer(offsets.size() * sizeof(glm::vec2));

    offsetBuffer.write(offsets.data(), 0, offsets.size() * sizeof(glm::vec2));

    VAO vao;

    // position attribute (vec2)
    vao.attribute(0, 2, GL_FLOAT, GL_FALSE, 0);
    vao.binding(0, vertexBuffer.getID(), 0, sizeof(float) * 2);
    vao.link(0, 0);

    vao.attribute(1, 2, GL_FLOAT, GL_FALSE, 0);
    vao.binding(1, offsetBuffer.getID(), 0, sizeof(glm::vec2));
    vao.link(1, 1);

    vao.divisor(1, 1);

    ShaderProgram shader;
    shader.addShader(GL_VERTEX_SHADER, readFileToString("shaders/multidraw.vert"));
    shader.addShader(GL_FRAGMENT_SHADER, readFileToString("shaders/multidraw.frag"));
    shader.compile();

    while (!window.shouldClose()) {
        window.getFormat();
        window.clear(GL_COLOR_BUFFER_BIT);
        window.setBackground(0.1, 0.1, 0.1, 1);
        window.setViewportSize(1, 1, 0, 0);
        window.setViewport();

        shader.useProgram();
        indirectBuffer.bind(GL_DRAW_INDIRECT_BUFFER);

        vao.drawMultiIndirect(
            GL_TRIANGLES,
            cmds.size(),
            (void*)0,
            sizeof(DrawArraysIndirectCommand)
        );

        window.swapBuffers();
        glfwPollEvents();
    }

}