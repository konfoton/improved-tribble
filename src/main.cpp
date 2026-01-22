#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <string>

// Ustawienia okna
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// Zmienne globalne sterowania
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Pozycja i kierunek poruszajacego sie obiektu
glm::vec3 movingObjectPos(0.0f, 0.5f, 0.0f);
float movingObjectAngle = 0.0f;
float movingObjectSpeed = 2.0f;

// Kierunek reflektora na ruchomym obiekcie (wzgledem obiektu)
float spotlightYaw = 0.0f;
float spotlightPitch = -10.0f;

// Aktywna kamera
int activeCamera = 0; // 0 - statyczna, 1 - sledzaca, 2 - FPP/TPP

// Efekty
bool fogEnabled = true;
float fogDensity = 0.05f;
float dayNightFactor = 1.0f; // 1.0 = dzien, 0.0 = noc
bool useBlinn = false;

// Sila wiatru dla flagi
float windStrength = 0.3f;

// Tessellation level
int tessLevel = 16;

// ============== SHADER CLASS ==============
class Shader {
public:
    unsigned int ID;

    Shader() : ID(0) {}

    bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath,
                       const std::string& tcsPath = "", const std::string& tesPath = "") {
        std::string vertexCode, fragmentCode, tcsCode, tesCode;

        // Wczytaj vertex shader
        std::ifstream vShaderFile(vertexPath);
        if (!vShaderFile.is_open()) {
            std::cerr << "Nie mozna otworzyc: " << vertexPath << std::endl;
            return false;
        }
        std::stringstream vShaderStream;
        vShaderStream << vShaderFile.rdbuf();
        vertexCode = vShaderStream.str();
        vShaderFile.close();

        // Wczytaj fragment shader
        std::ifstream fShaderFile(fragmentPath);
        if (!fShaderFile.is_open()) {
            std::cerr << "Nie mozna otworzyc: " << fragmentPath << std::endl;
            return false;
        }
        std::stringstream fShaderStream;
        fShaderStream << fShaderFile.rdbuf();
        fragmentCode = fShaderStream.str();
        fShaderFile.close();

        // Opcjonalne shadery tessellation
        bool hasTessellation = !tcsPath.empty() && !tesPath.empty();
        if (hasTessellation) {
            std::ifstream tcsFile(tcsPath);
            if (tcsFile.is_open()) {
                std::stringstream tcsStream;
                tcsStream << tcsFile.rdbuf();
                tcsCode = tcsStream.str();
                tcsFile.close();
            }

            std::ifstream tesFile(tesPath);
            if (tesFile.is_open()) {
                std::stringstream tesStream;
                tesStream << tesFile.rdbuf();
                tesCode = tesStream.str();
                tesFile.close();
            }
        }

        // Kompilacja
        unsigned int vertex, fragment, tcs = 0, tes = 0;
        int success;
        char infoLog[512];

        // Vertex
        vertex = glCreateShader(GL_VERTEX_SHADER);
        const char* vCode = vertexCode.c_str();
        glShaderSource(vertex, 1, &vCode, NULL);
        glCompileShader(vertex);
        glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(vertex, 512, NULL, infoLog);
            std::cerr << "Blad vertex shader:\n" << infoLog << std::endl;
            return false;
        }

        // Fragment
        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        const char* fCode = fragmentCode.c_str();
        glShaderSource(fragment, 1, &fCode, NULL);
        glCompileShader(fragment);
        glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(fragment, 512, NULL, infoLog);
            std::cerr << "Blad fragment shader:\n" << infoLog << std::endl;
            return false;
        }

        // Tessellation Control
        if (hasTessellation && !tcsCode.empty()) {
            tcs = glCreateShader(GL_TESS_CONTROL_SHADER);
            const char* tcsCodePtr = tcsCode.c_str();
            glShaderSource(tcs, 1, &tcsCodePtr, NULL);
            glCompileShader(tcs);
            glGetShaderiv(tcs, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(tcs, 512, NULL, infoLog);
                std::cerr << "Blad TCS shader:\n" << infoLog << std::endl;
                return false;
            }
        }

        // Tessellation Evaluation
        if (hasTessellation && !tesCode.empty()) {
            tes = glCreateShader(GL_TESS_EVALUATION_SHADER);
            const char* tesCodePtr = tesCode.c_str();
            glShaderSource(tes, 1, &tesCodePtr, NULL);
            glCompileShader(tes);
            glGetShaderiv(tes, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(tes, 512, NULL, infoLog);
                std::cerr << "Blad TES shader:\n" << infoLog << std::endl;
                return false;
            }
        }

        // Linkowanie
        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        if (hasTessellation) {
            if (tcs) glAttachShader(ID, tcs);
            if (tes) glAttachShader(ID, tes);
        }
        glLinkProgram(ID);
        glGetProgramiv(ID, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(ID, 512, NULL, infoLog);
            std::cerr << "Blad linkowania:\n" << infoLog << std::endl;
            return false;
        }

        glDeleteShader(vertex);
        glDeleteShader(fragment);
        if (tcs) glDeleteShader(tcs);
        if (tes) glDeleteShader(tes);

        return true;
    }

    void use() { glUseProgram(ID); }

    void setBool(const std::string& name, bool value) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
    }
    void setInt(const std::string& name, int value) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
    }
    void setFloat(const std::string& name, float value) const {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
    }
    void setVec2(const std::string& name, const glm::vec2& value) const {
        glUniform2fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(value));
    }
    void setVec3(const std::string& name, const glm::vec3& value) const {
        glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(value));
    }
    void setMat3(const std::string& name, const glm::mat3& mat) const {
        glUniformMatrix3fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
    }
    void setMat4(const std::string& name, const glm::mat4& mat) const {
        glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
    }
};

