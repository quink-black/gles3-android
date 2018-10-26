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

static const char *FRAG_SHADER_INTEGER_SAMPLER =
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

static const char *FRAG_SHADER_FLOAT_SAMPLER =
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
        mProgramFloatSampler(0),
        mProgramIntSampler(0),
        mProgramCurrent(0),
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

    virtual ~ToneMapImpl() {
        glDeleteTextures(1, &mTexture);
        glDeleteBuffers(1, &mVBO);
        glDeleteBuffers(1, &mEBO);
        glDeleteVertexArrays(1, &mVAO);
        glDeleteProgram(mProgramFloatSampler);
        glDeleteProgram(mProgramIntSampler);
    }

    int Init() override {
        const ImageCoord coord = {
            {-1.0f, 1.0f},
            {-1.0f, -1.0f},
            {1.0f, -1.0f},
            {1.0f, 1.0f}
        };
        return Init(coord);
    }

    int Init(const ImageCoord &coord) override {
        mProgramFloatSampler = OpenGL_Helper::CreateProgram(VERTEX_SHADER, FRAG_SHADER_FLOAT_SAMPLER);
        mProgramIntSampler = OpenGL_Helper::CreateProgram(VERTEX_SHADER, FRAG_SHADER_INTEGER_SAMPLER);
        glGenVertexArrays(1, &mVAO);
        glBindVertexArray(mVAO);
        CheckGLError();

        float vbo[] = {
            coord.mTopLeft.mX, coord.mTopLeft.mY,           0.0f, 0.0f,
            coord.mBottomLeft.mX, coord.mBottomLeft.mY,     0.0f, 1.0f,
            coord.mBottomRight.mX, coord.mBottomRight.mY,   1.0f, 1.0f,
            coord.mTopRight.mX, coord.mTopRight.mY,         1.0f, 0.0f,
        };
        glGenBuffers(1, &mVBO);
        glBindBuffer(GL_ARRAY_BUFFER, mVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vbo), vbo, GL_STATIC_DRAW);
        CheckGLError();

        GLushort ebo[] = {
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
        return 0;
    }

    int UpLoadTexture(std::shared_ptr<ImageDecoder> img) override {
        if (img->mDataType == "uint16_t") {
            mProgramCurrent = mProgramIntSampler;
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        } else {
            mProgramCurrent = mProgramFloatSampler;
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }
        CheckGLError();

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

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mTexture);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, img->mWidth);
        glTexImage2D(GL_TEXTURE_2D, 0, internalformat, img->mWidth, img->mHeight, 0, format, type, img->mData);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        CheckGLError();

        return 0;
    }

    int Draw() override {
        glUseProgram(mProgramCurrent);
        glUniform1f(glGetUniformLocation(mProgramCurrent, "gamma"), mGamma);
        glUniform1f(glGetUniformLocation(mProgramCurrent, "A"), mA);
        glUniform1f(glGetUniformLocation(mProgramCurrent, "B"), mB);
        glUniform1f(glGetUniformLocation(mProgramCurrent, "C"), mC);
        glUniform1f(glGetUniformLocation(mProgramCurrent, "D"), mD);
        glUniform1f(glGetUniformLocation(mProgramCurrent, "E"), mE);
        glUniform1f(glGetUniformLocation(mProgramCurrent, "F"), mF);
        glUniform1f(glGetUniformLocation(mProgramCurrent, "W"), mW);

        glBindVertexArray(mVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mTexture);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
        CheckGLError();
        return 0;
    }

private:

    GLuint mProgramFloatSampler;
    GLuint mProgramIntSampler;
    GLuint mProgramCurrent;
    GLuint mVAO, mVBO, mEBO;
    GLuint mTexture;

    float mGamma;
    float mA, mB, mC, mD, mE, mF, mW;
};

ToneMap *ToneMap::CreateToneMap() {
    return new ToneMapImpl();
}
