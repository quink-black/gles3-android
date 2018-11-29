#include "config.h"

#if HAVE_GLES
#   define GLFW_INCLUDE_ES3    1
#endif
#if HAVE_EGL
#   define GLFW_EXPOSE_NATIVE_EGL  1
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <algorithm>
#include <vector>

#include "perf-monitor.h"
#include "log.h"
#include "opengl-helper.h"
#include "tonemap.h"

#define WINDOW_WIDTH    1280
#define WINDOW_HEIGHT   720

#ifndef GL_DEBUG_OUTPUT
#define GL_DEBUG_OUTPUT 0x92E0
#endif

static std::vector<ToneMap::ImageCoord> GetCoord(int n, bool sideByside = true) {
    std::vector<ToneMap::ImageCoord> coords;
    const float left = -1.0f;
    const float right = 1.0f;
    const float top = 1.0f;
    const float bottom = -1.0f;
    const float width = right - left;
    const float height = top - bottom;
    if (sideByside) {
        for (int i = 0; i < n; ++i) {
            coords.push_back({
                    {left + (float)i / n * width, top},
                    {left + (float)i / n * width, bottom},
                    {left + (float)(i + 1) / n * width, bottom},
                    {left + (float)(i + 1) / n * width, top}
                   });
        }
    } else {    // top bottom
        for (int i = 0; i < n; ++i) {
            coords.push_back({
                    {left, top + (float) i / n * height},
                    {left, top + (float) (i + 1) / n * height},
                    {right, top + (float) (i + 1) / n * height},
                    {right, top + (float) i / n * height},
                   });
        }
    }
    return coords;
}

int main(int argc, char *argv[])
{
    (void)argc;
    glfwSetErrorCallback(
            [](int error, const char* description)
            { ALOGE("error: %d, %s\n", error, description); });

    if (!glfwInit())
        return 1;

#if HAVE_EGL
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
#endif
#if HAVE_GLES
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#elif HAVE_GL
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

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
            [](GLFWwindow *, int w, int h) {
                glViewport(0, 0, w, h);
            });
    glfwMakeContextCurrent(window);
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    OpenGL_Helper::PrintGLString("Version", GL_VERSION);
    OpenGL_Helper::PrintGLString("Vendor", GL_VENDOR);
    OpenGL_Helper::PrintGLString("Renderer", GL_RENDERER);
    OpenGL_Helper::PrintGLExtension();
    OpenGL_Helper::SetupDebugCallback();

    std::string texDataType = "float";
#if 0
    bool hasFloatExt = OpenGL_Helper::CheckGLExtension("GL_OES_texture_float");
    if (!hasFloatExt)
        texDataType = "uint16_t";
#endif
    ALOGD("texture data type %s", texDataType.c_str());

    std::vector<std::string> files;
    for (int i = 1; i < argc; i++) {
        files.push_back(argv[i]);
    }
    if (files.empty())
        files.push_back("Tree.exr");
    if (files.size() == 1)
        files.push_back(files[0]);

    std::vector<std::shared_ptr<ImageDecoder>> imgs;
    for (size_t i = 0; i < files.size(); i++) {
        auto img = ImageDecoder::Create(files[i].c_str());
        if (img->Decode(files[i], texDataType)) {
            ALOGE("cannot decode %s", files[i].c_str());
            return 1;
        }
        imgs.push_back(img);
    }

    bool sideByside = true;
#ifndef __APPLE__
    if (sideByside)
        glfwSetWindowSize(window, imgs[0]->mWidth * imgs.size(), imgs[0]->mHeight);
    else
        glfwSetWindowSize(window, imgs[0]->mWidth, imgs[0]->mHeight * imgs.size());
#endif
    const auto coords = GetCoord(imgs.size(), sideByside);

    std::vector<std::shared_ptr<ToneMap>> tonemaps;
    auto tonemap = std::shared_ptr<ToneMap>(ToneMap::CreateToneMap("Hable"));
    if (tonemap->Init(coords[0]))
        return 1;
    tonemaps.push_back(tonemap);

    for (size_t i = 1; i < imgs.size(); i++) {
        tonemap = std::shared_ptr<ToneMap>(ToneMap::CreateToneMap(""));
        if (tonemap->Init(coords[i]))
            return 1;
        tonemaps.push_back(tonemap);
    }

    PerfMonitor uploadPerf(100, [](long long t) {
                ALOGD("hable upload takes %lld us", t);
            });
    PerfMonitor drawPerf(100, [](long long t) {
                ALOGD("hable draw takes %lld us", t);
            });
    PerfMonitor fps(100, [](long long t) {
                ALOGD("fps %f", 1000000.0 / t);
            });
    while (!glfwWindowShouldClose(window)) {
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        auto t1 = std::chrono::steady_clock::now();

        tonemaps[0]->UploadTexture(imgs[0]);
        auto t2 = std::chrono::steady_clock::now();
        tonemaps[0]->Draw();
        auto t3 = std::chrono::steady_clock::now();

        for (size_t i = 1; i < imgs.size(); i++) {
            tonemaps[i]->UploadTexture(imgs[i]);
            tonemaps[i]->Draw();
        }

        uploadPerf.Update(t2 -t1);
        drawPerf.Update(t3 - t2);

        glfwSwapBuffers(window);
        glfwPollEvents();
        fps.Update(std::chrono::steady_clock::now());
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
