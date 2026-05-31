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
        GLuint padding;
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

    struct PartHandle {
        GLuint mesh_index;
        GLuint padding;
    };

    struct PartMatrix {
        glm::mat4 transform_matrix;
    };

}

static Buffer* geometryBuffer = nullptr;
static Buffer* surfaceBuffer = nullptr;
static Buffer* meshBuffer = nullptr;

static Buffer* partBuffer = nullptr;
static Buffer* partHandleBuffer = nullptr;
static Buffer* partMatrixBuffer = nullptr;

static Buffer* commandBuffer = nullptr;

static VAO* vao = nullptr;

static ShaderProgram* commandBuilderProgram = nullptr;
static ShaderProgram* matrixBuilderProgram = nullptr;
static ShaderProgram* renderProgram = nullptr;

static const unsigned int MAX_MESH_COUNT = 100;
static const unsigned int MAX_VERT_COUNT = 1000000;
static const unsigned int MAX_PART_COUNT = 100000;

static unsigned int partCount = 0;
static unsigned int meshCount = 0;
static unsigned int vertCount = 0;

Fence renderFence;

/*   
* =================================
*   note:
*       -alignment=std430
*       -padding element excluded in shaders
* =================================
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
*   meshBuffer:
*       -contains: surface and geometry handles
*       -stride=4*uint
*       -layout=(struct{
*           uint(vertex_first),
*           uint(vertex_count),
*           uint(vertex_capacity),
*           uint(padding)
*           }(handle))
*       -note: not vao binding
* 
*   partBuffer:
*       -contains: part data
*       -stride=12*float
*       -layout=(struct{
*               vec3(position),
*               float(padding),
*               vec4(rotation),
*               vec3(scale),
*               float(padding)
                }(transform))
*       -note: not vao binding
* 
*   partHandleBuffer:
*       -contains: mesh index
*       -stride=2*uint
*       -layout=(
*           uint(mesh_index),
*           uint(padding))
*       -note: not vao binding
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
*       -layout=(struct{
*               uint(count),
*               uint(instance_count),
*               uint(first),
*               uint(base_instance)
*               }(command))
*       -note: not vao binding
* =================================
*   vao:
*       -attributes:
*           -per vertex:
*               0=vec3(vertex position)->geometryBuffer(position)
*               1=vec3(vertex normal)->geometryBuffer(normal)
*               2=vec4(vertex color)->surfaceBuffer(color)
*           -per part:
*               3,4,5,6=mat4(part transform matrix)->partMatrixBuffer(transformation_matrix)
*       -uniforms:
*           viewMatrix=mat4(camera_view_matrix)
*           projMatrix=mat4(camera_projection_matrix)
* =================================
*   buffer bindings:
*       0=commandBuffer
*       1=partHandleBuffer
*       2=meshBuffer
*       3=partBuffer
*       4=partMatrixBuffer
* =================================
*   command builder shader:
*       -purpose: construct indirect draw commands
*       -bindings:
*           0=struct{
*               uint(count),
*               uint(instance_count),
*               uint(first),
*               uint(base_instance)
*               }(command)->commandBuffer(command)
*           1=uint(mesh_index)->partHandleBuffer(mesh_index)
*           2=struct{
*               uint(vertex_first),
*               uint(vertex_count),
*               uint(vertex_capacity),
*               uint(padding)
*               }(mesh_handle)->meshBuffer(handle)
*       -uniforms:
*           uint partCount;
*           
*   matrix builder shader:
*       -purpose: compute transformation matracies from transform components
*       -bindings:
*           3=struct{
*               vec3(position),
*               float(padding),
*               vec4(rotation),
*               vec3(scale),
*               float(padding)
*               }(part_transform)->partBuffer(transform)
*           4=mat4(part_transform_matrix)->partMatrixBuffer(transform_matrix)
*       -uniforms:
*           uint partCount;
* =================================
*/

