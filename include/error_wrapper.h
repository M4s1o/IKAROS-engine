/*
=========== IKAROS Engine - Error System ===========

Author: Branislav Wilhelm
Started: December 12, 2025 (12.12.2025)
Finished:

Versions:
	C++17
	openGL 4.5
	GLFW 3.x
*/

#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <vector>
#include <array>
#include <string>
#include <iostream>
#include <fstream>

// ============================================================
//  Error Enums
// ============================================================

enum ERRsystem {
    OPENGL,
    GLFW,
    ENGINE_GRAPHICS,
    ENGINE_OBJECTS,
    USER,
    MISC,
    UNKNOWN_SYSTEM
};

enum ERRsource {
    UNKNOWN_SOURCE,
    BUFFER,
    VERTEX_ARRAY,
    SHADER_PROGRAM,
    WINDOW,
    HELPER_FUNC,
    ALLOCATION_POOL,
    RING_BUFFER
};

enum ERRtype {
    LOG,
    NOTIFICATION,
    WARNING,
    ERROR
};

enum ERRseverity {
    SEVERITY_NONE,
    SEVERITY_LOW,
    SEVERITY_MEDIUM,
    SEVERITY_HIGH,
    SEVERITY_UNKNOWN
};

enum ERRcode {
    UNKNOWN_CODE,
    INVALID_POINTER,
    INVALID_VALUE,
    INVALID_HANDLE,
    INVALID_FUNCTION_CALL,
    OUT_OF_RANGE_VALUE,
    FILE_NOT_OPEN,
    VALUE_NOT_FOUND,
    UNINITIALIZED_SOURCE,
    CHANGED_VALUE
};

// ============================================================
//  Error Structure
// ============================================================

struct Error {
    ERRsystem   system;
    ERRsource   source;
    ERRtype     type;
    ERRseverity severity;
    ERRcode     code;

    std::string message;
    const char* file;
    int         line;
    const char* function;

    Error();
    Error(ERRsystem system, ERRsource source, ERRtype type,
        ERRseverity severity, ERRcode code);

    Error(ERRsystem system, ERRsource source, ERRtype type,
        ERRseverity severity, ERRcode code,
        std::string message = {});

    Error(ERRsystem system, ERRsource source, ERRtype type,
        ERRseverity severity, ERRcode code,
        const char* file, int line, const char* function,
        std::string message = {});
};

// ============================================================
//  Logging API
// ============================================================

void logError(const Error& error);

// ============================================================
//  Enum ? String
// ============================================================

std::string systemToString(ERRsystem);
std::string sourceToString(ERRsource);
std::string typeToString(ERRtype);
std::string severityToString(ERRseverity);
std::string codeToString(ERRcode);

// ============================================================
//  Error Position Helpers
// ============================================================

void logErrorPosition(const char* file, int line, const char* func);

#define errorMark     logErrorPosition(__FILE__, __LINE__, __func__)
#define errorPosition __FILE__, __LINE__, __func__