#version 410 core

layout (location = 0) in vec3 aPos;

out vec3 vPos;

void main()
{
    vPos = aPos;
}
