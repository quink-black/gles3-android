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
static PerfMonitor *g_Upload;
static PerfMonitor *g_Draw;
static PerfMonitor *g_Fps;

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
        bool hasFloatExt = OpenGL_Helper::CheckGLExtension("GL_EXT_color_buffer_float");
        std::string texDataType("float");
        if (!hasFloatExt)
            texDataType = "uint16_t";
        ALOGD("texture data type %s", texDataType.c_str());

        std::string path = getLibDirectory();
        std::string hdrImg = path + "/libhdr.so";
        std::string ldrImg = path + "/libldr.so";

        imgs[0] = ImageDecoder::CreateByType("pfm");
        if (imgs[0]->Decode(hdrImg.c_str(), texDataType.c_str())) {
            imgs[0] = nullptr;
        }

        imgs[1] = ImageDecoder::CreateByType("ldr");
        if (imgs[1]->Decode(ldrImg.c_str(), texDataType.c_str())) {
            imgs[1] = nullptr;
        }
    }

    return imgs;
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

    if (g_Upload == nullptr) {
        g_Upload = new PerfMonitor(100, [](long long t) {
                ALOGD("upload takes %lld us", t);
            });
    }
    if (g_Draw == nullptr) {
        g_Draw = new PerfMonitor(100, [](long long t) {
                ALOGD("draw takes %lld us", t);
            });
    }
    if (g_Fps == nullptr) {
        g_Fps = new PerfMonitor(100, [](long long t) {
                ALOGD("fps %f", 1000000.0 / t);
            });
    }

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
    if (g_Hable) {
        auto t1 = std::chrono::steady_clock::now();
        g_Hable->UploadTexture(imgs[0]);
        auto t2 = std::chrono::steady_clock::now();
        g_Hable->Draw();
        auto t3 = std::chrono::steady_clock::now();
        g_Upload->Update(t2 - t1);
        g_Draw->Update(t3 - t2);
        g_Fps->Update(t3);
    }
    if (g_Plain) {
        g_Plain->UploadTexture(imgs[1]);
        g_Plain->Draw();
    }
}
