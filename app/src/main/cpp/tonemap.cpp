#include "tonemap.h"

#include <assert.h>
#include <GLES3/gl3.h>
#include <memory>

#include "image.h"
#include "log.h"
#include "opengl-helper.h"

static const char *VERTEX_SHADER =
R"(#version 300 es
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 uv;
out vec2 o_uv;
void main()
{
    gl_Position = vec4(position, 0.0, 1.0);
    o_uv = uv;
}
)";

static const char *FRAG_SHADER_A =
R"(#version 300 es
precision mediump float;
precision mediump usampler2D;
uniform usampler2D source;
uniform float gamma;
uniform float A;
uniform float B;
uniform float C;
uniform float D;
uniform float E;
uniform float F;
uniform float W;
in vec2 o_uv;
out vec4 out_color;

vec4 clampedValue(vec4 color)
{
    color.a = 1.0;
    return clamp(color, 0.0, 1.0);
}

vec4 gammaCorrect(vec4 color)
{
    return pow(color, vec4(1.0 / gamma));
}

vec4 tonemap(vec4 x)
{
    return ((x * (A*x + C*B) + D*E) / (x * (A*x+B) + D*F)) - E/F;
}

void main()
{
    uvec4 cc = texture(source, o_uv);
    float ratio = 255.0;
    vec4 color = vec4(float(cc.r)/ratio, float(cc.g)/ratio, float(cc.b)/ratio, 1.0);
    float exposureBias = 2.0;
    vec4 curr = tonemap(exposureBias * color);
    vec4 whiteScale = 1.0 / tonemap(vec4(W));
    color = curr * whiteScale;
    color = clampedValue(color);
    out_color = gammaCorrect(color);
}
)";

static const char *FRAG_SHADER_B =
R"(#version 300 es
precision mediump float;
uniform sampler2D source;
uniform float gamma;
uniform float A;
uniform float B;
uniform float C;
uniform float D;
uniform float E;
uniform float F;
uniform float W;
in vec2 o_uv;
out vec4 out_color;

vec4 clampedValue(vec4 color)
{
    color.a = 1.0;
    return clamp(color, 0.0, 1.0);
}

vec4 gammaCorrect(vec4 color)
{
    return pow(color, vec4(1.0 / gamma));
}

vec4 tonemap(vec4 x)
{
    return ((x * (A*x + C*B) + D*E) / (x * (A*x+B) + D*F)) - E/F;
}

void main()
{
    vec4 color = texture(source, o_uv);
    float exposureBias = 2.0;
    vec4 curr = tonemap(exposureBias * color);
    vec4 whiteScale = 1.0 / tonemap(vec4(W));
    color = curr * whiteScale;
    color = clampedValue(color);
    out_color = gammaCorrect(color);
}
)";

class ToneMapImpl : public ToneMap {
public:
    ToneMapImpl() :
        mProgram(0),
        mVAO(0),
        mVBO(0),
        mEBO(0),
        mTexture(0),
        mGamma(2.2f),
        mA(0.22f),
        mB(0.3f),
        mC(0.1f),
        mD(0.2f),
        mE(0.01f),
        mF(0.3f),
        mW(11.2f) {
    }

    int Init(const char *filename) override {
        int ret = 0;
        //ALOGD("%s", vertex_shader);
        //ALOGD("%s", frag_shader);
        bool hasFloatExt = OpenGL_Helper::CheckGLExtension("EXT_color_buffer_float");
        if (hasFloatExt)
            mProgram = OpenGL_Helper::CreateProgram(VERTEX_SHADER, FRAG_SHADER_B);
        else
            mProgram = OpenGL_Helper::CreateProgram(VERTEX_SHADER, FRAG_SHADER_A);
        if (!mProgram)
            return -1;

        glUseProgram(mProgram);

        glGenVertexArrays(1, &mVAO);
        glBindVertexArray(mVAO);
        CheckGLError();

        static float vbo[] = {
            -1.0f, 1.0f,    0.0f, 0.0f,
            -1.0f, -1.0f,   0.0f, 1.0f,
            1.0f, -1.0f,    1.0f, 1.0f,
            1.0f, 1.0f,     1.0f, 0.0f,
        };
        glGenBuffers(1, &mVBO);
        glBindBuffer(GL_ARRAY_BUFFER, mVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vbo), vbo, GL_STATIC_DRAW);
        CheckGLError();

        static GLushort ebo[] = {
            0, 1, 2,
            0, 2, 3,
        };
        glGenBuffers(1, &mEBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(ebo), ebo, GL_STATIC_DRAW);
        CheckGLError();

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const void *)(2 * sizeof(float)));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        CheckGLError();

        glGenTextures(1, &mTexture);
        glBindTexture(GL_TEXTURE_2D, mTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        CheckGLError();

        glUniform1i(glGetUniformLocation(mProgram, "source"), 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mTexture);

        std::unique_ptr<ImageDecoder> img(ImageDecoder::CreateImageDecoder("OpenEXR"));
        if (hasFloatExt)
            ret = img->Decode(filename, "float");
        else
            ret = img->Decode(filename, "uint16_t");

        if (ret) {
            return -1;
        }

        GLint internalformat;
        GLenum format, type;
        if (img->mDataType == "float") {
            internalformat = GL_RGB32F;
            format = GL_RGB;
            type = GL_FLOAT;
        } else if (img->mDataType == "uint16_t") {
            internalformat = GL_RGB16UI;
            format = GL_RGB_INTEGER;
            type = GL_UNSIGNED_SHORT;
        } else if (img->mDataType == "uint8_t") {
            internalformat = GL_RGB;
            format = GL_RGB;
            type = GL_UNSIGNED_BYTE;
        } else {
            assert(false);
            return -1;
        }

        glPixelStorei(GL_UNPACK_ROW_LENGTH, img->mWidth);
        glTexImage2D(GL_TEXTURE_2D, 0, internalformat, img->mWidth, img->mHeight, 0, format, type, img->mData);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        CheckGLError();

        return 0;
    }

    void Draw() override {
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        CheckGLError();

        glUseProgram(mProgram);
        setFloat("gamma", mGamma);
        setFloat("A", mA);
        setFloat("B", mB);
        setFloat("C", mC);
        setFloat("D", mD);
        setFloat("E", mE);
        setFloat("F", mF);
        setFloat("W", mW);

        glBindVertexArray(mVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
        CheckGLError();
    }

private:


    void setFloat(const char *name, float value)
    { 
        glUniform1f(glGetUniformLocation(mProgram, name), value); 
    }

    GLuint mProgram;
    GLuint mVAO, mVBO, mEBO;
    GLuint mTexture;

    float mGamma;
    float mA, mB, mC, mD, mE, mF, mW;
};

ToneMap *ToneMap::CreateToneMap() {
    return new ToneMapImpl();
}
