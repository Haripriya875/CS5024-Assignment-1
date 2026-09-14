
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

std::string loadShaderSource(const char* filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        std::cerr << "Failed to open shader file: "
                  << filepath << std::endl;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main()
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(
        800, 600,
        "CS5024 Assignment 1 - 3D Cubic Grid",
        nullptr, nullptr
    );

    if (!window)
    {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();

    if (err != GLEW_OK && err != GLEW_ERROR_NO_GLX_DISPLAY)
    {
        std::cerr << "GLEW Error: "
                  << glewGetErrorString(err) << std::endl; 
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glViewport(0, 0, 800, 600);
    glEnable(GL_DEPTH_TEST);
    glLineWidth(1.5f);

    float cubeVertices[] = {
        // Front
        -0.5f,-0.5f, 0.5f, 1.0f,0.70f,0.75f,
         0.5f,-0.5f, 0.5f, 1.0f,0.70f,0.75f,
         0.5f, 0.5f, 0.5f, 1.0f,0.70f,0.75f,
        -0.5f,-0.5f, 0.5f, 1.0f,0.70f,0.75f,
         0.5f, 0.5f, 0.5f, 1.0f,0.70f,0.75f,
        -0.5f, 0.5f, 0.5f, 1.0f,0.70f,0.75f,

        // Back
        -0.5f,-0.5f,-0.5f, 0.70f,0.90f,0.75f,
         0.5f,-0.5f,-0.5f, 0.70f,0.90f,0.75f,
         0.5f, 0.5f,-0.5f, 0.70f,0.90f,0.75f,
        -0.5f,-0.5f,-0.5f, 0.70f,0.90f,0.75f,
         0.5f, 0.5f,-0.5f, 0.70f,0.90f,0.75f,
        -0.5f, 0.5f,-0.5f, 0.70f,0.90f,0.75f,

        // Left
        -0.5f,-0.5f,-0.5f, 0.70f,0.80f,1.0f,
        -0.5f,-0.5f, 0.5f, 0.70f,0.80f,1.0f,
        -0.5f, 0.5f, 0.5f, 0.70f,0.80f,1.0f,
        -0.5f,-0.5f,-0.5f, 0.70f,0.80f,1.0f,
        -0.5f, 0.5f, 0.5f, 0.70f,0.80f,1.0f,
        -0.5f, 0.5f,-0.5f, 0.70f,0.80f,1.0f,

        // Right
         0.5f,-0.5f,-0.5f, 1.0f,0.90f,0.60f,
         0.5f,-0.5f, 0.5f, 1.0f,0.90f,0.60f,
         0.5f, 0.5f, 0.5f, 1.0f,0.90f,0.60f,
         0.5f,-0.5f,-0.5f, 1.0f,0.90f,0.60f,
         0.5f, 0.5f, 0.5f, 1.0f,0.90f,0.60f,
         0.5f, 0.5f,-0.5f, 1.0f,0.90f,0.60f,

        // Top
        -0.5f, 0.5f,-0.5f, 0.85f,0.70f,0.95f,
         0.5f, 0.5f,-0.5f, 0.85f,0.70f,0.95f,
         0.5f, 0.5f, 0.5f, 0.85f,0.70f,0.95f,
        -0.5f, 0.5f,-0.5f, 0.85f,0.70f,0.95f,
         0.5f, 0.5f, 0.5f, 0.85f,0.70f,0.95f,
        -0.5f, 0.5f, 0.5f, 0.85f,0.70f,0.95f,

        // Bottom
        -0.5f,-0.5f,-0.5f, 0.65f,0.90f,0.90f,
         0.5f,-0.5f,-0.5f, 0.65f,0.90f,0.90f,
         0.5f,-0.5f, 0.5f, 0.65f,0.90f,0.90f,
        -0.5f,-0.5f,-0.5f, 0.65f,0.90f,0.90f,
         0.5f,-0.5f, 0.5f, 0.65f,0.90f,0.90f,
        -0.5f,-0.5f, 0.5f, 0.65f,0.90f,0.90f
    };

    GLuint cubeVAO, cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);

    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices),
                 cubeVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE,
        6 * sizeof(float), (void*)0
    );
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE,
        6 * sizeof(float),
        (void*)(3 * sizeof(float))
    );
    glEnableVertexAttribArray(1);

    // ============================================================
    // 5 x 5 x 5 GRID
    // Five lines along each axis, 1 unit apart.
    // Coordinates: -2, -1, 0, 1, 2
    // ============================================================

    const int GRID_LINES = 5;
    const float GRID_MIN = -2.0f;
    const float GRID_MAX = 2.0f;

    std::vector<float> gridVertices;

    glm::vec3 gridColor(0.35f, 0.35f, 0.35f);

    auto addGridVertex = [&](float x, float y, float z)
    {
        gridVertices.push_back(x);
        gridVertices.push_back(y);
        gridVertices.push_back(z);
        gridVertices.push_back(gridColor.r);
        gridVertices.push_back(gridColor.g);
        gridVertices.push_back(gridColor.b);
    };

    // Lines parallel to X
    for (int y = 0; y < GRID_LINES; y++)
    {
        for (int z = 0; z < GRID_LINES; z++)
        {
            float yy = GRID_MIN + y;
            float zz = GRID_MIN + z;

            addGridVertex(GRID_MIN, yy, zz);
            addGridVertex(GRID_MAX, yy, zz);
        }
    }

    // Lines parallel to Y
    for (int x = 0; x < GRID_LINES; x++)
    {
        for (int z = 0; z < GRID_LINES; z++)
        {
            float xx = GRID_MIN + x;
            float zz = GRID_MIN + z;

            addGridVertex(xx, GRID_MIN, zz);
            addGridVertex(xx, GRID_MAX, zz);
        }
    }

    // Lines parallel to Z
    for (int x = 0; x < GRID_LINES; x++)
    {
        for (int y = 0; y < GRID_LINES; y++)
        {
            float xx = GRID_MIN + x;
            float yy = GRID_MIN + y;

            addGridVertex(xx, yy, GRID_MIN);
            addGridVertex(xx, yy, GRID_MAX);
        }
    }

    GLuint gridVAO, gridVBO;
    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);

    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        gridVertices.size() * sizeof(float),
        gridVertices.data(),
        GL_STATIC_DRAW
    );

    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE,
        6 * sizeof(float), (void*)0
    );
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE,
        6 * sizeof(float),
        (void*)(3 * sizeof(float))
    );
    glEnableVertexAttribArray(1);

    std::string vertexSource =
        loadShaderSource("shaders/vertex.glsl");

    std::string fragmentSource =
        loadShaderSource("shaders/fragment.glsl");

    if (vertexSource.empty() || fragmentSource.empty())
    {
        return -1;
    }

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    const char* vertexCode = vertexSource.c_str();

    glShaderSource(vertexShader, 1, &vertexCode, nullptr);
    glCompileShader(vertexShader);

    int success;
    char infoLog[512];

    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        glGetShaderInfoLog(
            vertexShader, 512, nullptr, infoLog
        );
        std::cerr << "Vertex Shader Error:\n"
                  << infoLog << std::endl;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

    const char* fragmentCode = fragmentSource.c_str();

    glShaderSource( fragmentShader, 1, &fragmentCode, nullptr );
    glCompileShader(fragmentShader);
    glGetShaderiv( fragmentShader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        glGetShaderInfoLog(
            fragmentShader, 512, nullptr, infoLog
        );
        std::cerr << "Fragment Shader Error:\n" << infoLog << std::endl;
    }

    GLuint shaderProgram = glCreateProgram();

    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glGetProgramiv(
        shaderProgram, GL_LINK_STATUS, &success
    );

    if (!success)
    {
        glGetProgramInfoLog( shaderProgram, 512, nullptr, infoLog );
        std::cerr << "Shader Linking Error:\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    glm::mat4 view = glm::mat4(1.0f);

    view = glm::translate(
        view,
        glm::vec3(0.0f, 0.0f, -8.0f)
    );

    glm::mat4 projection = glm::perspective(
        glm::radians(45.0f),
        800.0f / 600.0f,
        0.1f,
        100.0f
    );

    glm::vec3 cubePosition(0.5f, 0.5f, 0.5f);

    glUseProgram(shaderProgram);

    GLint modelLoc =
        glGetUniformLocation(shaderProgram, "model");

    GLint viewLoc =
        glGetUniformLocation(shaderProgram, "view");

    GLint projectionLoc =
        glGetUniformLocation(shaderProgram, "projection");

    glUniformMatrix4fv(
        viewLoc, 1, GL_FALSE, glm::value_ptr(view)
    );

    glUniformMatrix4fv(
        projectionLoc, 1, GL_FALSE,
        glm::value_ptr(projection)
    );

    while (!glfwWindowShouldClose(window))
    {
        glClearColor(
            0.95f, 0.95f, 0.95f, 1.0f
        );

        glClear(
            GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT
        );

        glUseProgram(shaderProgram);

        glm::mat4 gridModel = glm::mat4(1.0f);

        glUniformMatrix4fv(
            modelLoc, 1, GL_FALSE,
            glm::value_ptr(gridModel)
        );

        glBindVertexArray(gridVAO);

        glDrawArrays(
            GL_LINES,
            0,
            150
        );

        glm::mat4 cubeModel = glm::mat4(1.0f);

        cubeModel = glm::translate(
            cubeModel,
            cubePosition
        );

        glUniformMatrix4fv(
            modelLoc, 1, GL_FALSE,
            glm::value_ptr(cubeModel)
        );

        glBindVertexArray(cubeVAO);

        glDrawArrays( GL_TRIANGLES, 0, 36 );
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);

    glDeleteVertexArrays(1, &gridVAO);
    glDeleteBuffers(1, &gridVBO);

    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
