#pragma once

// open GL
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <gtc/type_ptr.hpp>

// textures
#include "std.h"

// Time management
#include <chrono>

// Random number generation
#include <random>

// imGUI (UI)
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

// math
#include <cmath>
#include <complex>
#include <valarray>
#include <algorithm>
#include <numeric>
#include <glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <gtx/quaternion.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtx/euler_angles.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// containers
#include <memory>
#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <array>

// utilities
#include <thread>
#include <iostream>
