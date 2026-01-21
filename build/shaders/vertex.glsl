#version 410 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;

void main()
{
    // Pozycja w ukladzie kamery (view space)
    vec4 viewPos = view * model * vec4(aPos, 1.0);
    FragPos = viewPos.xyz;

    // Poprawne przeksztalcenie wektora normalnego
    // normalMatrix = transpose(inverse(mat3(view * model)))
    Normal = normalize(normalMatrix * aNormal);

    TexCoord = aTexCoord;

    gl_Position = projection * viewPos;
}
