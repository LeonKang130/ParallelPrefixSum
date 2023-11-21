#include <iostream>
#include <fstream>
#include <array>
#include <string>
#include <stdexcept>

#include <rang.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define MINIMUM_BINS (16 * 1024)
#define NUM_BINS (16 * 1024 * 1024)

GLuint CreateComputeProgram(const char* filename) {
    std::fstream fstream;
    fstream.open(filename);
    std::string content, buffer(4096, '\0');
    while (fstream.read(&buffer[0], 4096)) {
        content.append(buffer, 0, fstream.gcount());
    }
    content.append(buffer, 0, fstream.gcount());
    std::array<const char*, 1> sources { content.data() };
    GLuint program = glCreateProgram();
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, sources.data(), nullptr);
    glCompileShader(shader);
    int success; char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << rang::fg::red << "[Error]" << rang::fg::reset << " Error compiling shader file: " << filename << std::endl;
        std::cerr << infoLog << std::endl;
        throw std::runtime_error("Shader compiling failure.");
    }
    glAttachShader(program, shader);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << rang::fg::red << "[Error]" << rang::fg::reset << " Error linking shader program: " << infoLog << std::endl;
        throw std::runtime_error("Compute program linking failure.");
    }
    glDetachShader(program, shader);
    glDeleteShader(shader);
    return program;
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    auto window = glfwCreateWindow(1, 1, "Parallel Prefix Sum", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    // Allocate buffers for photon emitting, sorting and collecting
    GLuint uploadBuffer; glCreateBuffers(1, &uploadBuffer);
    glNamedBufferStorage(uploadBuffer, NUM_BINS * sizeof(int), nullptr, GL_MAP_WRITE_BIT | GL_MAP_READ_BIT | GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT);
    GLuint inputBuffer; glCreateBuffers(1, &inputBuffer);
    glNamedBufferStorage(inputBuffer, NUM_BINS * sizeof(int), nullptr, GL_DYNAMIC_STORAGE_BIT);
    GLuint outputBuffer; glCreateBuffers(1, &outputBuffer);
    glNamedBufferStorage(outputBuffer, NUM_BINS * sizeof(int), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, inputBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, outputBuffer);
    GLuint downloadBuffer; glCreateBuffers(1, &downloadBuffer);
    glNamedBufferStorage(downloadBuffer, NUM_BINS * sizeof(int), nullptr, GL_MAP_READ_BIT | GL_DYNAMIC_STORAGE_BIT | GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT);
    // Write input data to input buffer
    int* inputData = reinterpret_cast<int*>(glMapNamedBufferRange(uploadBuffer, 0, NUM_BINS * sizeof(int), GL_MAP_WRITE_BIT));
    for (int i = 0; i < NUM_BINS; i++) {
        inputData[i] = ((i << 12 | i >> 20) ^ i) & 15;
    }
    glUnmapNamedBuffer(uploadBuffer);
    glCopyNamedBufferSubData(uploadBuffer, inputBuffer, 0, 0, NUM_BINS * sizeof(int));
    // Create query object
    GLuint query; glCreateQueries(GL_TIME_ELAPSED, 1, &query);
    GLuint prefixSumProgram = CreateComputeProgram("shader/prefix_sum.comp");
    GLuint cleanUpProgram = CreateComputeProgram("shader/clean_up.comp");
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glBeginQuery(GL_TIME_ELAPSED, query);
    glUseProgram(prefixSumProgram);
    glDispatchCompute(NUM_BINS / (32 * 32 * 2), 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glUseProgram(cleanUpProgram);
    glDispatchCompute(NUM_BINS / (32 * 32 * 32), 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glEndQuery(GL_TIME_ELAPSED);
    glCopyNamedBufferSubData(outputBuffer, downloadBuffer, 0, 0, NUM_BINS * sizeof(int));
    inputData = reinterpret_cast<int*>(glMapNamedBufferRange(uploadBuffer, 0, NUM_BINS * sizeof(int), GL_MAP_READ_BIT));
    int* outputData = reinterpret_cast<int*>(glMapNamedBufferRange(downloadBuffer, 0, NUM_BINS * sizeof(int), GL_MAP_READ_BIT));
    int prefixSum = 0;
    for (int i = 0; i < NUM_BINS; i++) {
        prefixSum += inputData[i];
        if (prefixSum != outputData[i]) {
            std::cout << rang::fg::red << "[Error] " << rang::fg::reset << " Wrong answer at " << i << ": " << prefixSum << ", " << outputData[i] << std::endl;
            break;
        }
    }
    glUnmapNamedBuffer(uploadBuffer);
    glUnmapNamedBuffer(downloadBuffer);
    GLuint timeElapsed;
    glGetQueryObjectuiv(query, GL_QUERY_RESULT, &timeElapsed);
    std::cout << rang::fg::yellow << "[Perf]" << rang::fg::reset << " " << timeElapsed / 1e6f << "ms" << std::endl;
    glDeleteQueries(1, &query);
    glDeleteProgram(prefixSumProgram);
    glDeleteProgram(cleanUpProgram);
    glDeleteBuffers(1, &uploadBuffer);
    glDeleteBuffers(1, &downloadBuffer);
    glDeleteBuffers(1, &inputBuffer);
    glDeleteBuffers(1, &outputBuffer);
    glfwDestroyWindow(window);
    glfwTerminate();
}
