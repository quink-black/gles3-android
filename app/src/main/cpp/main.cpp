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
#include <array>

#include "image_decoder.h"
#include "image_merge.h"
#include "tonemapper.h"
#include "log.h"
#include "perf-monitor.h"
#include "opengl-helper.h"
#include "render.h"

#define WINDOW_WIDTH    1280
#define WINDOW_HEIGHT   720

#ifndef GL_DEBUG_OUTPUT
#define GL_DEBUG_OUTPUT 0x92E0
#endif

using namespace quink;

static std::vector<Render::ImageCoord> GetCoord(int n, bool sideByside = true) {
    std::vector<Render::ImageCoord> coords;
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

    if (argc != 3)
        return 1;

    std::string files[2] = {argv[1], argv[2]};
    using ImageGroup = std::pair<std::shared_ptr<Image<uint8_t>>, std::shared_ptr<Image<float>>>;
    ImageGroup imageGroup;
    {
        std::array<std::shared_ptr<Image<uint8_t>>, 2> imgs;
        for (size_t i = 0; i < imgs.size(); i++) {
            auto imgWrapper = ImageLoader::LoadImage(files[i]);
            if (imgWrapper.Empty()) {
                ALOGE("cannot decode %s", files[i].c_str());
                return 1;
            }
            imgs[i] = imgWrapper.GetImg<uint8_t>();
        }
        auto imgNew = ImageMerge::Merge<float>(imgs[0], imgs[1]);
        imageGroup.first = imgs[0];
        imageGroup.second = imgNew;
    }

    bool sideByside = true;
#ifndef __APPLE__
    if (sideByside)
        glfwSetWindowSize(window, imageGroup.first->mWidth * 2, imageGroup.first->mHeight);
    else
        glfwSetWindowSize(window, imageGroup.first->mWidth, imageGroup.first->mHeight * 2);
#endif
    const auto coords = GetCoord(2, sideByside);

    std::array<std::shared_ptr<Render>, 2> renders;
    renders[0] = std::shared_ptr<Render>(Render::Create("Plain"));
    renders[1] = std::shared_ptr<Render>(Render::Create("Hable"));
    renders[0]->Init(coords[0]);
    renders[1]->Init(coords[1]);

    PerfMonitor perf[] = {
        PerfMonitor(100, [](long long t) { ALOGD("[1] upload takes %f ms", t/1000.0); }),
        PerfMonitor(100, [](long long t) { ALOGD("[1] draw takes %f ms", t/1000.0); }),
        PerfMonitor(100, [](long long t) { ALOGD("[2] upload takes %f ms", t/1000.0); }),
        PerfMonitor(100, [](long long t) { ALOGD("[2] draw takes %f ms", t/1000.0); }),
        PerfMonitor(100, [](long long t) { ALOGD("fps %f", 1000000.0 / t); }),
    };

    while (!glfwWindowShouldClose(window)) {
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        {
            auto t1 = std::chrono::high_resolution_clock::now();
            renders[0]->UploadTexture(imageGroup.first);
            auto t2 = std::chrono::high_resolution_clock::now();
            renders[0]->Draw();
            auto t3 = std::chrono::high_resolution_clock::now();
            perf[0].Update(t2 - t1);
            perf[1].Update(t3 - t2);
        }

        {
            auto t1 = std::chrono::high_resolution_clock::now();
            renders[1]->UploadTexture(imageGroup.second);
            auto t2 = std::chrono::high_resolution_clock::now();
            renders[1]->Draw();
            auto t3 = std::chrono::high_resolution_clock::now();
            perf[2].Update(t2 - t1);
            perf[3].Update(t3 - t2);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
        perf[4].Update(std::chrono::high_resolution_clock::now());
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
