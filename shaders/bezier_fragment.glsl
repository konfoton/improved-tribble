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
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
};

// Reflektor
struct SpotLight {
    vec3 position;
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
    float cutOff;
    float outerCutOff;
};

#define MAX_POINT_LIGHTS 4
#define MAX_SPOT_LIGHTS 4

uniform Material material;
uniform int numPointLights;
uniform int numSpotLights;
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform SpotLight spotLights[MAX_SPOT_LIGHTS];

uniform bool fogEnabled;
uniform float fogDensity;
uniform vec3 fogColor;
uniform float dayNightFactor;
uniform bool useBlinn;
uniform vec3 objectColor;

// Kolory flagi (Polska - bialo-czerwona)
uniform vec3 flagColor1;
uniform vec3 flagColor2;
uniform bool useFlagColors;

vec3 calcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diffuseColor)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);

    float spec = 0.0;
    if(useBlinn) {
        vec3 halfwayDir = normalize(lightDir + viewDir);
        spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess * 2.0);
    } else {
        vec3 reflectDir = reflect(-lightDir, normal);
        spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    }

    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance +
                               light.quadratic * distance * distance);

    vec3 ambient = light.ambient * material.ambient * diffuseColor;
    vec3 diffuse = light.diffuse * diff * diffuseColor;
    vec3 specular = light.specular * spec * material.specular;

    return (ambient + diffuse + specular) * attenuation;
}

vec3 calcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diffuseColor)
{
    vec3 lightDir = normalize(light.position - fragPos);

    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

    float diff = max(dot(normal, lightDir), 0.0);

    float spec = 0.0;
    if(useBlinn) {
        vec3 halfwayDir = normalize(lightDir + viewDir);
        spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess * 2.0);
    } else {
        vec3 reflectDir = reflect(-lightDir, normal);
        spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    }

    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance +
                               light.quadratic * distance * distance);

    vec3 ambient = light.ambient * material.ambient * diffuseColor;
    vec3 diffuse = light.diffuse * diff * diffuseColor;
    vec3 specular = light.specular * spec * material.specular;

    return (ambient + (diffuse + specular) * intensity) * attenuation;
}

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(-FragPos);

    // Kolor flagi - gorna/dolna polowa (polska flaga: bialy u gory, czerwony na dole)
    vec3 baseColor;
    if(useFlagColors) {
        if(TexCoord.y > 0.5) {
            baseColor = flagColor1; // Gorna polowa (bialy)
        } else {
            baseColor = flagColor2; // Dolna polowa (czerwony)
        }
    } else {
        baseColor = objectColor;
    }

    vec3 result = vec3(0.0);

    for(int i = 0; i < numPointLights && i < MAX_POINT_LIGHTS; i++) {
        result += calcPointLight(pointLights[i], norm, FragPos, viewDir, baseColor);
    }

    for(int i = 0; i < numSpotLights && i < MAX_SPOT_LIGHTS; i++) {
        result += calcSpotLight(spotLights[i], norm, FragPos, viewDir, baseColor);
    }

    // Dzien/Noc ambient
    vec3 dayAmbient = vec3(0.3);
    vec3 nightAmbient = vec3(0.05);
    vec3 ambientLight = mix(nightAmbient, dayAmbient, dayNightFactor);
    result += baseColor * ambientLight;

    // Mgla
    if(fogEnabled) {
        float dist = length(FragPos);
        float fogFactor = exp(-fogDensity * dist);
        fogFactor = clamp(fogFactor, 0.0, 1.0);
        vec3 currentFogColor = mix(vec3(0.1, 0.1, 0.15), fogColor, dayNightFactor);
        result = mix(currentFogColor, result, fogFactor);
    }

    FragColor = vec4(result, 1.0);
}
