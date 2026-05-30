/*
IKAROS Engine - Graphics API Implementation
Author: Branislav Wilhelm
*/

/*
*   TODO:
*       - switch from hybrid indirect instancing to normal
*/

#include "graphics_API.h"
#include "graphics_API_extension.h"
#include "error_wrapper.h"

#include <glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <gtx/quaternion.hpp>
#include <gtc/type_ptr.hpp>

static Buffer* geometryBuffer = nullptr;
static Buffer* surfaceBuffer = nullptr;
static Buffer* meshBuffer = nullptr;

static Buffer* partBuffer = nullptr;
static Buffer* partHandleBuffer = nullptr;
static Buffer* partMatrixBuffer = nullptr;

static Buffer* commandBuffer = nullptr;

static VAO* vao = nullptr;

static ShaderProgram* commandBuilderShader = nullptr;
static ShaderProgram* matrixBuilderShader = nullptr;
static ShaderProgram* renderShader = nullptr;

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
*           uint(geometry_first),
*           uint(surface_first),
*           uint(vertex_count),
*           uint(padding)
            }(handle))
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
*               uint(geometry_first),
*               uint(surface_first),
*               uint(vertex_count),
*               uint(padding)
*               }(mesh_handle)->meshBuffer(handle)
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
* =================================
*/

void ikEcsInit() {
    const GLsizeiptr defultBufferSize = 1000000; // 1Mb

    geometryBuffer =   new Buffer(defultBufferSize);
    surfaceBuffer =    new Buffer(defultBufferSize);
    meshBuffer =       new Buffer(defultBufferSize);
    partBuffer =       new Buffer(defultBufferSize);
    partHandleBuffer = new Buffer(defultBufferSize);
    partMatrixBuffer = new Buffer(defultBufferSize);
    commandBuffer =    new Buffer(defultBufferSize);


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


    commandBuilderShader = new ShaderProgram;
    commandBuilderShader->addShader(GL_COMPUTE_SHADER, readFileToString("command_builder_shader.comp"));
    commandBuilderShader->compile();

    matrixBuilderShader = new ShaderProgram;
    matrixBuilderShader->addShader(GL_COMPUTE_SHADER, readFileToString("matrix_builder_shader.comp"));
    matrixBuilderShader->compile();

    renderShader = new ShaderProgram;
    renderShader->addShader(GL_VERTEX_SHADER, readFileToString("render_shader.vert"));
    renderShader->addShader(GL_FRAGMENT_SHADER, readFileToString("render_shader.frag"));
    renderShader->compile();
}