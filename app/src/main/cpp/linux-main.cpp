#define GLFW_INCLUDE_ES3    1
#define GLFW_EXPOSE_NATIVE_EGL  1
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <algorithm>

#include "perf-monitor.h"
#include "log.h"
#include "opengl-helper.h"
#include "tonemap.h"

#define WINDOW_WIDTH    1280
#define WINDOW_HEIGHT   720

#ifndef GL_DEBUG_OUTPUT
#define GL_DEBUG_OUTPUT 0x92E0
#endif

static std::array<ToneMap::ImageCoord, 2> GetCoord(bool sideByside = true) {
    std::array<ToneMap::ImageCoord, 2> ret;
    if (sideByside) {
        ret[0] = {ToneMap::TopLeft(), ToneMap::BottomLeft(),
                  ToneMap::BottomMiddle(), ToneMap::TopMiddle()};
        ret[1] = {ToneMap::TopMiddle(), ToneMap::BottomMiddle(),
                  ToneMap::BottomRight(), ToneMap::TopRight()};
    } else {    // top bottom
        ret[0] = {ToneMap::TopLeft(), ToneMap::MiddleLeft(),
                  ToneMap::MiddleRight(), ToneMap::TopRight()};
        ret[1] = {ToneMap::MiddleLeft(), ToneMap::BottomLeft(),
                  ToneMap::BottomRight(), ToneMap::MiddleRight()};
    }
    return ret;
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

    OpenGL_Helper::PrintGLString("Version", GL_VERSION);
    OpenGL_Helper::PrintGLString("Vendor", GL_VENDOR);
    OpenGL_Helper::PrintGLString("Renderer", GL_RENDERER);
    OpenGL_Helper::PrintGLExtension();
    OpenGL_Helper::SetupDebugCallback();

    bool hasFloatExt = OpenGL_Helper::CheckGLExtension("GL_EXT_color_buffer_float");
    std::string texDataType = "float";
    if (!hasFloatExt)
        texDataType = "uint16_t";
    ALOGD("texture data type %s", texDataType.c_str());

    std::string filename1 = argc > 1 ? argv[1] : "Tree.exr";
    std::string filename2 = argc > 2 ? argv[2] : filename1;

    auto img1 = ImageDecoder::CreateByName(filename1.c_str());
    if (img1->Decode(filename1.c_str(), texDataType.c_str())) {
        return 1;
    }

    auto img2 = ImageDecoder::CreateByName(filename2.c_str());
    if (img2->Decode(filename2.c_str(), texDataType.c_str())) {
        return 1;
    }

    bool sideByside = true;
    /*
    if ((float)img1->mWidth / img1->mHeight > 16.0f / 9.0f)
        sideByside = false;
    */

    if (sideByside)
        glfwSetWindowSize(window, img1->mWidth * 2, img1->mHeight);
    else
        glfwSetWindowSize(window, img1->mWidth, img1->mHeight * 2);

    std::shared_ptr<ToneMap> hable(ToneMap::CreateToneMap("Hable"));
    const auto coords = GetCoord(sideByside);
    if (hable->Init(coords[0]))
        return 1;

    std::shared_ptr<ToneMap> plain(ToneMap::CreateToneMap(""));
    if (plain->Init(coords[1]))
        return 1;

    PerfMonitor hableUpload(100, [](long long t) {
                ALOGD("hable upload takes %lld us", t);
            });
    PerfMonitor hableDraw(100, [](long long t) {
                ALOGD("hable draw takes %lld us", t);
            });
    PerfMonitor plainUpload(100, [](long long t) {
                ALOGD("plain upload takes %lld us", t);
            });
    PerfMonitor plainDraw(100, [](long long t) {
                ALOGD("plain draw takes %lld us", t);
            });
    PerfMonitor fps(100, [](long long t) {
                ALOGD("fps %f", 1000000.0 / t);
            });
    while (!glfwWindowShouldClose(window)) {
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        auto t1 = std::chrono::steady_clock::now();

        hable->UploadTexture(img1);
        auto t2 = std::chrono::steady_clock::now();
        hable->Draw();
        auto t3 = std::chrono::steady_clock::now();

        plain->UploadTexture(img2);
        auto t4 = std::chrono::steady_clock::now();
        plain->Draw();
        auto t5 = std::chrono::steady_clock::now();

        hableUpload.Update(t2 -t1);
        hableDraw.Update(t3 - t2);
        plainUpload.Update(t4 - t3);
        plainDraw.Update(t5 - t4);

        glfwSwapBuffers(window);
        glfwPollEvents();
        fps.Update(std::chrono::steady_clock::now());
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
