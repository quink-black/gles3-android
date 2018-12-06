#include "render.h"
#include "config.h"
#if HAVE_GLES
#   include <GLES3/gl3.h>
#else
#   define GLFW_INCLUDE_GLCOREARB
#   define GL_GLEXT_PROTOTYPES
#   define GLFW_INCLUDE_GLEXT
#   include <GLFW/glfw3.h>
#endif

#include <assert.h>
#include <memory>
#include <string>

#include "log.h"
#include "opengl-helper.h"

#if HAVE_GLES
#define HEADER_VERSION  "#version 300 es\n"
#else
#define HEADER_VERSION  "#version 330 core\n"
#endif

namespace quink {

class Plain : public Render {
public:
    Plain();
    virtual ~Plain();
    int Init() override;
    int Init(const ImageCoord &coord) override;
    int UploadTexture(std::shared_ptr<Image<uint8_t>> img) override;
    int Draw() override;

    std::string GetVertexSrc();
    std::string GetFragSrc();

protected:
    GLuint mProgram;
    GLuint mVAO, mVBO, mEBO;
    GLuint mTexture;
    float mGamma = 2.2f;
};

Plain::Plain() :
    mProgram(0),
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
    glDeleteProgram(mProgram);
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
    std::string vertexSrc(HEADER_VERSION);
    vertexSrc +=
R"(layout(location = 0) in vec2 position;
layout(location = 1) in vec2 uv;
out vec2 o_uv;
void main()
{
    gl_Position = vec4(position, 0.0, 1.0);
    o_uv = uv;
}
)";
    return vertexSrc;
}

std::string Plain::GetFragSrc() {
    std::string src(HEADER_VERSION);
    src +=
R"(precision mediump float;
uniform sampler2D source;
uniform float gamma;
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

void main()
{
    vec4 color = texture(source, o_uv);
    color = clampedValue(color);
    out_color = gammaCorrect(color);
})";
    return src;
}

int Plain::Init(const ImageCoord &coord) {
    std::string vertexSrc = GetVertexSrc();
    std::string fragSrc = GetFragSrc();

    mProgram = OpenGL_Helper::CreateProgram(vertexSrc.c_str(),
            fragSrc.c_str());
    if (!mProgram)
        return -1;

    glUseProgram(mProgram);
    glUniform1f(glGetUniformLocation(mProgram, "gamma"), mGamma);
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    return 0;
}

int Plain::UploadTexture(std::shared_ptr<Image<uint8_t>> img) {
    CheckGLError();

    mGamma = 2.2 / img->mGamma;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, img->mWidth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img->mWidth, img->mHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, img->mData.get());
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    CheckGLError();

    return 0;
}

int Plain::Draw() {
    glUseProgram(mProgram);
    glUniform1f(glGetUniformLocation(mProgram, "gamma"), mGamma);

    glBindVertexArray(mVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
    CheckGLError();
    return 0;
}

Render *Render::Create() {
    return new Plain();
}

}
