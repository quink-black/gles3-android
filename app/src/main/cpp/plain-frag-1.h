static const char *PLAIN_FRAG_WITH_INT_SAMPLER = R"(#version 300 es
precision mediump float;
precision mediump usampler2D;
uniform usampler2D source;
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
    uvec4 cc = texture(source, o_uv);
    float ratio = 255.0;
    vec4 color = vec4(float(cc.r)/ratio, float(cc.g)/ratio, float(cc.b)/ratio, 1.0);
    color = clampedValue(color);
    out_color = gammaCorrect(color);
}
)";
