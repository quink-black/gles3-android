#define GLFW_INCLUDE_ES3    1
#define GLFW_EXPOSE_NATIVE_EGL  1
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "log.h"
#include "tonemap.h"

#define WINDOW_WIDTH    1280
#define WINDOW_HEIGHT   720

#ifndef GL_DEBUG_OUTPUT
#define GL_DEBUG_OUTPUT 0x92E0
#endif

static inline void printGlString(const char* name, GLenum s) {
    const char* v = (const char*)glGetString(s);
    ALOGD("GL %s: %s", name, v);
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

int main(int argc, char *argv[])
{
    (void)argc;
    glfwSetErrorCallback(
            [](int error, const char* description)
            { ALOGE("error: %d, %s\n", error, description); });

    if (!glfwInit())
        return 1;

    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    GLFWwindow *window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, argv[0], nullptr, nullptr);
    if (!window)
        return 1;

    glfwSetKeyCallback(window,
            [](GLFWwindow* window, int key, int scancode, int action, int mods) {
                (void)scancode;
                (void)mods;
                if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
            });
    glfwSetWindowSizeCallback(window,
            [](GLFWwindow *, int w, int h) { glViewport(0, 0, w, h); });
    glfwMakeContextCurrent(window);
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    auto debugCallback  = (void (*)(void *, void *))eglGetProcAddress("glDebugMessageCallback");
    if (debugCallback) {
        glEnable(GL_DEBUG_OUTPUT);
        debugCallback((void*)openGLMessageCallback, nullptr);
    }

    printGlString("Version", GL_VERSION);
    printGlString("Vendor", GL_VENDOR);
    printGlString("Renderer", GL_RENDERER);
    //printGlString("Extensions", GL_EXTENSIONS);

    ToneMap *toneMap = ToneMap::CreateToneMap();
    if (toneMap->Init("test.exr"))
        return 1;

    while (!glfwWindowShouldClose(window)) {
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        toneMap->Draw();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    delete toneMap;

    glfwTerminate();
    return 0;
}
