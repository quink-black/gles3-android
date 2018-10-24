#include "opengl-helper.h"

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <string.h>

#include <vector>

#include "log.h"

#ifndef GL_DEBUG_OUTPUT
#define GL_DEBUG_OUTPUT 0x92E0
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
    (void)source;
    (void)id;
    (void)length;
    (void)userParam;
    ALOGE("GL CALLBACK: type = 0x%x, severity = 0x%x, message = %s", type, severity, message);
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
