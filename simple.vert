#version 430 core

uniform mat4 proj_mat;
uniform mat4 view_mat;
uniform mat4 world_mat;


layout (location = 0) in vec4 position;

void main(void)
{
    gl_Position = proj_mat * view_mat * world_mat * position;
}
