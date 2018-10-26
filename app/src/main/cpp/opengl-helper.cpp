#include "opengl-helper.h"

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <string.h>

#include <vector>

#include "log.h"

#ifndef GL_DEBUG_OUTPUT
#define GL_DEBUG_OUTPUT 0x92E0
#endif

#ifndef GL_DEBUG_SOURCE_API
#define GL_DEBUG_SOURCE_API               0x8246
#endif
#ifndef GL_DEBUG_SOURCE_WINDOW_SYSTEM
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM     0x8247
#endif
#ifndef GL_DEBUG_SOURCE_SHADER_COMPILER
#define GL_DEBUG_SOURCE_SHADER_COMPILER   0x8248
#endif
#ifndef GL_DEBUG_SOURCE_THIRD_PARTY
#define GL_DEBUG_SOURCE_THIRD_PARTY       0x8249
#endif
#ifndef GL_DEBUG_SOURCE_APPLICATION
#define GL_DEBUG_SOURCE_APPLICATION       0x824A
#endif
#ifndef GL_DEBUG_SOURCE_OTHER
#define GL_DEBUG_SOURCE_OTHER             0x824B
#endif
#ifndef GL_DEBUG_TYPE_ERROR
#define GL_DEBUG_TYPE_ERROR               0x824C
#endif
#ifndef GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR 0x824D
#endif
#ifndef GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR  0x824E
#endif
#ifndef GL_DEBUG_TYPE_PORTABILITY
#define GL_DEBUG_TYPE_PORTABILITY         0x824F
#endif
#ifndef GL_DEBUG_TYPE_PERFORMANCE
#define GL_DEBUG_TYPE_PERFORMANCE         0x8250
#endif
#ifndef GL_DEBUG_TYPE_OTHER
#define GL_DEBUG_TYPE_OTHER               0x8251
#endif
#ifndef GL_DEBUG_SEVERITY_HIGH
#define GL_DEBUG_SEVERITY_HIGH            0x9146
#endif
#ifndef GL_DEBUG_SEVERITY_MEDIUM
#define GL_DEBUG_SEVERITY_MEDIUM          0x9147
#endif
#ifndef GL_DEBUG_SEVERITY_LOW
#define GL_DEBUG_SEVERITY_LOW             0x9148
#endif
#ifndef GL_DEBUG_SEVERITY_NOTIFICATION
#define GL_DEBUG_SEVERITY_NOTIFICATION    0x826B
#endif

void OpenGL_Helper::PrintGLString(const char* name, int s) {
    const char* v = (const char*)glGetString(s);
    ALOGD("GL %s: %s", name, v);
}

void OpenGL_Helper::PrintGLExtension(void) {
    ALOGD("========= OpenGL extensions begin ========");
    char *exts = strdup((const char *)glGetString(GL_EXTENSIONS));
    char *saveptr;
    char *token = strtok_r(exts, " ", &saveptr);
    while (token) {
        ALOGD("%s", token);
        token = strtok_r(nullptr, " ", &saveptr);
    }
    free(exts);
    ALOGD("========= OpenGL extensions end ========");
}

static void openGLMessageCallback(GLenum source, GLenum type, GLuint id,
        GLenum severity, GLsizei length, const GLchar* message,
        const void* userParam)
{
    const char *source_str;
    switch (source) {
        case GL_DEBUG_SOURCE_API:
            source_str = "api";
            break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
            source_str = "window system";
            break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER:
            source_str = "shader compiler";
            break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:
            source_str = "third party";
            break;
        case GL_DEBUG_SOURCE_APPLICATION:
            source_str = "application";
            break;
        case GL_DEBUG_SOURCE_OTHER:
        default:
            source_str = "other";
            break;
    }

    const char *type_str;
    switch (type) {
        case GL_DEBUG_TYPE_ERROR:
            type_str = "error";
            break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            type_str = "deprecated behavior";
            break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            type_str = "undefined behavior";
            break;
        case GL_DEBUG_TYPE_PORTABILITY:
            type_str = "portability";
            break;
        case GL_DEBUG_TYPE_PERFORMANCE:
            type_str = "performance";
            break;
        case GL_DEBUG_TYPE_OTHER:
            type_str = "other";
        default:
            break;
    }

    const char *severity_str;
    switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH:
            severity_str = "high";
            break;
        case GL_DEBUG_SEVERITY_MEDIUM:
            severity_str = "medium";
            break;
        case GL_DEBUG_SEVERITY_LOW:
            severity_str = "low";
            break;
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            severity_str = "notification";
            break;
    }

    (void)id;
    (void)length;
    (void)userParam;
    ALOGE("GL CALLBACK: source [%s], type [%s], severity [%s], message [%s]",
            source_str, type_str, severity_str, message);
}

