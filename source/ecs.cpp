/*
IKAROS Engine - Graphics API Implementation
Author: Branislav Wilhelm
*/

/*
*   TODO:
*       - switch from hybrid indirect instancing to normal
*/

#include "ecs.h"
#include "graphics_API.h"
#include "graphics_API_extension.h"
#include "error_wrapper.h"

#include <glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <gtx/quaternion.hpp>
#include <gtc/type_ptr.hpp>

// buffer managment helper declarations
namespace {

    struct IndirectCommand {
        GLuint count;
        GLuint instanceCount;
        GLuint first;
        GLuint baseInstance;
    };

    struct MeshMetaData {
        GLuint vertex_first;
        GLuint vertex_count;
        GLuint vertex_capacity;
        GLuint state;
    };

    struct GeometryVertex {
        glm::vec3 position;
        float padding0;
        glm::vec3 normal;
        float padding1;
    };

    struct SurfaceVertex {
        glm::vec4 color;
    };

    struct PartTransform {
        glm::vec3 position;
        float padding0;
        glm::quat rotation;
        glm::vec3 scale;
        float padding1;
    };

    struct PartMetaData {
        GLuint mesh_index;
        GLuint state;
    };

    struct PartMatrix {
        glm::mat4 transform_matrix;
    };

    struct PartReference {
        GLuint part_index;
        GLuint padding;
    };

    struct MeshRenderData {
        GLuint count;
        GLuint reference_offset;
    };
}

static Buffer* geometryBuffer = nullptr;
static Buffer* surfaceBuffer = nullptr;
static Buffer* meshMetaBuffer = nullptr;
static Buffer* meshRenderBuffer = nullptr;

static Buffer* partTransformBuffer = nullptr;
static Buffer* partMetaBuffer = nullptr;
static Buffer* partMatrixBuffer = nullptr;

static Buffer* commandBuffer = nullptr;
static Buffer* referenceBuffer = nullptr;

static VAO* vao = nullptr;

static ShaderProgram* commandBuilderProgram = nullptr;
static ShaderProgram* matrixBuilderProgram = nullptr;
static ShaderProgram* renderabilityProgram = nullptr;
static ShaderProgram* renderProgram = nullptr;
static ShaderProgram* prefixSumProgram = nullptr;
static ShaderProgram* orderingProgram = nullptr;

static const unsigned int MAX_MESH_COUNT = 100;
static const unsigned int MAX_VERT_COUNT = 1000000;
static const unsigned int MAX_PART_COUNT = 100000;

static unsigned int partCount = 0;
static unsigned int meshCount = 0;
static unsigned int vertCount = 0;

static unsigned int activePartCount = 0;
static unsigned int activeMeshCount = 0;

Fence renderFence;

