cmake_minimum_required(VERSION 3.24)
project(ParallelPrefixSum LANGUAGES CXX)
add_subdirectory(${PROJECT_SOURCE_DIR}/extern/rang)
add_subdirectory(${PROJECT_SOURCE_DIR}/extern/glad)
add_subdirectory(${PROJECT_SOURCE_DIR}/extern/glfw)
add_executable(main ${PROJECT_SOURCE_DIR}/src/main.cpp)
set_target_properties(main PROPERTIES
    CXX_STANDARD 20
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)
target_include_directories(main PRIVATE
    ${PROJECT_SOURCE_DIR}/extern/rang/include
    ${PROJECT_SOURCE_DIR}/extern/glad/include    
    ${PROJECT_SOURCE_DIR}/extern/glfw/include
    ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(main PRIVATE
    glad
    glfw
)