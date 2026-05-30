/*
IKAROS Engine - Error System Implementation
Author: Branislav Wilhelm

note:
	this file is only for variables, not logic
*/

#include "error_wrapper.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <vector>
#include <array>
#include <string>
#include <iostream>
#include <fstream>

// ============================================================
//  Internal State (translation-unit local)
// ============================================================

static std::vector<Error> errorLog;

static const char* lastFile = nullptr;
static int         lastLine = 0;
static const char* lastFunc = nullptr;

// ============================================================
//  Error Position
// ============================================================

void logErrorPosition(const char* file, int line, const char* func) {
    lastFile = file;
    lastLine = line;
    lastFunc = func;
}

// ============================================================
//  Error Implementation
// ============================================================

Error::Error()
    : system(UNKNOWN_SYSTEM),
    source(UNKNOWN_SOURCE),
    type(LOG),
    severity(SEVERITY_NONE),
    code(UNKNOWN_CODE),
    file(nullptr),
    line(0),
    function(nullptr) {
}

Error::Error(ERRsystem system, ERRsource source, ERRtype type,
    ERRseverity severity, ERRcode code)
    : system(system),
    source(source),
    type(type),
    severity(severity),
    code(code),
    file(lastFile),
    line(lastLine),
    function(lastFunc) {
    logError(*this);
}

Error::Error(ERRsystem system, ERRsource source, ERRtype type,
    ERRseverity severity, ERRcode code,
    std::string message)
    : system(system),
    source(source),
    type(type),
    severity(severity),
    code(code),
    file(lastFile),
    line(lastLine),
    function(lastFunc),
    message(std::move(message)) {
    logError(*this);
}

Error::Error(ERRsystem system, ERRsource source, ERRtype type,
    ERRseverity severity, ERRcode code,
    const char* file, int line, const char* function,
    std::string message)
    : system(system),
    source(source),
    type(type),
    severity(severity),
    code(code),
    message(std::move(message)),
    file(file),
    line(line),
    function(function) {
    logError(*this);
}

// ============================================================
//  Logging
// ============================================================

void outputError(const Error& error) {
    std::cerr
        << "[" << systemToString(error.system) << "]" << std::endl
        << "Source: " << sourceToString(error.source) << std::endl
        << "Type: " << typeToString(error.type) << std::endl
        << "Severity: " << severityToString(error.severity) << std::endl
        << "code: " << codeToString(error.code) << std::endl
        << "message: " << error.message << std::endl
        << "file: " << error.file
        << " | line: " << error.line
        << " | function: " << error.function
        << std::endl << std::endl;
}

void logError(const Error& error) {
    errorLog.push_back(error);
    outputError(error);
    if (error.severity == SEVERITY_HIGH)
        throw std::runtime_error("hehe");
}

// ============================================================
//  Enum ? String
// ============================================================

std::string systemToString(ERRsystem system) {
    switch (system) {
    case OPENGL: return "OPENGL";
    case GLFW: return "GLFW";
    case ENGINE_GRAPHICS: return "IKAROS - GRAPHICS";
    case ENGINE_OBJECTS: return "IKAROS - OBJECTS";
    case USER: return "USER";
    case MISC: return "MISC";
    case UNKNOWN_SYSTEM: return "UNKNOWN_SYSTEM";
    default: return "UNKNOWN";
    }
}

std::string sourceToString(ERRsource source) {
    switch (source) {
    case UNKNOWN_SOURCE: return "UNKNOWN_SOURCE";
    case BUFFER: return "BUFFER";
    case VERTEX_ARRAY: return "VERTEX_ARRAY";
    case SHADER_PROGRAM: return "SHADER_PROGRAM";
    case WINDOW: return "WINDOW";
    case HELPER_FUNC: return "HELPER_FUNC";
    case ALLOCATION_POOL: return "ALLOCATION_POOL";
    case RING_BUFFER: return "RING_BUFFER";
    default: return "UNKNOWN_SOURCE";
    }
}

std::string typeToString(ERRtype type) {
    switch (type) {
    case LOG: return "LOG";
    case NOTIFICATION: return "NOTIFICATION";
    case WARNING: return "WARNING";
    case ERROR: return "ERROR";
    default: return "UNKNOWN";
    }
}

std::string severityToString(ERRseverity severity) {
    switch (severity) {
    case SEVERITY_NONE: return "NONE";
    case SEVERITY_LOW: return "LOW";
    case SEVERITY_MEDIUM: return "MEDIUM";
    case SEVERITY_HIGH: return "HIGH";
    case SEVERITY_UNKNOWN: return "UNKNOWN";
    default: return "UNKNOWN";
    }
}

std::string codeToString(ERRcode code) {
    switch (code) {
    case UNKNOWN_CODE: return "UNKNOWN_CODE";
    case INVALID_POINTER: return "INVALID_POINTER";
    case INVALID_VALUE: return "INVALID_VALUE";
    case INVALID_HANDLE: return "INVALID_HANDLE";
    case INVALID_FUNCTION_CALL: return "INVALID_FUNCTION_CALL";
    case OUT_OF_RANGE_VALUE: return "OUT_OF_RANGE_VALUE";
    case FILE_NOT_OPEN: return "FILE_NOT_OPEN";
    case VALUE_NOT_FOUND: return "VALUE_NOT_FOUND";
    case UNINITIALIZED_SOURCE: return "UNINITIALIZED_SOURCE";
    case CHANGED_VALUE: return "CHANGED_VALUE";
    default: return "UNKNOWN_CODE";
    }
}