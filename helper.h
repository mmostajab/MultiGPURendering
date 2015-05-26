#ifndef __HELPER_H__
#define __HELPER_H__

#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>


static std::string convertFileToString(const std::string& filename);
static void show_compiler_error(GLuint shader);

// read the file content and generate a string from it.
static std::string convertFileToString(const std::string& filename) {
    std::ifstream ifile(filename.c_str());
    if (!ifile){
        return std::string("");
    }

    return std::string(std::istreambuf_iterator<char>(ifile), (std::istreambuf_iterator<char>()));

}

static std::string convertNumToString(const int& num){
  char buf[11];
  sprintf(buf, "%d", num);
  return std::string(buf);
}

static std::string convertNumToString(const unsigned int& num){
  return convertNumToString(static_cast<int>(num));
}

static void show_compiler_error(GLuint shader) {
    GLint isCompiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

        // The maxLength includes the NULL character
        std::vector<GLchar> errorLog(maxLength);
        glGetShaderInfoLog(shader, maxLength, &maxLength, &errorLog[0]);

        std::cout << "Shader " << shader << " Log: \n";
        std::cout << ((char*)&errorLog[0]);

        // Provide the infolog in whatever manor you deem best.
        // Exit with failure.
        glDeleteShader(shader); // Don't leak the shader.
        return;
    }
}

static GLuint compile_link_vs_fs(const std::string& vert_shader_file, const std::string& frag_shader_file) {
    GLuint vertex_shader, fragment_shader;
    std::string vertex_shader_source = convertFileToString(vert_shader_file);
    std::string coord_sys_shader_fragment_source = convertFileToString(frag_shader_file);

    if (vertex_shader_source.size() == 0 || coord_sys_shader_fragment_source.size() == 0){
        std::cout << "Problem in finding " << vert_shader_file << " or " << frag_shader_file << std::endl;
        return -1;
    }


    vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    const GLchar* vertex_shader_sourcePtr = vertex_shader_source.c_str();
    const GLchar* fragment_shader_sourcePtr = coord_sys_shader_fragment_source.c_str();

    glShaderSource(vertex_shader, 1, &vertex_shader_sourcePtr, NULL);
    glShaderSource(fragment_shader, 1, &fragment_shader_sourcePtr, NULL);

    glCompileShader(vertex_shader);
    glCompileShader(fragment_shader);

    show_compiler_error(vertex_shader);
    show_compiler_error(fragment_shader);

    GLuint shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgramARB(shader_program);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    return shader_program;
}

static bool findString(const std::string& all_string, const std::string& search_string){
  size_t index = all_string.find(search_string);
  if (std::string::npos == index){
    return false;
  } else {
    return true;
  }
}

#endif
