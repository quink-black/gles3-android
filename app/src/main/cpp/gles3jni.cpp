/*
 * Copyright 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <GLES3/gl3.h>
#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <array>
#include <fstream>
#include <memory>
#include <string>

#include "image_decoder.h"
#include "image_merge.h"
#include "tonemapper.h"
#include "log.h"
#include "opengl-helper.h"
#include "perf-monitor.h"
#include "render.h"

using namespace quink;

static std::array<Render*, 2> g_Renders;

extern "C" {
    JNIEXPORT void JNICALL Java_com_android_gles3jni_GLES3JNILib_init(JNIEnv* env, jobject obj);
    JNIEXPORT void JNICALL Java_com_android_gles3jni_GLES3JNILib_resize(JNIEnv* env, jobject obj, jint width, jint height);
    JNIEXPORT void JNICALL Java_com_android_gles3jni_GLES3JNILib_render(JNIEnv* env, jobject obj);
};

static std::string getLibDirectory() {
    std::string path;
    std::ifstream maps("/proc/self/maps");

    uintptr_t needle = (uintptr_t)Java_com_android_gles3jni_GLES3JNILib_init;

    for (std::string line; std::getline(maps, line);) {
        void *start, *end;
        if (sscanf(line.c_str(), "%p-%p", &start, &end) < 2)
            continue;
        /* This mapping contains the address of this function. */
        if (needle < (uintptr_t)start || (uintptr_t)end <= needle)
            continue;

        auto dirPos = line.find('/');
        if (dirPos == std::string::npos)
            continue;
        auto filePos = line.rfind('/');
        if (filePos != dirPos) {
            path = line.substr(dirPos, filePos - dirPos);
        } else {
            path = "/";
        }
        break;
    }

    ALOGD("library path %s", path.c_str());
    return path;
}

using ImageGroup = std::pair<std::shared_ptr<Image<uint8_t>>, std::shared_ptr<Image<float>>>;
static ImageGroup GetImage() {
    static ImageGroup imageGroup;

    if (imageGroup.first == nullptr || imageGroup.second == nullptr) {
        std::shared_ptr<Image<uint8_t>> imgs[2];

        std::string path = getLibDirectory();

        auto t1 = std::chrono::high_resolution_clock::now();
        std::string file = path + "/liblow.so";
        auto imgWrapper = ImageLoader::LoadImage(file);
        if (imgWrapper.Empty()) {
            ALOGE("cannot decode %s", file.c_str());
            return ImageGroup();
        }
        imgs[0] = imgWrapper.GetImg<uint8_t>();
        auto t2 = std::chrono::high_resolution_clock::now();
        ALOGD("decode %s takes %lld ms", file.c_str(),
                (long long)std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());

        t1 = std::chrono::high_resolution_clock::now();
        file = path + "/libhigh.so";
        imgWrapper = ImageLoader::LoadImage(file);
        if (imgWrapper.Empty()) {
            ALOGE("cannot decode %s", file.c_str());
            return ImageGroup();
        }
        imgs[1] = imgWrapper.GetImg<uint8_t>();
        t2 = std::chrono::high_resolution_clock::now();
        ALOGD("decode %s takes %lld ms", file.c_str(),
                (long long)std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());

        t1 = std::chrono::high_resolution_clock::now();
        auto imgNew = ImageMerge::Merge<float>(imgs[0], imgs[1]);
        t2 = std::chrono::high_resolution_clock::now();
        ALOGD("merge two picture takes %lld ms",
                (long long)std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());

        imageGroup.first = imgs[0];
        imageGroup.second = imgNew;
    }

    return imageGroup;
}

JNIEXPORT void JNICALL
Java_com_android_gles3jni_GLES3JNILib_init(JNIEnv* env, jobject obj) {
    for (int i = 0; i < g_Renders.size(); i++) {
        delete g_Renders[i];
        g_Renders[i] = nullptr;
    }

    OpenGL_Helper::PrintGLString("Version", GL_VERSION);
    OpenGL_Helper::PrintGLString("Vendor", GL_VENDOR);
    OpenGL_Helper::PrintGLString("Renderer", GL_RENDERER);
    OpenGL_Helper::PrintGLExtension();
    OpenGL_Helper::SetupDebugCallback();

    const char* versionStr = (const char*)glGetString(GL_VERSION);
    if (!strstr(versionStr, "OpenGL ES 3.")) {
        ALOGE("Unsupported OpenGL ES version");
        return;
    }

    Render::ImageCoord coordA = {
        {-1.0f, 1.0f},
        {-1.0f, -1.0f},
        {0.0f, -1.0f},
        {0.0f, 1.0f},
    };
    Render::ImageCoord coordB = {
        {0.0f, 1.0f},
        {0.0f, -1.0f},
        {1.0f, -1.0f},
        {1.0f, 1.0f},
    };

    g_Renders[0] = Render::Create("Plain");
    g_Renders[0]->Init(coordA);
    g_Renders[1] = Render::Create("Hable");
    g_Renders[1]->Init(coordB);
 
    return;
}

JNIEXPORT void JNICALL
Java_com_android_gles3jni_GLES3JNILib_resize(JNIEnv* env, jobject obj, jint width, jint height) {
    glViewport(0, 0, width, height);
}

JNIEXPORT void JNICALL
Java_com_android_gles3jni_GLES3JNILib_render(JNIEnv* env, jobject obj) {
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const auto imgs = GetImage();

    static PerfMonitor perf[4] = {
        PerfMonitor(30, [](long long dura) { ALOGD("[1] update takes %f ms", dura/1000.0); }),
        PerfMonitor(30, [](long long dura) { ALOGD("[1] draw takes %f ms", dura/1000.0); }),
        PerfMonitor(30, [](long long dura) { ALOGD("[2] update takes %f ms", dura/1000.0); }),
        PerfMonitor(30, [](long long dura) { ALOGD("[2] draw takes %f ms", dura/1000.0); }),
    };

    std::chrono::high_resolution_clock::time_point t1, t2, t3;
    if (imgs.first) {
        t1 = std::chrono::high_resolution_clock::now();
        g_Renders[0]->UploadTexture(imgs.first);
        t2 = std::chrono::high_resolution_clock::now();

        g_Renders[0]->Draw();
        t3 = std::chrono::high_resolution_clock::now();

        perf[0].Update(t2 - t1);
        perf[1].Update(t3 - t2);
    }

    if (imgs.second) {
        t1 = std::chrono::high_resolution_clock::now();
        g_Renders[1]->UploadTexture(imgs.second);
        t2 = std::chrono::high_resolution_clock::now();

        g_Renders[1]->Draw();
        t3 = std::chrono::high_resolution_clock::now();

        perf[2].Update(t2 - t1);
        perf[3].Update(t3 - t2);
    }
}
