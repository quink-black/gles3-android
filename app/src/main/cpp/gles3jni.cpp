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

#include "perf-monitor.h"
#include "log.h"
#include "tonemap.h"
#include "opengl-helper.h"

static ToneMap *g_Plain;
static ToneMap *g_Hable;
static PerfMonitor *g_Upload[2];
static PerfMonitor *g_Draw[2];
static PerfMonitor *g_Fps[2];

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

static std::array<std::shared_ptr<ImageDecoder>, 2> GetImage() {
    static std::array<std::shared_ptr<ImageDecoder>, 2> imgs;
    if (imgs[0] == nullptr) {
        std::string texDataType("float");
#if 0
        bool hasFloatExt = OpenGL_Helper::CheckGLExtension("GL_OES_texture_float");
        if (!hasFloatExt)
            texDataType = "uint16_t";
#endif
        ALOGD("texture data type %s", texDataType.c_str());

        std::string path = getLibDirectory();
        std::string hdrImg = path + "/libhdr.so";
        std::string ldrImg = path + "/libldr.so";

        imgs[0] = ImageDecoder::Create(ImageType::pfm);
        if (imgs[0]->Decode(hdrImg, texDataType.c_str())) {
            imgs[0] = nullptr;
        }

        imgs[1] = ImageDecoder::Create(ImageType::common_ldr);
        if (imgs[1]->Decode(ldrImg, texDataType.c_str())) {
            imgs[1] = nullptr;
        }
    }

    return imgs;
}

static void CreatePerf() {
    for (int i = 0; i < 2; i++) {
        if (g_Upload[i] == nullptr) {
            g_Upload[i] = new PerfMonitor(100, [i](long long t) {
                ALOGD("[%d] upload takes %lld us", i + 1, t);
            });
        }
        if (g_Draw[i] == nullptr) {
            g_Draw[i] = new PerfMonitor(100, [i](long long t) {
                ALOGD("[%d] draw takes %lld us", i + 1, t);
            });
        }
        if (g_Fps[i] == nullptr) {
            g_Fps[i] = new PerfMonitor(100, [i](long long t) {
                ALOGD("[%d] fps %f", i + 1, 1000000.0 / t);
            });
        }
    }
}

JNIEXPORT void JNICALL
Java_com_android_gles3jni_GLES3JNILib_init(JNIEnv* env, jobject obj) {
    if (g_Plain) {
        delete g_Plain;
        g_Plain = nullptr;
    }
    if (g_Hable) {
        delete g_Hable;
        g_Hable = nullptr;
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

    ToneMap::ImageCoord coordA = {
        {-1.0f, 1.0f},
        {-1.0f, -1.0f},
        {0.0f, -1.0f},
        {0.0f, 1.0f},
    };
    ToneMap::ImageCoord coordB = {
        {0.0f, 1.0f},
        {0.0f, -1.0f},
        {1.0f, -1.0f},
        {1.0f, 1.0f},
    };

    g_Hable = ToneMap::CreateToneMap("Hable");
    if (g_Hable->Init(coordA))
        goto error;
    g_Plain = ToneMap::CreateToneMap("");
    if (g_Plain->Init(coordB))
        goto error;
    CreatePerf();

    return;

error:
    delete g_Plain;
    g_Plain = nullptr;;
    delete g_Hable;
    g_Hable = nullptr;
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
    if (1 && g_Hable) {
        auto t1 = std::chrono::steady_clock::now();
        g_Hable->UploadTexture(imgs[0]);
        auto t2 = std::chrono::steady_clock::now();
        g_Hable->Draw();
        auto t3 = std::chrono::steady_clock::now();
        g_Upload[0]->Update(t2 - t1);
        g_Draw[0]->Update(t3 - t2);
        g_Fps[0]->Update(t3);
    }
    if (1 && g_Plain) {
        auto t1 = std::chrono::steady_clock::now();
        g_Plain->UploadTexture(imgs[1]);
        auto t2 = std::chrono::steady_clock::now();
        g_Plain->Draw();
        auto t3 = std::chrono::steady_clock::now();
        g_Upload[1]->Update(t2 - t1);
        g_Draw[1]->Update(t3 - t2);
        g_Fps[1]->Update(t3);
    }
}
