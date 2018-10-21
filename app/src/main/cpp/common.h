#ifndef SHADER_H
#define SHADER_H

#include <android/log.h>
#include <math.h>
#include <stdlib.h>

#define DEBUG 1

#if DYNAMIC_ES3
#   include "gl3stub.h"
#else
    // Include the latest possible header file( GL version header )
    #if __ANDROID_API__ >= 24
    #   include <GLES3/gl32.h>
    #elif __ANDROID_API__ >= 21
    #   include <GLES3/gl31.h>
    #else
    #   include <GLES3/gl3.h>
    #endif
#endif

#define LOG_TAG "Quink"
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#if DEBUG
#define ALOGD(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#else
#define ALOGD(...)
#endif

// returns true if a GL error occurred
static inline bool checkGlError(const char* funcName, int line = -1)
{
    GLint err = glGetError();
    if (err != GL_NO_ERROR) {
        ALOGE("GL error after (%s, %d): 0x%08x\n", funcName, line, err);
        return true;
    }
    return false;
}

#define checkGlError()  checkGlError(__func__, __LINE__)

static inline GLuint createShader(GLenum shaderType, const char* src)
{
    GLuint shader = glCreateShader(shaderType);
    if (!shader) {
        checkGlError();
        return 0;
    }
    glShaderSource(shader, 1, &src, NULL);

    GLint compiled = GL_FALSE;
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint infoLogLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLen);
        if (infoLogLen > 0) {
            GLchar* infoLog = (GLchar*)malloc(infoLogLen);
            if (infoLog) {
                glGetShaderInfoLog(shader, infoLogLen, NULL, infoLog);
                ALOGE("Could not compile %s shader:\n%s\n",
                        shaderType == GL_VERTEX_SHADER ? "vertex" : "fragment",
                        infoLog);
                free(infoLog);
            }
        }
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static inline GLuint createProgram(const char* vtxSrc, const char* fragSrc)
{
    GLuint vtxShader = 0;
    GLuint fragShader = 0;
    GLuint program = 0;
    GLint linked = GL_FALSE;

    vtxShader = createShader(GL_VERTEX_SHADER, vtxSrc);
    if (!vtxShader)
        goto exit;

    fragShader = createShader(GL_FRAGMENT_SHADER, fragSrc);
    if (!fragShader)
        goto exit;

    program = glCreateProgram();
    if (!program) {
        checkGlError();
        goto exit;
    }
    glAttachShader(program, vtxShader);
    glAttachShader(program, fragShader);

    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        ALOGE("Could not link program");
        GLint infoLogLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLen);
        if (infoLogLen) {
            GLchar* infoLog = (GLchar*)malloc(infoLogLen);
            if (infoLog) {
                glGetProgramInfoLog(program, infoLogLen, NULL, infoLog);
                ALOGE("Could not link program:\n%s\n", infoLog);
                free(infoLog);
            }
        }
        glDeleteProgram(program);
        program = 0;
    }

exit:
    glDeleteShader(vtxShader);
    glDeleteShader(fragShader);
    return program;
}

static inline void printGlString(const char* name, GLenum s) {
    const char* v = (const char*)glGetString(s);
    ALOGD("GL %s: %s\n", name, v);
}

#endif
