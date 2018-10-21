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

#include "gles3jni.h"
#include "common.h"
#include <EGL/egl.h>

#define STR(s) #s
#define STRV(s) STR(s)

#define POS_ATTRIB 0
#define COLOR_ATTRIB 1

static const char VERTEX_SHADER[] =
    "#version 300 es\n"
    "layout(location = " STRV(POS_ATTRIB) ") in vec2 pos;\n"
    "layout(location=" STRV(COLOR_ATTRIB) ") in vec4 color;\n"
    "out vec4 vColor;\n"
    "void main() {\n"
    "    gl_Position = vec4(pos, 0.0, 1.0);\n"
    "    vColor = color;\n"
    "}\n";

static const char FRAGMENT_SHADER[] =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec4 vColor;\n"
    "out vec4 outColor;\n"
    "void main() {\n"
    "    outColor = vColor;\n"
    "}\n";

class RendererES3: public Renderer {
public:
    RendererES3();
    virtual ~RendererES3();
    bool init();

private:
    virtual void draw();

    const EGLContext mEglContext;
    GLuint mProgram;
    GLuint mVA;
    GLuint mVB;
};

Renderer* createES3Renderer() {
    RendererES3* renderer = new RendererES3;
    if (!renderer->init()) {
        delete renderer;
        return NULL;
    }
    return renderer;
}

RendererES3::RendererES3()
:   mEglContext(eglGetCurrentContext()),
    mProgram(0),
    mVA(0),
    mVB(0) {
}

bool RendererES3::init() {
    mProgram = createProgram(VERTEX_SHADER, FRAGMENT_SHADER);
    if (!mProgram)
        return false;

    static float vb_data[] = {
        0.0f,   0.5f,   0.0f,   1.0f,   0.0f,   1.0f,
        -0.5f,  -0.5f,  0.0f,   1.0f,   0.0f,   1.0f,
        0.5f,   -0.5f,  0.0f,   1.0f,   0.0f,   1.0f,
    };

    glGenVertexArrays(1, &mVA);
    glBindVertexArray(mVA);
    checkGlError();

    glGenBuffers(1, &mVB);
    glBindBuffer(GL_ARRAY_BUFFER, mVB);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vb_data), vb_data, GL_STATIC_DRAW);
    checkGlError();

    glEnableVertexAttribArray(POS_ATTRIB);
    glEnableVertexAttribArray(COLOR_ATTRIB);
    glVertexAttribPointer(POS_ATTRIB, 2, GL_FLOAT, GL_FALSE,
            6 * sizeof(float), 0);
    glVertexAttribPointer(COLOR_ATTRIB, 4, GL_FLOAT, GL_FALSE,
            6 * sizeof(float), (const void *)(2 * sizeof(float)));
    checkGlError();

    ALOGD("Using OpenGL ES 3.0 renderer");
    return true;
}

RendererES3::~RendererES3() {
    /* The destructor may be called after the context has already been
     * destroyed, in which case our objects have already been destroyed.
     *
     * If the context exists, it must be current. This only happens when we're
     * cleaning up after a failed init().
     */
    if (eglGetCurrentContext() != mEglContext)
        return;
    glDeleteVertexArrays(1, &mVA);
    glDeleteBuffers(1, &mVB);
    glDeleteProgram(mProgram);
}

void RendererES3::draw() {
    glUseProgram(mProgram);
    checkGlError();
    glBindVertexArray(mVA);
    checkGlError();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    checkGlError();
}
