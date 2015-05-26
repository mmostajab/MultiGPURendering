#version 430 core

uniform vec4 color;

layout(location = 0) out vec4 color0;

void main(void)
{
    color0 = color;
    gl_FragDepth = 0.1f;
}
