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

#include "log.h"
#include "tonemap.h"
#include "opengl-helper.h"

static ToneMap *g_Plain;
static ToneMap *g_Hable;

static inline void printGlString(const char* name, GLenum s) {
    const char* v = (const char*)glGetString(s);
    ALOGD("GL %s: %s", name, v);
}

extern "C" {
    JNIEXPORT void JNICALL Java_com_android_gles3jni_GLES3JNILib_init(JNIEnv* env, jobject obj);
    JNIEXPORT void JNICALL Java_com_android_gles3jni_GLES3JNILib_resize(JNIEnv* env, jobject obj, jint width, jint height);
    JNIEXPORT void JNICALL Java_com_android_gles3jni_GLES3JNILib_render(JNIEnv* env, jobject obj);
};

static std::shared_ptr<ImageDecoder> GetImage() {
    static std::shared_ptr<ImageDecoder> img(nullptr);
    if (img == nullptr) {
        bool hasFloatExt = OpenGL_Helper::CheckGLExtension("GL_EXT_color_buffer_float");
        std::string texDataType("float");
        if (!hasFloatExt)
            texDataType = "uint16_t";
        ALOGD("texture data type %s", texDataType.c_str());

        img = ImageDecoder::CreateImageDecoder("OpenEXR");
        if (img->Decode("/sdcard/test.exr", texDataType.c_str())) {
            img = nullptr;
        }
    }
    return img;
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
    if (g_Hable) {
        g_Hable->UploadTexture(GetImage());
        g_Hable->Draw();
    }
    if (g_Plain) {
        g_Plain->UploadTexture(GetImage());
        g_Plain->Draw();
    }
}
