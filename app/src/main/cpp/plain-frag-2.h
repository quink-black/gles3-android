static const char *PLAIN_FRAG_WITH_FLOAT_SAMPLER = R"(#version 300 es
precision mediump float;
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
}
)";
