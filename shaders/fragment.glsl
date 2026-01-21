#version 410 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

// Material
struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

// Swiatlo punktowe
struct PointLight {
    vec3 position;  // w ukladzie kamery
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
};

// Reflektor (spotlight)
struct SpotLight {
    vec3 position;      // w ukladzie kamery
    vec3 direction;     // w ukladzie kamery
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
    float cutOff;       // cos kata wewnetrznego
    float outerCutOff;  // cos kata zewnetrznego
};

#define MAX_POINT_LIGHTS 4
#define MAX_SPOT_LIGHTS 4

uniform Material material;
uniform int numPointLights;
uniform int numSpotLights;
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform SpotLight spotLights[MAX_SPOT_LIGHTS];

// Mgla
uniform bool fogEnabled;
uniform float fogDensity;
uniform vec3 fogColor;

// Dzien/Noc
uniform float dayNightFactor; // 0.0 = noc, 1.0 = dzien

// Phong vs Blinn
uniform bool useBlinn;

// Kolor obiektu
uniform vec3 objectColor;
uniform bool useTexture;
uniform sampler2D textureDiffuse;

vec3 calcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);

    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);

    // Specular (Phong lub Blinn)
    float spec = 0.0;
    if(useBlinn) {
        vec3 halfwayDir = normalize(lightDir + viewDir);
        spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess * 2.0);
    } else {
        vec3 reflectDir = reflect(-lightDir, normal);
        spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    }

    // Zanikanie
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance +
                               light.quadratic * distance * distance);

    vec3 ambient = light.ambient * material.ambient;
    vec3 diffuse = light.diffuse * diff * material.diffuse;
    vec3 specular = light.specular * spec * material.specular;

    return (ambient + diffuse + specular) * attenuation;
}

vec3 calcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);

    // Sprawdz czy fragment jest w stozku swiatla
    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);

    // Specular (Phong lub Blinn)
    float spec = 0.0;
    if(useBlinn) {
        vec3 halfwayDir = normalize(lightDir + viewDir);
        spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess * 2.0);
    } else {
        vec3 reflectDir = reflect(-lightDir, normal);
        spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    }

    // Zanikanie
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance +
                               light.quadratic * distance * distance);

    vec3 ambient = light.ambient * material.ambient;
    vec3 diffuse = light.diffuse * diff * material.diffuse;
    vec3 specular = light.specular * spec * material.specular;

    return (ambient + (diffuse + specular) * intensity) * attenuation;
}

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(-FragPos); // W ukladzie kamery, kamera jest w (0,0,0)

    vec3 baseColor;
    if(useTexture) {
        baseColor = texture(textureDiffuse, TexCoord).rgb;
    } else {
        baseColor = objectColor;
    }

    vec3 result = vec3(0.0);

    // Swiatla punktowe
    for(int i = 0; i < numPointLights && i < MAX_POINT_LIGHTS; i++) {
        result += calcPointLight(pointLights[i], norm, FragPos, viewDir);
    }

    // Reflektory
    for(int i = 0; i < numSpotLights && i < MAX_SPOT_LIGHTS; i++) {
        result += calcSpotLight(spotLights[i], norm, FragPos, viewDir);
    }

    // Zastosuj kolor obiektu
    result *= baseColor;

    // Dzien/Noc - modyfikuj ambient
    vec3 dayAmbient = vec3(0.3);
    vec3 nightAmbient = vec3(0.05);
    vec3 ambientLight = mix(nightAmbient, dayAmbient, dayNightFactor);
    result += baseColor * ambientLight;

    // Mgla (exponential fog)
    if(fogEnabled) {
        float dist = length(FragPos);
        float fogFactor = exp(-fogDensity * dist);
        fogFactor = clamp(fogFactor, 0.0, 1.0);

        // Kolor mgly zalezy od dnia/nocy
        vec3 currentFogColor = mix(vec3(0.1, 0.1, 0.15), fogColor, dayNightFactor);
        result = mix(currentFogColor, result, fogFactor);
    }

    FragColor = vec4(result, 1.0);
}
