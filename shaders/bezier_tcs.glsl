#version 410 core

layout (vertices = 16) out;

in vec3 vPos[];
out vec3 tcPos[];

uniform int tessLevelOuter;
uniform int tessLevelInner;

void main()
{
    tcPos[gl_InvocationID] = vPos[gl_InvocationID];

    if (gl_InvocationID == 0) {
        gl_TessLevelOuter[0] = tessLevelOuter;
        gl_TessLevelOuter[1] = tessLevelOuter;
        gl_TessLevelOuter[2] = tessLevelOuter;
        gl_TessLevelOuter[3] = tessLevelOuter;
        gl_TessLevelInner[0] = tessLevelInner;
        gl_TessLevelInner[1] = tessLevelInner;
    }
}
