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

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "log.h"
#include "tonemap.h"

#ifndef GL_DEBUG_OUTPUT
#define GL_DEBUG_OUTPUT 0x92E0
#endif

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

static void openGLMessageCallback(GLenum source, GLenum type, GLuint id,
        GLenum severity, GLsizei length, const GLchar* message,
        const void* userParam)
{
    (void)source;
    (void)id;
    (void)length;
    (void)userParam;
    ALOGE("GL CALLBACK: type = 0x%x, severity = 0x%x, message = %s", type, severity, message);
}

JNIEXPORT void JNICALL
Java_com_android_gles3jni_GLES3JNILib_init(JNIEnv* env, jobject obj) {
    if (g_renderer) {
        delete g_renderer;
        g_renderer = NULL;
    }

    printGlString("Version", GL_VERSION);
    printGlString("Vendor", GL_VENDOR);
    printGlString("Renderer", GL_RENDERER);
    printGlString("Extensions", GL_EXTENSIONS);

    auto debugCallback  = (void (*)(void *, void *))eglGetProcAddress("glDebugMessageCallback");
    if (debugCallback) {
        glEnable(GL_DEBUG_OUTPUT);
        debugCallback((void*)openGLMessageCallback, nullptr);
        ALOGD("setup opengl message callback");
    }

    const char* versionStr = (const char*)glGetString(GL_VERSION);
    if (strstr(versionStr, "OpenGL ES 3.")) {
        g_renderer = ToneMap::CreateToneMap();
        g_renderer->Init("/sdcard/test.exr");
    } else {
        ALOGE("Unsupported OpenGL ES version");
    }
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
