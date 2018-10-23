#define GLFW_INCLUDE_ES3    1
#include <GLFW/glfw3.h>

#include "log.h"
#include "tonemap.h"

#define WINDOW_WIDTH    1280
#define WINDOW_HEIGHT   720

static inline void printGlString(const char* name, GLenum s) {
    const char* v = (const char*)glGetString(s);
    ALOGD("GL %s: %s", name, v);
}

static void error_callback(int error, const char* description)
{
    ALOGE("error: %d, %s\n", error, description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    (void)scancode;
    (void)mods;
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

int main(int argc, char *argv[])
{
    (void)argc;
    glfwSetErrorCallback(error_callback);
    if (!glfwInit())
        return 1;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    GLFWwindow *window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, argv[0], nullptr, nullptr);
    if (!window)
        return 1;

    glfwSetKeyCallback(window, key_callback);
    glfwSetWindowSizeCallback(window,
            [](GLFWwindow *, int w, int h) { glViewport(0, 0, w, h); });
    glfwMakeContextCurrent(window);
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    printGlString("Version", GL_VERSION);
    printGlString("Vendor", GL_VENDOR);
    printGlString("Renderer", GL_RENDERER);

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