bool OpenGL_Helper::SetupDebugCallback(void) {
    bool ret = false;
    auto debugCallback  = (void (*)(void *, void *))eglGetProcAddress("glDebugMessageCallback");
    if (debugCallback) {
        glEnable(GL_DEBUG_OUTPUT);
        debugCallback((void*)openGLMessageCallback, nullptr);
        ret = true;
    }
    ALOGD("setup opengl message callback %s", ret ? "success" : "failed");
    return ret;
}

bool (OpenGL_Helper::CheckGLError)(const char *file, const char *func, int line) {
    GLint err = glGetError();
    if (err != GL_NO_ERROR) {
        ALOGE("GL error at (%s, %s, %d): 0x%x", file, func, line, err);
        return true;
    }
    return false;
}

bool OpenGL_Helper::CheckGLExtension(const char *api) {
    const char *apis = (const char *)glGetString(GL_EXTENSIONS);
    size_t apilen = strlen(api);
    while (apis) {
        while (*apis == ' ')
            apis++;
        if (!strncmp(apis, api, apilen) && memchr(" ", apis[apilen], 2))
            return true;
        apis = strchr(apis, ' ');
    }
    return false;
}

unsigned int OpenGL_Helper::CreateShader(int shaderType, const char *src) {
    GLuint shader = glCreateShader(shaderType);
    if (!shader) {
        CheckGLError();
        return 0;
    }
    glShaderSource(shader, 1, &src, nullptr);

    GLint compiled = GL_FALSE;
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled)
        return shader;
    // something going wrong
    GLint infoLogLen = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLen);
    if (infoLogLen > 0) {
        std::vector<GLchar> infoLog(infoLogLen);
        glGetShaderInfoLog(shader, infoLogLen, nullptr, infoLog.data());
        const char *type = shaderType == GL_VERTEX_SHADER ? "vertex" : "fragment";
        ALOGE("could not compile %s shader:\n%s", type, infoLog.data());
    }
    glDeleteShader(shader);
    return 0;
}

unsigned int OpenGL_Helper::CreateProgram(const char *vtxSrc, const char *fragSrc) {
    GLuint vtxShader = 0;
    GLuint fragShader = 0;
    GLuint program = 0;
    GLint linked = GL_FALSE;

    vtxShader = CreateShader(GL_VERTEX_SHADER, vtxSrc);
    if (!vtxShader)
        goto exit;
    fragShader = CreateShader(GL_FRAGMENT_SHADER, fragSrc);
    if (!fragShader)
        goto exit;

    program = glCreateProgram();
    if (!program) {
        CheckGLError();
        goto exit;
    }
    glAttachShader(program, vtxShader);
    glAttachShader(program, fragShader);

    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        ALOGE("could not link program");
        GLint infoLogLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLen);
        if (infoLogLen) {
            std::vector<GLchar> infoLog(infoLogLen);
            glGetProgramInfoLog(program, infoLogLen, nullptr, infoLog.data());
            ALOGE("%s", infoLog.data());
        }
        glDeleteProgram(program);
        program = 0;
    }

exit:
    glDeleteShader(vtxShader);
    glDeleteShader(fragShader);
    return program;
}