// ============== MESH DATA ==============
struct Mesh {
    unsigned int VAO, VBO, EBO;
    unsigned int indexCount;
};

// Generowanie kuli
Mesh createSphere(int sectors, int stacks) {
    Mesh mesh;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    float radius = 1.0f;

    for (int i = 0; i <= stacks; ++i) {
        float stackAngle = M_PI / 2 - i * M_PI / stacks;
        float xy = radius * cosf(stackAngle);
        float z = radius * sinf(stackAngle);

        for (int j = 0; j <= sectors; ++j) {
            float sectorAngle = j * 2 * M_PI / sectors;

            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);

            // Pozycja
            vertices.push_back(x);
            vertices.push_back(z);
            vertices.push_back(y);

            // Normalna (dla kuli = pozycja znormalizowana)
            float nx = x / radius;
            float ny = z / radius;
            float nz = y / radius;
            vertices.push_back(nx);
            vertices.push_back(ny);
            vertices.push_back(nz);

            // UV
            vertices.push_back((float)j / sectors);
            vertices.push_back((float)i / stacks);
        }
    }

    for (int i = 0; i < stacks; ++i) {
        int k1 = i * (sectors + 1);
        int k2 = k1 + sectors + 1;

        for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) {
                // Fixed winding order: CCW when viewed from outside
                indices.push_back(k1);
                indices.push_back(k1 + 1);
                indices.push_back(k2);
            }
            if (i != (stacks - 1)) {
                // Fixed winding order: CCW when viewed from outside
                indices.push_back(k1 + 1);
                indices.push_back(k2 + 1);
                indices.push_back(k2);
            }
        }
    }

    mesh.indexCount = indices.size();

    glGenVertexArrays(1, &mesh.VAO);
    glGenBuffers(1, &mesh.VBO);
    glGenBuffers(1, &mesh.EBO);

    glBindVertexArray(mesh.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Pozycja
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Normalna
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // UV
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    return mesh;
}

