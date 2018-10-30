#include "tonemap.h"

#include <assert.h>
#include <GLES3/gl3.h>
#include <memory>

#include "image.h"
#include "log.h"
#include "opengl-helper.h"
#include "hable-vertex.h"
#include "hable-frag-1.h"
#include "hable-frag-2.h"
#include "plain-vertex.h"
#include "plain-frag-1.h"
#include "plain-frag-2.h"

class Plain : public ToneMap {
public:
    Plain();
    virtual ~Plain();
    int Init() override;
    int Init(const ImageCoord &coord) override;
    int UploadTexture(std::shared_ptr<ImageDecoder> img) override;
    int Draw() override;

    virtual std::string GetVertexSrc();
    virtual std::string GetFragWithFloatSamplerSrc();
    virtual std::string GetFragWithIntSampleSrc();

protected:
    GLuint mProgramFloatSampler;
    GLuint mProgramIntSampler;
    GLuint mProgramCurrent;
    GLuint mVAO, mVBO, mEBO;
    GLuint mTexture;
    float mGamma = 2.2f;
};

Plain::Plain() :
    mProgramFloatSampler(0),
    mProgramIntSampler(0),
    mProgramCurrent(0),
    mVAO(0),
    mVBO(0),
    mEBO(0),
    mTexture(0) {
}

Plain::~Plain() {
    glDeleteTextures(1, &mTexture);
    glDeleteBuffers(1, &mVBO);
    glDeleteBuffers(1, &mEBO);
    glDeleteVertexArrays(1, &mVAO);
    glDeleteProgram(mProgramFloatSampler);
    glDeleteProgram(mProgramIntSampler);
}

int Plain::Init(){
    const ImageCoord coord = {
        {-1.0f, 1.0f},
        {-1.0f, -1.0f},
        {1.0f, -1.0f},
        {1.0f, 1.0f}
    };
    return Init(coord);
}

std::string Plain::GetVertexSrc() {
    return PLAIN_VERTEX;
}

std::string Plain::GetFragWithFloatSamplerSrc() {
    return PLAIN_FRAG_WITH_FLOAT_SAMPLER;
}

std::string Plain::GetFragWithIntSampleSrc() {
    return PLAIN_FRAG_WITH_INT_SAMPLER;
}

int Plain::Init(const ImageCoord &coord) {
    std::string vertexSrc = GetVertexSrc();
    std::string fragFloatSrc = GetFragWithFloatSamplerSrc();
    std::string fragIntSrc = GetFragWithIntSampleSrc();

    mProgramFloatSampler = OpenGL_Helper::CreateProgram(vertexSrc.c_str(),
            fragFloatSrc.c_str());
    mProgramIntSampler = OpenGL_Helper::CreateProgram(vertexSrc.c_str(),
            fragIntSrc.c_str());
    if (!mProgramFloatSampler || !mProgramIntSampler) {
        return -1;
    }
    glUseProgram(mProgramFloatSampler);
    glUniform1f(glGetUniformLocation(mProgramFloatSampler, "gamma"), mGamma);
    glUseProgram(mProgramIntSampler);
    glUniform1f(glGetUniformLocation(mProgramIntSampler, "gamma"), mGamma);
    CheckGLError();

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
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
            (const void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    CheckGLError();

    glGenTextures(1, &mTexture);
    glBindTexture(GL_TEXTURE_2D, mTexture);
    return 0;
}

int Plain::UploadTexture(std::shared_ptr<ImageDecoder> img) {
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

int Plain::Draw() {
    glUseProgram(mProgramCurrent);
    glBindVertexArray(mVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
    CheckGLError();
    return 0;
}

class Hable : public Plain {
public:
    Hable();
    virtual ~Hable();
    int Init(const ImageCoord &coord) override;

    std::string GetVertexSrc() override;
    std::string GetFragWithFloatSamplerSrc() override;
    std::string GetFragWithIntSampleSrc() override;

private:
    /* F(x) = ((x*(A*x + C*B) + D*E) / (x*(A*x + B) + D*F)) - E/F;
     * FinalColor = F(Linearcolor) / F(LinearWhite)
     */
    float mA = 0.15f;   // Shoulder Strength
    float mB = 0.50f;   // Linear Strength
    float mC = 0.10f;   // Linear Angle
    float mD = 0.20f;   // Toe Strength
    float mE = 0.02f;   // Toe Numerator
    float mF = 0.30f;   // Tone Denominator E/F = Toe Angle
    float mW = 11.2f;   // Linear White Point Value
};

Hable::Hable() { }

Hable::~Hable() { }

std::string Hable::GetVertexSrc() {
    return HABLE_VERTEX;
}

std::string Hable::GetFragWithFloatSamplerSrc() {
    return HABLE_FRAG_WITH_FLOAT_SAMPLER;
}

std::string Hable::GetFragWithIntSampleSrc() {
    return HABLE_FRAG_WITH_INT_SAMPLER;
}

int Hable::Init(const ImageCoord &coord) {
    int ret = Plain::Init(coord);
    if (ret)
        return ret;

    mProgramCurrent = mProgramFloatSampler;
    glUseProgram(mProgramCurrent);
    glUniform1f(glGetUniformLocation(mProgramCurrent, "A"), mA);
    glUniform1f(glGetUniformLocation(mProgramCurrent, "B"), mB);
    glUniform1f(glGetUniformLocation(mProgramCurrent, "C"), mC);
    glUniform1f(glGetUniformLocation(mProgramCurrent, "D"), mD);
    glUniform1f(glGetUniformLocation(mProgramCurrent, "E"), mE);
    glUniform1f(glGetUniformLocation(mProgramCurrent, "F"), mF);
    glUniform1f(glGetUniformLocation(mProgramCurrent, "W"), mW);

    mProgramCurrent = mProgramIntSampler;
    glUseProgram(mProgramCurrent);
    glUniform1f(glGetUniformLocation(mProgramCurrent, "A"), mA);
    glUniform1f(glGetUniformLocation(mProgramCurrent, "B"), mB);
    glUniform1f(glGetUniformLocation(mProgramCurrent, "C"), mC);
    glUniform1f(glGetUniformLocation(mProgramCurrent, "D"), mD);
    glUniform1f(glGetUniformLocation(mProgramCurrent, "E"), mE);
    glUniform1f(glGetUniformLocation(mProgramCurrent, "F"), mF);
    glUniform1f(glGetUniformLocation(mProgramCurrent, "W"), mW);

    return 0;
}

ToneMap *ToneMap::CreateToneMap(const std::string &algorithm) {
    if (algorithm == "Hable")
        return new Hable();
    else
        return new Plain();
}
