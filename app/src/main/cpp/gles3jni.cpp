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

static ToneMap *g_renderer;

static inline void printGlString(const char* name, GLenum s) {
    const char* v = (const char*)glGetString(s);
    ALOGD("GL %s: %s", name, v);
}

extern "C" {
    JNIEXPORT void JNICALL Java_com_android_gles3jni_GLES3JNILib_init(JNIEnv* env, jobject obj);
    JNIEXPORT void JNICALL Java_com_android_gles3jni_GLES3JNILib_resize(JNIEnv* env, jobject obj, jint width, jint height);
    JNIEXPORT void JNICALL Java_com_android_gles3jni_GLES3JNILib_render(JNIEnv* env, jobject obj);
};

JNIEXPORT void JNICALL
Java_com_android_gles3jni_GLES3JNILib_init(JNIEnv* env, jobject obj) {
    if (g_renderer) {
        delete g_renderer;
        g_renderer = nullptr;
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

    bool hasFloatExt = OpenGL_Helper::CheckGLExtension("GL_EXT_color_buffer_float");
    std::string texDataType = "float";
    if (!hasFloatExt)
        texDataType = "uint16_t";
    ALOGD("texture data type %s", texDataType.c_str());

    auto img = ImageDecoder::CreateImageDecoder("OpenEXR");
    if (img->Decode("/sdcard/test.exr", texDataType.c_str()))
        return;

    g_renderer = ToneMap::CreateToneMap();
    if (g_renderer->Init())
        goto error;
    if (g_renderer->UpLoadTexture(img))
        goto error;
    return;
error:
    delete g_renderer;
    g_renderer = nullptr;;
}

JNIEXPORT void JNICALL
Java_com_android_gles3jni_GLES3JNILib_resize(JNIEnv* env, jobject obj, jint width, jint height) {
    glViewport(0, 0, width, height);
}

JNIEXPORT void JNICALL
Java_com_android_gles3jni_GLES3JNILib_render(JNIEnv* env, jobject obj) {
    if (g_renderer) {
        g_renderer->Draw();
    }
}