// Generowanie szescianu
Mesh createCube() {
    Mesh mesh;

    float vertices[] = {
        // Pozycja          Normalna           UV
        // Front
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,
        // Back
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
        // Left
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        // Right
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
        // Top
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
        // Bottom
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
    };

    unsigned int indices[] = {
        0, 1, 2, 2, 3, 0,       // Front
        4, 6, 5, 6, 4, 7,       // Back
        8, 9, 10, 10, 11, 8,    // Left
        12, 14, 13, 14, 12, 15, // Right
        16, 17, 18, 18, 19, 16, // Top
        20, 22, 21, 22, 20, 23  // Bottom
    };

    mesh.indexCount = 36;

    glGenVertexArrays(1, &mesh.VAO);
    glGenBuffers(1, &mesh.VBO);
    glGenBuffers(1, &mesh.EBO);

    glBindVertexArray(mesh.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    return mesh;
}

// Generowanie podlogi (plaski kwadrat)
Mesh createPlane(float size) {
    Mesh mesh;

    float halfSize = size / 2.0f;
    float vertices[] = {
        // Pozycja              Normalna          UV (0-1)
        -halfSize, 0.0f, -halfSize,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f,
         halfSize, 0.0f, -halfSize,  0.0f, 1.0f, 0.0f,  1.0f, 0.0f,
         halfSize, 0.0f,  halfSize,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f,
        -halfSize, 0.0f,  halfSize,  0.0f, 1.0f, 0.0f,  0.0f, 1.0f,
    };

    unsigned int indices[] = {
        0, 1, 2, 2, 3, 0
    };

    mesh.indexCount = 6;

    glGenVertexArrays(1, &mesh.VAO);
    glGenBuffers(1, &mesh.VBO);
    glGenBuffers(1, &mesh.EBO);

    glBindVertexArray(mesh.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    return mesh;
}

// Generowanie torusa
Mesh createTorus(float innerRadius, float outerRadius, int rings, int sides) {
    Mesh mesh;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    float ringRadius = (outerRadius - innerRadius) / 2.0f;
    float torusRadius = innerRadius + ringRadius;

    for (int i = 0; i <= rings; ++i) {
        float u = (float)i / rings * 2.0f * M_PI;
        float cu = cosf(u);
        float su = sinf(u);

        for (int j = 0; j <= sides; ++j) {
            float v = (float)j / sides * 2.0f * M_PI;
            float cv = cosf(v);
            float sv = sinf(v);

            float x = (torusRadius + ringRadius * cv) * cu;
            float y = ringRadius * sv;
            float z = (torusRadius + ringRadius * cv) * su;

            float nx = cv * cu;
            float ny = sv;
            float nz = cv * su;

            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            vertices.push_back(nx);
            vertices.push_back(ny);
            vertices.push_back(nz);
            vertices.push_back((float)i / rings);
            vertices.push_back((float)j / sides);
        }
    }

    for (int i = 0; i < rings; ++i) {
        for (int j = 0; j < sides; ++j) {
            int a = i * (sides + 1) + j;
            int b = a + sides + 1;

            // Winding: CCW when viewed from outside (matches outward normals)
            indices.push_back(a);
            indices.push_back(a + 1);
            indices.push_back(b);

            indices.push_back(a + 1);
            indices.push_back(b + 1);
            indices.push_back(b);
        }
    }

    mesh.indexCount = indices.size();

    glGenVertexArrays(1, &mesh.VAO);
    glGenBuffers(1, &mesh.VBO);
    glGenBuffers(1, &mesh.EBO);

    glBindVertexArray(mesh.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    return mesh;
}

// Generowanie platu Beziera (16 punktow kontrolnych)
Mesh createBezierPatch() {
    Mesh mesh;

    // 16 punktow kontrolnych dla platu bikubicznego Beziera (flaga)
    // Flaga jest pionowa, przymocowana przy maszcie (x=0)
    // u - kierunek poziomy (od masztu w prawo)
    // v - kierunek pionowy (od dolu do gory)
    float flagWidth = 1.5f;
    float flagHeight = 1.0f;
    float flagBottom = 2.3f;  // Wysokosc dolnej krawedzi flagi

    std::vector<float> controlPoints = {
        // Wiersz 0 (u=0 - przy maszcie)
        0.0f, flagBottom + 0.0f * flagHeight / 3.0f, 0.0f,
        0.0f, flagBottom + 1.0f * flagHeight / 3.0f, 0.0f,
        0.0f, flagBottom + 2.0f * flagHeight / 3.0f, 0.0f,
        0.0f, flagBottom + 3.0f * flagHeight / 3.0f, 0.0f,
        // Wiersz 1
        flagWidth / 3.0f, flagBottom + 0.0f * flagHeight / 3.0f, 0.0f,
        flagWidth / 3.0f, flagBottom + 1.0f * flagHeight / 3.0f, 0.0f,
        flagWidth / 3.0f, flagBottom + 2.0f * flagHeight / 3.0f, 0.0f,
        flagWidth / 3.0f, flagBottom + 3.0f * flagHeight / 3.0f, 0.0f,
        // Wiersz 2
        2.0f * flagWidth / 3.0f, flagBottom + 0.0f * flagHeight / 3.0f, 0.0f,
        2.0f * flagWidth / 3.0f, flagBottom + 1.0f * flagHeight / 3.0f, 0.0f,
        2.0f * flagWidth / 3.0f, flagBottom + 2.0f * flagHeight / 3.0f, 0.0f,
        2.0f * flagWidth / 3.0f, flagBottom + 3.0f * flagHeight / 3.0f, 0.0f,
        // Wiersz 3 (u=1 - swobodna krawedz)
        flagWidth, flagBottom + 0.0f * flagHeight / 3.0f, 0.0f,
        flagWidth, flagBottom + 1.0f * flagHeight / 3.0f, 0.0f,
        flagWidth, flagBottom + 2.0f * flagHeight / 3.0f, 0.0f,
        flagWidth, flagBottom + 3.0f * flagHeight / 3.0f, 0.0f,
    };

    glGenVertexArrays(1, &mesh.VAO);
    glGenBuffers(1, &mesh.VBO);

    glBindVertexArray(mesh.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, controlPoints.size() * sizeof(float), controlPoints.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    mesh.indexCount = 16; // 16 punktow kontrolnych

    return mesh;
}

// Generowanie masztu (cylinder)
Mesh createCylinder(float radius, float height, int segments) {
    Mesh mesh;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    // Dolna i gorna podstawa + boki
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * M_PI * i / segments;
        float x = radius * cosf(angle);
        float z = radius * sinf(angle);

        // Dolny wierzcholek
        vertices.push_back(x);
        vertices.push_back(0.0f);
        vertices.push_back(z);
        vertices.push_back(x / radius);
        vertices.push_back(0.0f);
        vertices.push_back(z / radius);
        vertices.push_back((float)i / segments);
        vertices.push_back(0.0f);

        // Gorny wierzcholek
        vertices.push_back(x);
        vertices.push_back(height);
        vertices.push_back(z);
        vertices.push_back(x / radius);
        vertices.push_back(0.0f);
        vertices.push_back(z / radius);
        vertices.push_back((float)i / segments);
        vertices.push_back(1.0f);
    }

    for (int i = 0; i < segments; ++i) {
        int base = i * 2;
        indices.push_back(base);
        indices.push_back(base + 2);
        indices.push_back(base + 1);

        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }

    mesh.indexCount = indices.size();

    glGenVertexArrays(1, &mesh.VAO);
    glGenBuffers(1, &mesh.VBO);
    glGenBuffers(1, &mesh.EBO);

    glBindVertexArray(mesh.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    return mesh;
}

// ============== USTAWIANIE UNIFORMOW SWIATLA ==============
void setLightUniforms(Shader& shader, const glm::mat4& view) {
    shader.setInt("numPointLights", 2);
    shader.setInt("numSpotLights", 2);

    // Material - nizszy shininess = wiekszy, bardziej rozproszony highlight
    shader.setVec3("material.ambient", glm::vec3(0.25f));
    shader.setVec3("material.diffuse", glm::vec3(0.9f));
    shader.setVec3("material.specular", glm::vec3(0.6f));  // Mniej intensywny specular
    shader.setFloat("material.shininess", 12.0f);          // Wiekszy highlight (bylo 32)

    // Swiatlo punktowe 1 - stale (lampa uliczna)
    glm::vec3 pointLight1Pos(3.0f, 4.0f, 3.0f);
    glm::vec3 pointLight1PosView = glm::vec3(view * glm::vec4(pointLight1Pos, 1.0f));
    shader.setVec3("pointLights[0].position", pointLight1PosView);
    shader.setVec3("pointLights[0].ambient", glm::vec3(0.1f) * dayNightFactor);
    shader.setVec3("pointLights[0].diffuse", glm::vec3(1.0f, 0.9f, 0.7f) * dayNightFactor);
    shader.setVec3("pointLights[0].specular", glm::vec3(1.0f) * dayNightFactor);
    shader.setFloat("pointLights[0].constant", 1.0f);
    shader.setFloat("pointLights[0].linear", 0.22f);      // Szybsze zanikanie
    shader.setFloat("pointLights[0].quadratic", 0.20f);   // Szybsze zanikanie

    // Swiatlo punktowe 2 - stale (druga lampa)
    glm::vec3 pointLight2Pos(-4.0f, 3.0f, -2.0f);
    glm::vec3 pointLight2PosView = glm::vec3(view * glm::vec4(pointLight2Pos, 1.0f));
    shader.setVec3("pointLights[1].position", pointLight2PosView);
    shader.setVec3("pointLights[1].ambient", glm::vec3(0.05f));
    shader.setVec3("pointLights[1].diffuse", glm::vec3(0.5f, 0.5f, 1.0f));
    shader.setVec3("pointLights[1].specular", glm::vec3(0.5f));
    shader.setFloat("pointLights[1].constant", 1.0f);
    shader.setFloat("pointLights[1].linear", 0.35f);      // Szybsze zanikanie
    shader.setFloat("pointLights[1].quadratic", 0.44f);   // Szybsze zanikanie

    // Reflektor 1 - na ruchomym obiekcie (reflektor samochodu)
    glm::vec3 spotLightPos = movingObjectPos + glm::vec3(0.0f, 0.3f, 0.0f);

    // Oblicz kierunek reflektora z uwzglednieniem kierunku obiektu i recznej regulacji
    float totalYaw = movingObjectAngle + spotlightYaw;
    glm::vec3 spotLightDir;
    spotLightDir.x = cos(glm::radians(spotlightPitch)) * sin(glm::radians(totalYaw));
    spotLightDir.y = sin(glm::radians(spotlightPitch));
    spotLightDir.z = cos(glm::radians(spotlightPitch)) * cos(glm::radians(totalYaw));
    spotLightDir = glm::normalize(spotLightDir);

    glm::vec3 spotLightPosView = glm::vec3(view * glm::vec4(spotLightPos, 1.0f));
    glm::vec3 spotLightDirView = glm::normalize(glm::vec3(view * glm::vec4(spotLightDir, 0.0f)));

    shader.setVec3("spotLights[0].position", spotLightPosView);
    shader.setVec3("spotLights[0].direction", spotLightDirView);
    shader.setVec3("spotLights[0].ambient", glm::vec3(0.05f));
    shader.setVec3("spotLights[0].diffuse", glm::vec3(2.5f, 2.5f, 2.0f));  // Mocniejszy reflektor
    shader.setVec3("spotLights[0].specular", glm::vec3(2.0f));              // Mocniejszy blask
    shader.setFloat("spotLights[0].constant", 1.0f);
    shader.setFloat("spotLights[0].linear", 0.14f);       // Umiarkowane zanikanie
    shader.setFloat("spotLights[0].quadratic", 0.07f);    // Umiarkowane zanikanie
    shader.setFloat("spotLights[0].cutOff", glm::cos(glm::radians(15.0f)));    // Szerszy stożek
    shader.setFloat("spotLights[0].outerCutOff", glm::cos(glm::radians(25.0f))); // Szerszy stożek

    // Reflektor 2 - staly (reflektor sceny)
    glm::vec3 spotLight2Pos(0.0f, 6.0f, 0.0f);
    glm::vec3 spotLight2Dir(0.0f, -1.0f, 0.0f);

    glm::vec3 spotLight2PosView = glm::vec3(view * glm::vec4(spotLight2Pos, 1.0f));
    glm::vec3 spotLight2DirView = glm::normalize(glm::vec3(view * glm::vec4(spotLight2Dir, 0.0f)));

    shader.setVec3("spotLights[1].position", spotLight2PosView);
    shader.setVec3("spotLights[1].direction", spotLight2DirView);
    shader.setVec3("spotLights[1].ambient", glm::vec3(0.0f));
    shader.setVec3("spotLights[1].diffuse", glm::vec3(0.8f) * dayNightFactor);
    shader.setVec3("spotLights[1].specular", glm::vec3(0.5f) * dayNightFactor);
    shader.setFloat("spotLights[1].constant", 1.0f);
    shader.setFloat("spotLights[1].linear", 0.045f);
    shader.setFloat("spotLights[1].quadratic", 0.0075f);
    shader.setFloat("spotLights[1].cutOff", glm::cos(glm::radians(25.0f)));
    shader.setFloat("spotLights[1].outerCutOff", glm::cos(glm::radians(35.0f)));

    // Efekty
    shader.setBool("fogEnabled", fogEnabled);
    shader.setFloat("fogDensity", fogDensity);
    shader.setVec3("fogColor", glm::vec3(0.5f, 0.6f, 0.7f));
    shader.setFloat("dayNightFactor", dayNightFactor);
    shader.setBool("useBlinn", useBlinn);
    shader.setBool("useTexture", false);
}

// ============== CALLBACK FUNKCJE ==============
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        switch (key) {
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, true);
                break;
            case GLFW_KEY_1:
                activeCamera = 0;
                std::cout << "Kamera: Statyczna" << std::endl;
                break;
            case GLFW_KEY_2:
                activeCamera = 1;
                std::cout << "Kamera: Sledzaca" << std::endl;
                break;
            case GLFW_KEY_3:
                activeCamera = 2;
                std::cout << "Kamera: TPP (trzecia osoba)" << std::endl;
                break;
            case GLFW_KEY_F:
                fogEnabled = !fogEnabled;
                std::cout << "Mgla: " << (fogEnabled ? "ON" : "OFF") << std::endl;
                break;
            case GLFW_KEY_B:
                useBlinn = !useBlinn;
                std::cout << "Model: " << (useBlinn ? "Blinn-Phong" : "Phong") << std::endl;
                break;
            case GLFW_KEY_N:
                dayNightFactor = (dayNightFactor > 0.5f) ? 0.0f : 1.0f;
                std::cout << "Pora dnia: " << (dayNightFactor > 0.5f ? "Dzien" : "Noc") << std::endl;
                break;
            case GLFW_KEY_KP_ADD:
            case GLFW_KEY_EQUAL:
                fogDensity = std::min(fogDensity + 0.01f, 0.5f);
                std::cout << "Gestosc mgly: " << fogDensity << std::endl;
                break;
            case GLFW_KEY_KP_SUBTRACT:
            case GLFW_KEY_MINUS:
                fogDensity = std::max(fogDensity - 0.01f, 0.0f);
                std::cout << "Gestosc mgly: " << fogDensity << std::endl;
                break;
            case GLFW_KEY_T:
                tessLevel = std::min(tessLevel + 2, 64);
                std::cout << "Tessellation level: " << tessLevel << std::endl;
                break;
            case GLFW_KEY_G:
                tessLevel = std::max(tessLevel - 2, 2);
                std::cout << "Tessellation level: " << tessLevel << std::endl;
                break;
            case GLFW_KEY_Y:
                windStrength = std::min(windStrength + 0.1f, 1.0f);
                std::cout << "Sila wiatru: " << windStrength << std::endl;
                break;
            case GLFW_KEY_H:
                windStrength = std::max(windStrength - 0.1f, 0.0f);
                std::cout << "Sila wiatru: " << windStrength << std::endl;
                break;
        }
    }
}

void processInput(GLFWwindow* window) {
    // Sterowanie ruchomym obiektem
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        movingObjectPos.x += sin(glm::radians(movingObjectAngle)) * movingObjectSpeed * deltaTime;
        movingObjectPos.z += cos(glm::radians(movingObjectAngle)) * movingObjectSpeed * deltaTime;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        movingObjectPos.x -= sin(glm::radians(movingObjectAngle)) * movingObjectSpeed * deltaTime;
        movingObjectPos.z -= cos(glm::radians(movingObjectAngle)) * movingObjectSpeed * deltaTime;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        movingObjectAngle += 90.0f * deltaTime;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        movingObjectAngle -= 90.0f * deltaTime;
    }

    // Sterowanie kierunkiem reflektora
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        spotlightYaw += 45.0f * deltaTime;
    }
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        spotlightYaw -= 45.0f * deltaTime;
    }
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        spotlightPitch = std::min(spotlightPitch + 30.0f * deltaTime, 45.0f);
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        spotlightPitch = std::max(spotlightPitch - 30.0f * deltaTime, -45.0f);
    }

    // Plynna zmiana dnia/nocy
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        dayNightFactor = std::min(dayNightFactor + 0.5f * deltaTime, 1.0f);
    }
    if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) {
        dayNightFactor = std::max(dayNightFactor - 0.5f * deltaTime, 0.0f);
    }
}