// =========== Initialization ===========
void ikEcsInit() {
    geometryBuffer =   new Buffer(sizeof(GeometryVertex) * MAX_VERT_COUNT);
    surfaceBuffer =    new Buffer(sizeof(SurfaceVertex) * MAX_VERT_COUNT);
    meshBuffer =       new Buffer(sizeof(MeshMetaData) * MAX_MESH_COUNT);
    partBuffer =       new Buffer(sizeof(PartTransform) * MAX_PART_COUNT);
    partHandleBuffer = new Buffer(sizeof(PartHandle) * MAX_PART_COUNT);
    partMatrixBuffer = new Buffer(sizeof(PartMatrix) * MAX_PART_COUNT);
    commandBuffer =    new Buffer(sizeof(IndirectCommand) * MAX_PART_COUNT);

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

    // part matrix buffer
    vao->binding(2, partMatrixBuffer->getID(), 0, sizeof(GLfloat) * 16);

    vao->attribute(3, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 0); // part transform matrix
    vao->attribute(4, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4);
    vao->attribute(5, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 8);
    vao->attribute(6, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 12);
    vao->link(3, 2);
    vao->link(4, 2);
    vao->link(5, 2);
    vao->link(6, 2);
    vao->divisor(2, 1);
    
    std::string renderingPipelineShaderDirectory = "shaders/rendering pipeline/";

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

// =========== Renderer ===========
void render(Camera camera) {
    commandBuffer->bind(   GL_SHADER_STORAGE_BUFFER, 0, 0, commandBuffer->getSize());
    partHandleBuffer->bind(GL_SHADER_STORAGE_BUFFER, 1, 0, partHandleBuffer->getSize());
    meshBuffer->bind(      GL_SHADER_STORAGE_BUFFER, 2, 0, meshBuffer->getSize());
    partBuffer->bind(      GL_SHADER_STORAGE_BUFFER, 3, 0, partBuffer->getSize());
    partMatrixBuffer->bind(GL_SHADER_STORAGE_BUFFER, 4, 0, partMatrixBuffer->getSize());
    
    commandBuilderProgram->useProgram();
    glUniform1ui(glGetUniformLocation(commandBuilderProgram->getID(), "part_count"), partCount);
    commandBuilderProgram->runCompute((partCount + 255) / 256, 1, 1, GL_SHADER_STORAGE_BARRIER_BIT);
    
    Fence commandBuilderFence;
    commandBuilderFence.place();
    
    matrixBuilderProgram->useProgram();
    glUniform1ui(glGetUniformLocation(matrixBuilderProgram->getID(), "part_count"), partCount);
    matrixBuilderProgram->runCompute((partCount + 255) / 256, 1, 1, GL_SHADER_STORAGE_BARRIER_BIT);
    
    Fence matrixBuilderFence;
    matrixBuilderFence.place();
    
    commandBuffer->unbind(GL_SHADER_STORAGE_BUFFER, 0);
    partHandleBuffer->unbind(GL_SHADER_STORAGE_BUFFER, 1);
    meshBuffer->unbind(GL_SHADER_STORAGE_BUFFER, 2);
    partBuffer->unbind(GL_SHADER_STORAGE_BUFFER, 3);
    partMatrixBuffer->unbind(GL_SHADER_STORAGE_BUFFER, 4);

    getCurrentContext()->clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    GLuint64 timeout = 1000000000; // 1 second
    commandBuilderFence.wait(timeout);
    matrixBuilderFence.wait(timeout);

    renderProgram->useProgram();

    commandBuffer->bind(GL_DRAW_INDIRECT_BUFFER);
    glEnable(GL_DEPTH_TEST);

    glm::mat4 viewMatrix = glm::inverse(camera.transform.getTransformMatrix());
    glm::mat4 projectionMatrix = glm::perspective(glm::radians(camera.fov), camera.aspectRatio, camera.nearPlane, camera.farPlane);
    glUniformMatrix4fv(glGetUniformLocation(renderProgram->getID(), "camera_view_matrix"), 1, GL_FALSE, glm::value_ptr(viewMatrix));
    glUniformMatrix4fv(glGetUniformLocation(renderProgram->getID(), "camera_projection_matrix"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));

    vao->drawMultiIndirect(GL_TRIANGLES, partCount, (void*)0, sizeof(IndirectCommand));

    renderFence.place();

    commandBuffer->unbind(GL_DRAW_INDIRECT_BUFFER);

    getCurrentContext()->swapBuffers();
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

    MeshMetaData metaData;
    metaData.vertex_capacity = vertex_capacity;
    metaData.vertex_count = 0;
    metaData.vertex_first = vertCount;
    meshBuffer->write(&metaData, sizeof(MeshMetaData) * index, sizeof(MeshMetaData));
    vertCount++;
}
void Mesh::vertex(glm::vec3 position, glm::vec3 normal, glm::vec4 color) {
    errorMark;
    GeometryVertex geometryVertex;
    geometryVertex.position = position;
    geometryVertex.normal = normal;

    SurfaceVertex surfaceVertex;
    surfaceVertex.color = color;

    MeshMetaData metaData;
    meshBuffer->read(&metaData, sizeof(MeshMetaData) * index, sizeof(MeshMetaData));

    if (metaData.vertex_count >= metaData.vertex_capacity) {
        Error(ENGINE_ECS, MESH, WARNING, SEVERITY_MEDIUM, OUT_OF_RANGE_VALUE, "vertex capacity reached, cannot upload more geometry");
        return;
    }

    geometryBuffer->write(&geometryVertex, sizeof(GeometryVertex) * (metaData.vertex_first + metaData.vertex_count), sizeof(GeometryVertex));
    surfaceBuffer->write(&surfaceVertex, sizeof(SurfaceVertex) * (metaData.vertex_first + metaData.vertex_count), sizeof(SurfaceVertex));

    metaData.vertex_count++;
    meshBuffer->write(&metaData, sizeof(MeshMetaData) * index, sizeof(MeshMetaData));

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
    index = partCount;
    partCount++;

    PartHandle meshHandle;
    meshHandle.mesh_index = 0;
    partHandleBuffer->write(&meshHandle, sizeof(PartHandle) * index, sizeof(PartHandle));
    syncToBuffer();
}
void Part::setMesh(Mesh& mesh) {
    PartHandle meshHandle;
    meshHandle.mesh_index = mesh.getHandle();
    partHandleBuffer->write(&meshHandle, sizeof(PartHandle) * index, sizeof(PartHandle));
}
void Part::syncFromBuffer() {
    PartTransform bufferTransform;
    partBuffer->read(&bufferTransform, sizeof(PartTransform) * index, sizeof(PartTransform));

    transform.position = bufferTransform.position;
    transform.rotation = bufferTransform.rotation;
    transform.scale = bufferTransform.scale;
}
void Part::syncToBuffer() {
    PartTransform bufferTransform;
    bufferTransform.position = transform.position;
    bufferTransform.rotation = transform.rotation;
    bufferTransform.scale = transform.scale;

    partBuffer->write(&bufferTransform, sizeof(PartTransform) * index, sizeof(PartTransform));
}
GLuint Part::getHandle() {
    return index;
}