/*   
* =================================
*   note:
*       -alignment=std430
*       -padding element excluded in shaders
* =================================
*   reference buffer:
*       -contains: references to objects
*       -stride=2*uint
*       -layout=(
*           uint(index),
*           uint(padding))
* 
*   geometry buffer:
*       -contains: mesh geometry data
*       -stride=8*float
*       -layout=(
*           vec3(position),
*           float(padding),
*           vec3(normal),
*           float(padding))
*
*   surfaceBuffer:
*       -contains: mesh surface data
*       -stride=4*float
*       -layout=(
*           vec4(color))
* 
*   meshMetaBuffer:
*       -contains: surface and geometry handles
*       -stride=5*uint
*       -layout=(
*           uint(vertex_first),
*           uint(vertex_count),
*           uint(vertex_capacity),
*           uint(state),
*           uint(reference_offset))
* 
*   meshRenderBuffer:
*       -contains: counts
*       -stride=uint
*       -layout=(
*           uint(count),
*           uint(reference_offset))
* 
*   partTransformBuffer:
*       -contains: part data
*       -stride=12*float
*       -layout=(
*           vec3(position),
*           float(padding),
*           vec4(rotation),
*           vec3(scale),
*           float(padding))
* 
*   partMetaBuffer:
*       -contains: mesh index
*       -stride=2*uint
*       -layout=(
*           uint(mesh_index),
*           uint(state))
* 
*   partMatrixBuffer:
*       -contains: transform matrix
*       -stride=16*float
*       -layout=(
*           mat4(transform_matrix))
*
*   commandBuffer:
*       -contains: draw commands
*       -stride=4*uint
*       -layout=(
*           uint(count),
*           uint(instance_count),
*           uint(first),
*           uint(base_instance))
* =================================
*   vao:
*       -attributes:
*           0=vec3(vertex_position)->geometryBuffer(position)
*           1=vec3(vertex_normal)->geometryBuffer(normal)
*           2=vec4(vertex color)->surfaceBuffer(color)
*       -bindings:
*           3=partMatrixBuffer(transformation_matrix)
*           5=referenceBuffer(index)
*       -uniforms:
*           viewMatrix=mat4(camera_view_matrix)
*           projMatrix=mat4(camera_projection_matrix)
* =================================
*   attributes:
*       0=vec3(vertex_position)->geometryBuffer(position)
*       1=vec3(vertex_normal)->geometryBuffer(normal)
*       2=vec4(vertex color)->surfaceBuffer(color)
* 
*   bindings:
*       0=meshMetaBuffer
*       1=meshRenderBuffer
*       2=partMetaBuffer
*       3=partTransformBuffer
*       4=partMatrixBuffer
*       5=commandBuffer
*       6=referenceBuffer
* ================================= // FIX!!!
*   renderability shader:
*       -purpose: count renderable object based on their state
*       -bindings:
*           1=meshAtomBuffer(mesh_counter)
*           2=partMetaBuffer(part_metadata)
*       -uniforms:
*           uint partCount
* 
*   command builder shader:
*       -purpose: construct indirect draw commands
*       -bindings:
*           0=meshMetaBuffer(mesh_metadata)
*           1=partMetaBuffer(part_metadata
*           4=commandBuffer(command)
*       -uniforms:
*           uint partCount
*           
*   matrix builder shader:
*       -purpose: compute transformation matracies from transform components
*       -bindings:
*           2=partTransformBuffer(part_transform)
*           3=partMatrixBuffer(part_transform_matrix)
*       -uniforms:
*           uint partCount
* =================================
*/

// =========== Initialization ===========
void ikEcsInit() {
    geometryBuffer =      new Buffer(sizeof(GeometryVertex) * MAX_VERT_COUNT);
    surfaceBuffer =       new Buffer(sizeof(SurfaceVertex) * MAX_VERT_COUNT);
    meshMetaBuffer =      new Buffer(sizeof(MeshMetaData) * MAX_MESH_COUNT);
    meshRenderBuffer =    new Buffer(sizeof(MeshRenderData) * MAX_MESH_COUNT);

    partTransformBuffer = new Buffer(sizeof(PartTransform) * MAX_PART_COUNT);
    partMetaBuffer =      new Buffer(sizeof(PartMetaData) * MAX_PART_COUNT);
    partMatrixBuffer =    new Buffer(sizeof(PartMatrix) * MAX_PART_COUNT);

    commandBuffer =       new Buffer(sizeof(IndirectCommand) * MAX_MESH_COUNT);
    referenceBuffer =     new Buffer(sizeof(PartReference) * MAX_PART_COUNT);

	vao = new VAO;

    // geometry buffer
    vao->binding(0, geometryBuffer->getID(), 0, sizeof(GLfloat) * 8);

    vao->attribute(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 0); // vertex position
    vao->link(0, 0);

    vao->attribute(1, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4); // vertex normal
    vao->link(1, 0);

    // surface buffer
    vao->binding(1, surfaceBuffer->getID(), 0, sizeof(GLfloat) * 4);

    vao->attribute(2, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 0); // vertex color
    vao->link(2, 1);
    
    std::string renderingPipelineShaderDirectory = "shaders/rendering pipeline/";

    renderabilityProgram = new ShaderProgram;
    renderabilityProgram->addShader(GL_COMPUTE_SHADER, readFileToString(renderingPipelineShaderDirectory + "renderability_shader.comp"));
    renderabilityProgram->compile();

    prefixSumProgram = new ShaderProgram;
    prefixSumProgram->addShader(GL_COMPUTE_SHADER, readFileToString(renderingPipelineShaderDirectory + "prefixSum_shader.comp"));
    prefixSumProgram->compile();

    orderingProgram = new ShaderProgram;
    orderingProgram->addShader(GL_COMPUTE_SHADER, readFileToString(renderingPipelineShaderDirectory + "ordering_shader.comp"));
    orderingProgram->compile();

    commandBuilderProgram = new ShaderProgram;
    commandBuilderProgram->addShader(GL_COMPUTE_SHADER, readFileToString(renderingPipelineShaderDirectory + "command_builder_shader.comp"));
    commandBuilderProgram->compile();

    matrixBuilderProgram = new ShaderProgram;
    matrixBuilderProgram->addShader(GL_COMPUTE_SHADER, readFileToString(renderingPipelineShaderDirectory + "matrix_builder_shader.comp"));
    matrixBuilderProgram->compile();

    renderProgram = new ShaderProgram;
    renderProgram->addShader(GL_VERTEX_SHADER, readFileToString(renderingPipelineShaderDirectory + "render_shader.vert"));
    renderProgram->addShader(GL_FRAGMENT_SHADER, readFileToString(renderingPipelineShaderDirectory + "render_shader.frag"));
    renderProgram->compile();
}

