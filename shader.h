#ifndef SHADER_H // if SHADER_H is not defined, include the code below
#define SHADER_H // define a macro SHADER_H (to mark that this header file has been included)

#include <string>
#include <fstream> // for reading files
#include <sstream> // for handling string streams
#include "libs/glad/glad.h"
#include "libs/glm/glm.hpp"
#include "libs/glm/gtc/type_ptr.hpp"

class Shader
{
public:
    unsigned int shaderProgram;

    // constructor that generates the graphics pipeline on the fly when the class instance is initialized
    Shader(const char *vertexPath, const char *fragmentPath)
    {
        // 1. retrieve the vertex/fragment shader source code from filePath
        std::string vertexCode, fragmentCode;
        std::ifstream vShaderFile, fShaderFile;
        std::stringstream vShaderStream, fShaderStream;

        vShaderFile.open(vertexPath);
        fShaderFile.open(fragmentPath);
        // read file's buffer contents into streams
        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();
        // close file handlers
        vShaderFile.close();
        fShaderFile.close();
        // convert stream into string
        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();
        // convert into C-style string because OpenGL expects a pointer to char array
        const char *vertexShaderSource = vertexCode.c_str();
        const char *fragmentShaderSource = fragmentCode.c_str();

        // 2. compile shaders
        unsigned int vertexShader;
        vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);

        unsigned int fragmentShader;
        fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);

        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    // define a class function that activates shader program
    void use()
    {
        glUseProgram(shaderProgram);
    }

    // utility functions to set uniform variables
    void setInt(const std::string &name, int value) const
    {
        glUniform1i(glGetUniformLocation(shaderProgram, name.c_str()), value);
    }

    void setFloat(const std::string &name, float value) const
    {
        glUniform1f(glGetUniformLocation(shaderProgram, name.c_str()), value);
    }

    void setBool(const std::string &name, bool value) const
    {
        glUniform1i(glGetUniformLocation(shaderProgram, name.c_str()), (int)value);
    }

    void setVec3(const std::string &name, glm::vec3 value) const
    {
        glUniform3fv(glGetUniformLocation(shaderProgram, name.c_str()), 1, glm::value_ptr(value));
    }

    void setVec4(const std::string &name, glm::vec4 value) const
    {
        glUniform4fv(glGetUniformLocation(shaderProgram, name.c_str()), 1, glm::value_ptr(value));
    }

    void setMat4(const std::string &name, glm::mat4 value) const
    {
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, name.c_str()), 1, GL_FALSE, glm::value_ptr(value));
    }
};

#endif
