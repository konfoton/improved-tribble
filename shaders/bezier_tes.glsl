#version 410 core

layout (quads, equal_spacing, ccw) in;

in vec3 tcPos[];

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;

uniform float time;
uniform float windStrength;
uniform vec2 windDirection;

// Funkcja Bernsteina
float bernstein(int i, float t)
{
    float mt = 1.0 - t;
    if (i == 0) return mt * mt * mt;
    if (i == 1) return 3.0 * mt * mt * t;
    if (i == 2) return 3.0 * mt * t * t;
    return t * t * t;
}

// Pochodna funkcji Bernsteina
float bernsteinDerivative(int i, float t)
{
    float mt = 1.0 - t;
    if (i == 0) return -3.0 * mt * mt;
    if (i == 1) return 3.0 * mt * mt - 6.0 * mt * t;
    if (i == 2) return 6.0 * mt * t - 3.0 * t * t;
    return 3.0 * t * t;
}

vec3 evaluateBezier(float u, float v)
{
    vec3 pos = vec3(0.0);
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            float bu = bernstein(i, u);
            float bv = bernstein(j, v);
            pos += tcPos[i * 4 + j] * bu * bv;
        }
    }
    return pos;
}

vec3 evaluateBezierDu(float u, float v)
{
    vec3 du = vec3(0.0);
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            float dbu = bernsteinDerivative(i, u);
            float bv = bernstein(j, v);
            du += tcPos[i * 4 + j] * dbu * bv;
        }
    }
    return du;
}

vec3 evaluateBezierDv(float u, float v)
{
    vec3 dv = vec3(0.0);
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            float bu = bernstein(i, u);
            float dbv = bernsteinDerivative(j, v);
            dv += tcPos[i * 4 + j] * bu * dbv;
        }
    }
    return dv;
}

void main()
{
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    // Oblicz pozycje na powierzchni Beziera
    vec3 pos = evaluateBezier(u, v);

    // Animacja wiatru - sinusoidalna deformacja
    // Im dalej od krawedzi przymocowania (u=0), tym wieksza deformacja
    float windEffect = u * u * windStrength;
    float wave1 = sin(time * 2.0 + u * 4.0 + v * 2.0) * windEffect;
    float wave2 = sin(time * 3.5 + u * 3.0 - v * 1.5) * windEffect * 0.5;
    float wave3 = sin(time * 1.5 + u * 2.0 + v * 3.0) * windEffect * 0.3;

    pos.x += (wave1 + wave2) * windDirection.x;
    pos.z += (wave1 + wave2) * windDirection.y;
    pos.y += wave3 * 0.5;

    // Oblicz normalna przez pochodne czesciowe
    vec3 du = evaluateBezierDu(u, v);
    vec3 dv = evaluateBezierDv(u, v);

    // Zmodyfikuj pochodne dla animacji
    du.x += cos(time * 2.0 + u * 4.0 + v * 2.0) * 4.0 * 2.0 * u * windStrength * windDirection.x;
    du.z += cos(time * 2.0 + u * 4.0 + v * 2.0) * 4.0 * 2.0 * u * windStrength * windDirection.y;

    vec3 normal = normalize(cross(du, dv));

    // Transformacja do ukladu kamery
    vec4 viewPos = view * model * vec4(pos, 1.0);
    FragPos = viewPos.xyz;
    Normal = normalize(normalMatrix * normal);

    TexCoord = vec2(u, v);

    gl_Position = projection * viewPos;
}