static Fence fence;
// =========== Renderer ===========
void render(Camera camera) {
    meshMetaBuffer->bind(     GL_SHADER_STORAGE_BUFFER, 0, 0, meshMetaBuffer->getSize());
    meshRenderBuffer->bind(   GL_SHADER_STORAGE_BUFFER, 1, 0, meshRenderBuffer->getSize());

    partMetaBuffer->bind(     GL_SHADER_STORAGE_BUFFER, 2, 0, partMetaBuffer->getSize());
    partTransformBuffer->bind(GL_SHADER_STORAGE_BUFFER, 3, 0, partTransformBuffer->getSize());
    partMatrixBuffer->bind(   GL_SHADER_STORAGE_BUFFER, 4, 0, partMatrixBuffer->getSize());

    commandBuffer->bind(      GL_SHADER_STORAGE_BUFFER, 5, 0, commandBuffer->getSize());
    referenceBuffer->bind(    GL_SHADER_STORAGE_BUFFER, 6, 0, referenceBuffer->getSize());

    meshRenderBuffer->set(0, 0, meshRenderBuffer->getSize());

    renderabilityProgram->useProgram();
    glUniform1ui(glGetUniformLocation(renderabilityProgram->getID(), "part_count"), partCount);
    renderabilityProgram->runCompute((partCount + 255) / 256, 1, 1, GL_SHADER_STORAGE_BARRIER_BIT);

    prefixSumProgram->useProgram();
    glUniform1ui(glGetUniformLocation(prefixSumProgram->getID(), "mesh_count"), meshCount);
    prefixSumProgram->runCompute(1, 1, 1, GL_SHADER_STORAGE_BARRIER_BIT);

    orderingProgram->useProgram();
    glUniform1ui(glGetUniformLocation(orderingProgram->getID(), "part_count"), partCount);
    orderingProgram->runCompute((partCount + 255) / 256, 1, 1, GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

    matrixBuilderProgram->useProgram();
    glUniform1ui(glGetUniformLocation(matrixBuilderProgram->getID(), "active_part_count"), activePartCount);
    matrixBuilderProgram->runCompute((activePartCount + 255) / 256, 1, 1);

    commandBuilderProgram->useProgram();
    glUniform1ui(glGetUniformLocation(commandBuilderProgram->getID(), "mesh_count"), meshCount);
    commandBuilderProgram->runCompute((meshCount + 255) / 256, 1, 1);

    meshMetaBuffer->unbind(     GL_SHADER_STORAGE_BUFFER, 0);
    meshRenderBuffer->unbind(   GL_ATOMIC_COUNTER_BUFFER, 1);
    partMetaBuffer->unbind(     GL_SHADER_STORAGE_BUFFER, 2);
    partTransformBuffer->unbind(GL_SHADER_STORAGE_BUFFER, 3);
    commandBuffer->unbind(      GL_SHADER_STORAGE_BUFFER, 5);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    renderProgram->useProgram();

    commandBuffer->bind(GL_DRAW_INDIRECT_BUFFER);

    glEnable(GL_DEPTH_TEST);

    glm::mat4 viewMatrix = glm::inverse(camera.transform.getTransformMatrix());
    glm::mat4 projectionMatrix = glm::perspective(glm::radians(camera.fov), camera.aspectRatio, camera.nearPlane, camera.farPlane);
    glUniformMatrix4fv(glGetUniformLocation(renderProgram->getID(), "camera_view_matrix"), 1, GL_FALSE, glm::value_ptr(viewMatrix));
    glUniformMatrix4fv(glGetUniformLocation(renderProgram->getID(), "camera_projection_matrix"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));

    vao->drawMultiIndirect(GL_TRIANGLES, activeMeshCount, (void*)0, sizeof(IndirectCommand));

    commandBuffer->unbind(   GL_DRAW_INDIRECT_BUFFER);
    partMatrixBuffer->unbind(GL_SHADER_STORAGE_BUFFER, 4);
    referenceBuffer->unbind( GL_SHADER_STORAGE_BUFFER, 6);

    fence.place();
}

// temp debug functions
#include <iostream>
#include <vector>

void dumpEcsState(unsigned int meshLimit, unsigned int partLimit) {
    std::cout << "\n================ ECS DEBUG DUMP ================\n\n";

    // ---------- MeshMetaData ----------
    {
        std::vector<MeshMetaData> data(meshLimit);
        meshMetaBuffer->read(
            data.data(),
            0,
            sizeof(MeshMetaData) * std::min(meshLimit, meshCount)
        );

        std::cout << "[MeshMetaData]\n";
        for (unsigned int i = 0; i < std::min(meshLimit, meshCount); i++) {
            std::cout
                << "mesh " << i
                << " | vFirst=" << data[i].vertex_first
                << " | vCount=" << data[i].vertex_count
                << " | vCap=" << data[i].vertex_capacity
                << " | state=" << data[i].state
                << "\n";
        }
        std::cout << "\n";
    }

    // ---------- MeshRenderData ----------
    {
        std::vector<MeshRenderData> data(meshLimit);
        meshRenderBuffer->read(
            data.data(),
            0,
            sizeof(MeshRenderData) * std::min(meshLimit, meshCount)
        );

        std::cout << "[MeshRenderData]\n";
        for (unsigned int i = 0; i < std::min(meshLimit, meshCount); i++) {
            std::cout
                << "mesh " << i
                << " | referenced=" << data[i].count
                << " | offset=" << data[i].reference_offset
                << "\n";
        }
        std::cout << "\n";
    }

    // ---------- PartMetaData ----------
    {
        std::vector<PartMetaData> data(partLimit);
        partMetaBuffer->read(
            data.data(),
            0,
            sizeof(PartMetaData) * std::min(partLimit, partCount)
        );

        std::cout << "[PartMetaData]\n";
        for (unsigned int i = 0; i < std::min(partLimit, partCount); i++) {
            std::cout
                << "part " << i
                << " | mesh=" << data[i].mesh_index
                << " | state=" << data[i].state
                << "\n";
        }
        std::cout << "\n";
    }

    // ---------- PartTransform ----------
    {
        std::vector<PartTransform> data(partLimit);
        partTransformBuffer->read(
            data.data(),
            0,
            sizeof(PartTransform) * std::min(partLimit, partCount)
        );

        std::cout << "[PartTransform]\n";
        for (unsigned int i = 0; i < std::min(partLimit, partCount); i++) {
            std::cout
                << "part " << i
                << " | pos=("
                << data[i].position.x << ", "
                << data[i].position.y << ", "
                << data[i].position.z << ")\n";
        }
        std::cout << "\n";
    }

    // ---------- PartMatrices ----------
    {
        std::vector<PartMatrix> data(partLimit);
        partMatrixBuffer->read(
            data.data(),
            0,
            sizeof(PartMatrix) * std::min(partLimit, partCount)
        );

        std::cout << "[PartMatrices]\n";
        for (unsigned int i = 0; i < std::min(partLimit, partCount); i++) {
            const glm::mat4& m = data[i].transform_matrix;

            std::cout
                << "mat " << i
                << " | first row: "
                << m[0][0] << ", "
                << m[0][1] << ", "
                << m[0][2] << ", "
                << m[0][3]
                << "\n";
        }
        std::cout << "\n";
    }

    // ---------- ReferenceBuffer ----------
    {
        std::vector<GLuint> data(partLimit);
        referenceBuffer->read(
            data.data(),
            0,
            sizeof(GLuint) * std::min(partLimit, partCount)
        );

        std::cout << "[ReferenceBuffer]\n";
        for (unsigned int i = 0; i < std::min(partLimit, partCount); i++) {
            std::cout
                << "ref[" << i << "] = part "
                << data[i]
                << "\n";
        }
        std::cout << "\n";
    }

    // ---------- IndirectCommand ----------
    {
        std::vector<IndirectCommand> data(meshLimit);
        commandBuffer->read(
            data.data(),
            0,
            sizeof(IndirectCommand) * std::min(meshLimit, meshCount)
        );

        std::cout << "[IndirectCommand]\n";
        for (unsigned int i = 0; i < std::min(meshLimit, meshCount); i++) {
            std::cout
                << "cmd " << i
                << " | count=" << data[i].count
                << " | inst=" << data[i].instanceCount
                << " | first=" << data[i].first
                << " | base=" << data[i].baseInstance
                << "\n";
        }
        std::cout << "\n";
    }

    std::cout << "================================================\n\n";
}

// =========== Mesh ===========
Mesh::Mesh(unsigned int vertex_capacity) {
    if (meshCount + 1 > MAX_MESH_COUNT) {
        Error(ENGINE_ECS, MESH, WARNING, SEVERITY_HIGH, OUT_OF_RANGE_VALUE, "maximum mesh count reached, cannot create more meshes");
        return;
    }
    if (vertCount + vertex_capacity > MAX_VERT_COUNT) {
        Error(ENGINE_ECS, MESH, WARNING, SEVERITY_HIGH, OUT_OF_RANGE_VALUE, "maximum vertex count reached, cannot create mesh of this size");
        return;
    }
    index = meshCount;
    meshCount++;
    activeMeshCount++;

    MeshMetaData metaData;
    metaData.vertex_capacity = vertex_capacity;
    metaData.vertex_count = 0;
    metaData.vertex_first = vertCount;
    metaData.state = 1;
    meshMetaBuffer->write(&metaData, sizeof(MeshMetaData) * index, sizeof(MeshMetaData));
    vertCount += vertex_capacity;
}
Mesh::~Mesh() {
    activeMeshCount--;
}
void Mesh::vertex(glm::vec3 position, glm::vec3 normal, glm::vec4 color) {
    errorMark;
    GeometryVertex geometryVertex;
    geometryVertex.position = position;
    geometryVertex.normal = normal;

    SurfaceVertex surfaceVertex;
    surfaceVertex.color = color;

    MeshMetaData metaData;
    meshMetaBuffer->read(&metaData, sizeof(MeshMetaData) * index, sizeof(MeshMetaData));

    if (metaData.vertex_count >= metaData.vertex_capacity) {
        Error(ENGINE_ECS, MESH, WARNING, SEVERITY_MEDIUM, OUT_OF_RANGE_VALUE, "vertex capacity reached, cannot upload more geometry");
        return;
    }

    geometryBuffer->write(&geometryVertex, sizeof(GeometryVertex) * (metaData.vertex_first + metaData.vertex_count), sizeof(GeometryVertex));
    surfaceBuffer->write(&surfaceVertex, sizeof(SurfaceVertex) * (metaData.vertex_first + metaData.vertex_count), sizeof(SurfaceVertex));

    metaData.vertex_count++;
    meshMetaBuffer->write(&metaData, sizeof(MeshMetaData) * index, sizeof(MeshMetaData));

}
void Mesh::sphere(glm::vec3 position, glm::quat rotation, glm::vec3 scale, glm::vec4 color, unsigned int segments) {
    for (unsigned int x = 0; x < segments; x++) {
        float thetha0 = glm::radians(360.0f / (float)segments * (float)x);
        float thetha1 = glm::radians(360.0f / (float)segments * (float)(x + 1));

        for (unsigned int y = 0; y < segments; y++) {
            float phi0 = glm::radians(180.0f / (float)segments * (float)y);
            float phi1 = glm::radians(180.0f / (float)segments * (float)(y+1));

            glm::vec3 vertexPosition[4];

            vertexPosition[0].x = glm::cos(phi0) * glm::cos(thetha0);
            vertexPosition[0].y = glm::sin(phi0) * glm::cos(thetha0);
            vertexPosition[0].z = glm::sin(thetha0);

            vertexPosition[1].x = glm::cos(phi1) * glm::cos(thetha0);
            vertexPosition[1].y = glm::sin(phi1) * glm::cos(thetha0);
            vertexPosition[1].z = glm::sin(thetha0);

            vertexPosition[2].x = glm::cos(phi0) * glm::cos(thetha1);
            vertexPosition[2].y = glm::sin(phi0) * glm::cos(thetha1);
            vertexPosition[2].z = glm::sin(thetha1);

            vertexPosition[3].x = glm::cos(phi1) * glm::cos(thetha1);
            vertexPosition[3].y = glm::sin(phi1) * glm::cos(thetha1);
            vertexPosition[3].z = glm::sin(thetha1);

            glm::vec3 vertexNormal[4];

            for (unsigned int i = 0; i < 4; i++) {
                vertexNormal[i] = glm::normalize(vertexPosition[i]);

                vertexPosition[i] *= scale;
                vertexPosition[i] = glm::rotate(rotation, vertexPosition[i]);
                vertexPosition[i] += position;
            }

            vertex(vertexPosition[0], vertexNormal[0], color);
            vertex(vertexPosition[1], vertexNormal[1], color);
            vertex(vertexPosition[2], vertexNormal[2], color);

            vertex(vertexPosition[1], vertexNormal[1], color);
            vertex(vertexPosition[2], vertexNormal[2], color);
            vertex(vertexPosition[3], vertexNormal[3], color);
        }
    }
}
GLuint Mesh::getHandle() {
    return index;
}

// =========== Part ===========
Part::Part() {
    if (partCount + 1 > MAX_PART_COUNT) {
        Error(ENGINE_ECS, MESH, WARNING, SEVERITY_HIGH, OUT_OF_RANGE_VALUE, "maximum part count reached, cannot create more parts");
        return;
    }
    if (!fence.signaled()) {
        std::cout << "Construct " << index << '\n';
    }
    index = partCount;
    partCount++;
    activePartCount++;

    PartMetaData partMetaData;
    partMetaData.mesh_index = 0;
    partMetaData.state = 0;
    partMetaBuffer->write(&partMetaData, sizeof(PartMetaData) * index, sizeof(PartMetaData));
    syncToBuffer();
}
Part::~Part() {
    if (!fence.signaled()) {
        std::cout << "Destroyed " << index << '\n';
    }
    activePartCount--;
    PartMetaData partMetaData;
    partMetaData.mesh_index = 0;
    partMetaData.state = 0;
    partMetaBuffer->write(&partMetaData, sizeof(PartMetaData) * index, sizeof(PartMetaData));
}
void Part::setMesh(Mesh& mesh) {
    PartMetaData partMetaData;
    partMetaData.mesh_index = mesh.getHandle();
    partMetaData.state = 1;
    partMetaBuffer->write(&partMetaData, sizeof(PartMetaData) * index, sizeof(PartMetaData));
}
void Part::syncFromBuffer() {
    PartTransform bufferTransform;
    partTransformBuffer->read(&bufferTransform, sizeof(PartTransform) * index, sizeof(PartTransform));

    transform.position = bufferTransform.position;
    transform.rotation = bufferTransform.rotation;
    transform.scale = bufferTransform.scale;
}
void Part::syncToBuffer() {
    PartTransform bufferTransform;
    bufferTransform.position = transform.position;
    bufferTransform.rotation = transform.rotation;
    bufferTransform.scale = transform.scale;

    partTransformBuffer->write(&bufferTransform, sizeof(PartTransform) * index, sizeof(PartTransform));
}
GLuint Part::getHandle() {
    return index;
}