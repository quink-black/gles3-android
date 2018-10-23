#include "tonemap.h"
#include <GLES3/gl3.h>

#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

#include "log.h"

// returns true if a GL error occurred
static inline bool checkGlError(const char* funcName, int line = -1)
{
    GLint err = glGetError();
    if (err != GL_NO_ERROR) {
        ALOGE("GL error after (%s, %d): 0x%08x\n", funcName, line, err);
        return true;
    }
    return false;
}

#define checkGlError()  checkGlError(__func__, __LINE__)

static inline GLuint createShader(GLenum shaderType, const char* src)
{
    GLuint shader = glCreateShader(shaderType);
    if (!shader) {
        checkGlError();
        return 0;
    }
    glShaderSource(shader, 1, &src, NULL);

    GLint compiled = GL_FALSE;
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint infoLogLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLen);
        if (infoLogLen > 0) {
            GLchar* infoLog = new GLchar[infoLogLen];
            if (infoLog) {
                glGetShaderInfoLog(shader, infoLogLen, NULL, infoLog);
                ALOGE("Could not compile %s shader:\n%s\n",
                        shaderType == GL_VERTEX_SHADER ? "vertex" : "fragment",
                        infoLog);
                delete []infoLog;
            }
        }
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static inline GLuint createProgram(const char* vtxSrc, const char* fragSrc)
{
    GLuint vtxShader = 0;
    GLuint fragShader = 0;
    GLuint program = 0;
    GLint linked = GL_FALSE;

    vtxShader = createShader(GL_VERTEX_SHADER, vtxSrc);
    if (!vtxShader)
        goto exit;

    fragShader = createShader(GL_FRAGMENT_SHADER, fragSrc);
    if (!fragShader)
        goto exit;

    program = glCreateProgram();
    if (!program) {
        checkGlError();
        goto exit;
    }
    glAttachShader(program, vtxShader);
    glAttachShader(program, fragShader);

    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        ALOGE("Could not link program");
        GLint infoLogLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLen);
        if (infoLogLen) {
            GLchar* infoLog = new GLchar[infoLogLen];
            if (infoLog) {
                glGetProgramInfoLog(program, infoLogLen, NULL, infoLog);
                ALOGE("Could not link program:\n%s\n", infoLog);
                delete []infoLog;
            }
        }
        glDeleteProgram(program);
        program = 0;
    }

exit:
    glDeleteShader(vtxShader);
    glDeleteShader(fragShader);
    return program;
}

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

static const char *FRAG_SHADER =
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
    ToneMapImpl() : mProgram(0), mVAO(0), mVBO(0), mEBO(0), mTexture(0) { }

    int Init(const char *filename) override {
        //ALOGD("%s", vertex_shader);
        //ALOGD("%s", frag_shader);
        mProgram = createProgram(VERTEX_SHADER, FRAG_SHADER);
        glUseProgram(mProgram);

        glGenVertexArrays(1, &mVAO);
        glBindVertexArray(mVAO);
        checkGlError();

        static float vbo[] = {
            -0.9f, 0.9f,    0.0f, 0.0f,
            -0.9f, -0.9f,   0.0f, 1.0f,
            0.9f, -0.9f,    1.0f, 1.0f,
            0.9f, 0.9f,     1.0f, 0.0f,
        };
        glGenBuffers(1, &mVBO);
        glBindBuffer(GL_ARRAY_BUFFER, mVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vbo), vbo, GL_STATIC_DRAW);
        checkGlError();

        static GLushort ebo[] = {
            0, 1, 2,
            0, 2, 3,
        };
        glGenBuffers(1, &mEBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(ebo), ebo, GL_STATIC_DRAW);
        checkGlError();

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const void *)(2 * sizeof(float)));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        checkGlError();

        glGenTextures(1, &mTexture);
        glBindTexture(GL_TEXTURE_2D, mTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        checkGlError();

        EXRImage img;
        const char *err = nullptr;
        EXRVersion version;
        EXRHeader header;

        InitEXRImage(&img);
        InitEXRHeader(&header);
        ParseEXRVersionFromFile(&version, filename);
        if (ParseEXRHeaderFromFile(&header, &version, filename, &err) != 0) {
            ALOGE("could not parse exr header: %s", err);
            return -1;
        }
        for (int i = 0; i < header.num_channels; ++i) {
            if (header.requested_pixel_types[i] == TINYEXR_PIXELTYPE_HALF)
                header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
        }
        if (LoadEXRImageFromFile(&img, &header, filename, &err) != 0) {
            ALOGE("could not open exr file: %s", err);
            return -1;
        }

        ALOGD("image width %d, height %d", img.width, img.height);

        uint8_t *data = new uint8_t[img.width * img.height * 3];
        int idxR = -1, idxG = -1, idxB = -1;
        for (int c = 0; c < header.num_channels; ++c) {
            if (strcmp(header.channels[c].name, "R") == 0)
                idxR = c;
            else if (strcmp(header.channels[c].name, "G") == 0)
                idxG = c;
            else if (strcmp(header.channels[c].name, "B") == 0)
                idxB = c;
            else if (strcmp(header.channels[c].name, "A") == 0)
                ;
            else {
                ALOGE("channel name %s", header.channels[c].name);
                return -1;
            }
        }
        for (int i = 0; i < img.height; ++i) {
            for (int j = 0; j < img.width; ++j) {
                int index = i * img.width + j;
                data[index * 3 + 0] = convert(img.images[idxR], index, header.pixel_types[idxR]);
                data[index * 3 + 1] = convert(img.images[idxG], index, header.pixel_types[idxG]);
                data[index * 3 + 2] = convert(img.images[idxB], index, header.pixel_types[idxB]);
            }
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mTexture);

        glPixelStorei(GL_UNPACK_ROW_LENGTH, img.width);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img.width, img.height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        checkGlError();

        FreeEXRHeader(&header);
        FreeEXRImage(&img);
        delete []data;
        return 0;
    }

    void Draw() override {
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        checkGlError();

        glUseProgram(mProgram);
        setFloat("gamma", 2.2f);
        setFloat("A", 0.22f);
        setFloat("B", 0.3f);
        setFloat("C", 0.1f);
        setFloat("D", 0.2f);
        setFloat("E", 0.01f);
        setFloat("F", 0.3f);
        setFloat("W", 11.2f);

        glBindVertexArray(mVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
        checkGlError();
    }

private:
    static inline uint8_t convert(void *p, int offset, int type) {
        switch(type) {
        case TINYEXR_PIXELTYPE_UINT: {
            int32_t *pix = (int32_t *)p;
            return (float)pix[offset];
        }
        case TINYEXR_PIXELTYPE_HALF:
        case TINYEXR_PIXELTYPE_FLOAT: {
            float *pix = (float *)p;
            return pix[offset] * 255;
        }
        default:
            return 0.f;
        }
    }

    void setFloat(const char *name, float value)
    { 
        glUniform1f(glGetUniformLocation(mProgram, name), value); 
    }

    GLuint mProgram;
    GLuint mVAO, mVBO, mEBO;
    GLuint mTexture;
};

ToneMap *ToneMap::CreateToneMap() {
    return new ToneMapImpl();
}