// ============== MAIN ==============
int main() {
    // Inicjalizacja GLFW
    if (!glfwInit()) {
        std::cerr << "Nie mozna zainicjalizowac GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Grafika Komputerowa - Projekt", NULL, NULL);
    if (!window) {
        std::cerr << "Nie mozna utworzyc okna GLFW" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);

    // Inicjalizacja GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Nie mozna zainicjalizowac GLEW" << std::endl;
        return -1;
    }

    // Konfiguracja OpenGL
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // Utworz domyslna biala teksture 1x1 (zapobiega bledowi samplera)
    unsigned int defaultTexture;
    glGenTextures(1, &defaultTexture);
    glBindTexture(GL_TEXTURE_2D, defaultTexture);
    unsigned char whitePixel[] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Wczytaj shadery
    Shader mainShader;
    if (!mainShader.loadFromFiles("shaders/vertex.glsl", "shaders/fragment.glsl")) {
        std::cerr << "Blad wczytywania shader'ow" << std::endl;
        return -1;
    }

    Shader bezierShader;
    if (!bezierShader.loadFromFiles("shaders/bezier_vertex.glsl", "shaders/bezier_fragment.glsl",
                                     "shaders/bezier_tcs.glsl", "shaders/bezier_tes.glsl")) {
        std::cerr << "Blad wczytywania shader'ow Beziera" << std::endl;
        return -1;
    }

    // Utworz geometrie
    Mesh sphere = createSphere(32, 16);
    Mesh cube = createCube();
    Mesh plane = createPlane(20.0f);
    Mesh torus = createTorus(0.3f, 0.8f, 32, 16);
    Mesh bezierPatch = createBezierPatch();
    Mesh cylinder = createCylinder(0.05f, 3.5f, 16);

    // Macierz projekcji
    glm::mat4 projection = glm::perspective(glm::radians(45.0f),
                                            (float)SCR_WIDTH / (float)SCR_HEIGHT,
                                            0.1f, 100.0f);

    std::cout << "\n=== STEROWANIE ===" << std::endl;
    std::cout << "WASD - ruch obiektu" << std::endl;
    std::cout << "Strzalki - kierunek reflektora" << std::endl;
    std::cout << "1/2/3 - przelaczanie kamer" << std::endl;
    std::cout << "F - wlacz/wylacz mgle" << std::endl;
    std::cout << "B - przelacz Phong/Blinn" << std::endl;
    std::cout << "N - dzien/noc (szybkie)" << std::endl;
    std::cout << "O/P - plynna zmiana dnia/nocy" << std::endl;
    std::cout << "+/- - gestosc mgly" << std::endl;
    std::cout << "T/G - poziom tessellation" << std::endl;
    std::cout << "Y/H - sila wiatru" << std::endl;
    std::cout << "ESC - wyjscie" << std::endl;
    std::cout << "==================\n" << std::endl;

    // Glowna petla renderowania
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // Czyszczenie
        glm::vec3 clearColor = glm::mix(glm::vec3(0.02f, 0.02f, 0.05f),
                                        glm::vec3(0.4f, 0.6f, 0.8f),
                                        dayNightFactor);
        glClearColor(clearColor.r, clearColor.g, clearColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Wybor kamery
        glm::mat4 view;
        glm::vec3 cameraPos;

        switch (activeCamera) {
            case 0: // Kamera statyczna
                cameraPos = glm::vec3(8.0f, 6.0f, 8.0f);
                view = glm::lookAt(cameraPos,
                                   glm::vec3(0.0f, 0.0f, 0.0f),
                                   glm::vec3(0.0f, 1.0f, 0.0f));
                break;
            case 1: // Kamera sledzaca
                cameraPos = glm::vec3(8.0f, 6.0f, 8.0f);
                view = glm::lookAt(cameraPos,
                                   movingObjectPos,
                                   glm::vec3(0.0f, 1.0f, 0.0f));
                break;
            case 2: // Kamera TPP
                {
                    float camDistance = 4.0f;
                    float camHeight = 2.0f;
                    cameraPos = movingObjectPos - glm::vec3(
                        sin(glm::radians(movingObjectAngle)) * camDistance,
                        -camHeight,
                        cos(glm::radians(movingObjectAngle)) * camDistance
                    );
                    view = glm::lookAt(cameraPos,
                                       movingObjectPos + glm::vec3(0.0f, 0.5f, 0.0f),
                                       glm::vec3(0.0f, 1.0f, 0.0f));
                }
                break;
        }

        // ====== RENDEROWANIE GLOWNYM SHADEREM ======
        mainShader.use();
        mainShader.setMat4("projection", projection);
        mainShader.setMat4("view", view);
        setLightUniforms(mainShader, view);

        // Aktywuj domyslna teksture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, defaultTexture);
        mainShader.setInt("textureDiffuse", 0);
        mainShader.setBool("useTexture", false);

        // Podloga z wzorem szachownicy
        {
            glm::mat4 model = glm::mat4(1.0f);
            glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(view * model)));
            mainShader.setMat4("model", model);
            mainShader.setMat3("normalMatrix", normalMatrix);
            mainShader.setBool("useCheckerboard", true);
            mainShader.setFloat("checkerScale", 10.0f); // 10x10 kratek
            mainShader.setVec3("checkerColor1", glm::vec3(0.5f, 0.5f, 0.5f)); // Szary jasny
            mainShader.setVec3("checkerColor2", glm::vec3(0.25f, 0.25f, 0.25f)); // Szary ciemny
            glDisable(GL_CULL_FACE); // Podloga widoczna z obu stron
            glBindVertexArray(plane.VAO);
            glDrawElements(GL_TRIANGLES, plane.indexCount, GL_UNSIGNED_INT, 0);
            glEnable(GL_CULL_FACE);
            mainShader.setBool("useCheckerboard", false); // Wylacz dla innych obiektow
        }

        // Ruchomy obiekt (samochod/szescian)
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, movingObjectPos);
            model = glm::rotate(model, glm::radians(movingObjectAngle), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::scale(model, glm::vec3(0.8f, 0.5f, 1.2f));
            glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(view * model)));
            mainShader.setMat4("model", model);
            mainShader.setMat3("normalMatrix", normalMatrix);
            mainShader.setVec3("objectColor", glm::vec3(0.8f, 0.2f, 0.2f));
            glBindVertexArray(cube.VAO);
            glDrawElements(GL_TRIANGLES, cube.indexCount, GL_UNSIGNED_INT, 0);
        }

        // Kula (obiekt gladki)
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(-3.0f, 1.0f, 2.0f));
            glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(view * model)));
            mainShader.setMat4("model", model);
            mainShader.setMat3("normalMatrix", normalMatrix);
            mainShader.setVec3("objectColor", glm::vec3(0.2f, 0.4f, 0.8f));
            glBindVertexArray(sphere.VAO);
            glDrawElements(GL_TRIANGLES, sphere.indexCount, GL_UNSIGNED_INT, 0);
        }

        // Torus
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(3.0f, 0.5f, -3.0f));
            model = glm::rotate(model, (float)glfwGetTime() * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(view * model)));
            mainShader.setMat4("model", model);
            mainShader.setMat3("normalMatrix", normalMatrix);
            mainShader.setVec3("objectColor", glm::vec3(0.8f, 0.6f, 0.2f));
            glBindVertexArray(torus.VAO);
            glDrawElements(GL_TRIANGLES, torus.indexCount, GL_UNSIGNED_INT, 0);
        }

        // Szescian statyczny 1
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(-4.0f, 0.5f, -4.0f));
            glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(view * model)));
            mainShader.setMat4("model", model);
            mainShader.setMat3("normalMatrix", normalMatrix);
            mainShader.setVec3("objectColor", glm::vec3(0.5f, 0.5f, 0.5f));
            glBindVertexArray(cube.VAO);
            glDrawElements(GL_TRIANGLES, cube.indexCount, GL_UNSIGNED_INT, 0);
        }

        // Szescian statyczny 2
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(4.0f, 0.75f, 2.0f));
            model = glm::scale(model, glm::vec3(1.5f));
            glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(view * model)));
            mainShader.setMat4("model", model);
            mainShader.setMat3("normalMatrix", normalMatrix);
            mainShader.setVec3("objectColor", glm::vec3(0.6f, 0.3f, 0.6f));
            glBindVertexArray(cube.VAO);
            glDrawElements(GL_TRIANGLES, cube.indexCount, GL_UNSIGNED_INT, 0);
        }

        // Maszt na flage
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(0.0f, 0.0f, -5.0f));
            glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(view * model)));
            mainShader.setMat4("model", model);
            mainShader.setMat3("normalMatrix", normalMatrix);
            mainShader.setVec3("objectColor", glm::vec3(0.4f, 0.3f, 0.2f));
            glBindVertexArray(cylinder.VAO);
            glDrawElements(GL_TRIANGLES, cylinder.indexCount, GL_UNSIGNED_INT, 0);
        }

        // ====== RENDEROWANIE FLAGI (BEZIER) ======
        glDisable(GL_CULL_FACE); // Flaga jest widoczna z obu stron

        bezierShader.use();
        bezierShader.setMat4("projection", projection);
        bezierShader.setMat4("view", view);
        setLightUniforms(bezierShader, view);

        // Ustawienia flagi
        bezierShader.setFloat("time", (float)glfwGetTime());
        bezierShader.setFloat("windStrength", windStrength);
        bezierShader.setVec2("windDirection", glm::vec2(1.0f, 0.3f));
        bezierShader.setInt("tessLevelOuter", tessLevel);
        bezierShader.setInt("tessLevelInner", tessLevel);
        bezierShader.setBool("useFlagColors", true);
        bezierShader.setVec3("flagColor1", glm::vec3(1.0f, 1.0f, 1.0f)); // Bialy
        bezierShader.setVec3("flagColor2", glm::vec3(0.9f, 0.1f, 0.2f)); // Czerwony

        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(0.0f, 0.0f, -5.0f));
            glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(view * model)));
            bezierShader.setMat4("model", model);
            bezierShader.setMat3("normalMatrix", normalMatrix);

            glBindVertexArray(bezierPatch.VAO);
            glPatchParameteri(GL_PATCH_VERTICES, 16);
            glDrawArrays(GL_PATCHES, 0, 16);
        }

        glEnable(GL_CULL_FACE); // Przywroc culling

        // Swap buffers
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &sphere.VAO);
    glDeleteBuffers(1, &sphere.VBO);
    glDeleteBuffers(1, &sphere.EBO);

    glDeleteVertexArrays(1, &cube.VAO);
    glDeleteBuffers(1, &cube.VBO);
    glDeleteBuffers(1, &cube.EBO);

    glDeleteVertexArrays(1, &plane.VAO);
    glDeleteBuffers(1, &plane.VBO);
    glDeleteBuffers(1, &plane.EBO);

    glDeleteVertexArrays(1, &torus.VAO);
    glDeleteBuffers(1, &torus.VBO);
    glDeleteBuffers(1, &torus.EBO);

    glDeleteVertexArrays(1, &bezierPatch.VAO);
    glDeleteBuffers(1, &bezierPatch.VBO);

    glDeleteVertexArrays(1, &cylinder.VAO);
    glDeleteBuffers(1, &cylinder.VBO);
    glDeleteBuffers(1, &cylinder.EBO);

    glfwTerminate();
    return 0;
}
