#ifndef TONEMAP_OPENGL_HELPER_H
#define TONEMAP_OPENGL_HELPER_H

class OpenGL_Helper {
public:
    static bool CheckGLError(const char *file, const char *fun, int line);
    static bool CheckGLExtension(const char *api);
    static void PrintGLString(const char* name, int s);
    static void PrintGLExtension(void);
    static bool SetupDebugCallback(void);

    static unsigned int CreateShader(int shaderType, const char *src);
    static unsigned int CreateProgram(const char *vtxSrc, const char *fragSrc);
};
#define CheckGLError()  OpenGL_Helper::CheckGLError(__FILE__, __func__, __LINE__)

#endif
