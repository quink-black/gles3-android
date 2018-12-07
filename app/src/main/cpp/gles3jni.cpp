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

static std::array<std::shared_ptr<Image<uint8_t>>, 2> GetImage() {
    static std::array<std::shared_ptr<Image<uint8_t>>, 2> imgs;

    if (imgs[0] == nullptr || imgs[1] == nullptr) {
        std::string path = getLibDirectory();
        std::string files[2] = {path + "/liblow.so", path + "/libhigh.so"};
        for (int i = 0; i < imgs.size(); i++) {
            auto imgWrapper = ImageLoader::LoadImage(files[i]);
            if (imgWrapper.Empty()) {
                ALOGE("cannot decode %s", files[i].c_str());
                return imgs;
            }
            imgs[i] = imgWrapper.GetImg<uint8_t>();
        }

        auto imgNew = ImageMerge::Merge<float>(imgs[0], imgs[1]);
        HableMapper toneMapper;
        toneMapper.Map(imgNew);
        ImageWrapper wrap(imgNew);
        imgs[1] = wrap.GetImg<uint8_t>();
    }

    return imgs;
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

    Render::ImageCoord coords[2] = { coordA, coordB };

    for (int i = 0; i < g_Renders.size(); i++) {
        g_Renders[i] = Render::Create();
        g_Renders[i]->Init(coords[i]);
    }

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

    for (int i = 0; i < imgs.size(); i++) {
        g_Renders[i]->UploadTexture(imgs[i]);
        g_Renders[i]->Draw();
    }
}
