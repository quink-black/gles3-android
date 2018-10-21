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

#ifndef GLES3JNI_H
#define GLES3JNI_H 1

// ----------------------------------------------------------------------------
// Interface to the ES2 and ES3 renderers, used by JNI code.

class Renderer {
public:
    virtual ~Renderer() = default;
    void resize(int w, int h);
    void render();

protected:
    virtual void draw() = 0;
};

#endif // GLES3JNI_H
