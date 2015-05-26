#version 430 core

layout(binding = 0) uniform sampler2D color_texture;
layout(binding = 1) uniform sampler2D depth_texture;

layout(location = 0) out vec4 out_color;

void main(void)
{
    vec2 P = gl_FragCoord.xy / textureSize(color_texture, 0);

    out_color = texture(color_texture, P);
    //out_color = vec4(P, 0.0f, 1.0f);
    gl_FragDepth =  texture(depth_texture, P);
}
