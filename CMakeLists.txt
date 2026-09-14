cmake_minimum_required(VERSION 3.16)

project(CS5024_Assignment_1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(CS5024_Assignment_1
    src/main.cpp
)

target_link_libraries(
    CS5024_Assignment_1
    PRIVATE
    GLEW::GLEW
    glfw
    OpenGL::GL
)

configure_file(
    ${CMAKE_SOURCE_DIR}/shaders/vertex.glsl
    ${CMAKE_BINARY_DIR}/shaders/vertex.glsl
    COPYONLY
)

configure_file(
    ${CMAKE_SOURCE_DIR}/shaders/fragment.glsl
    ${CMAKE_BINARY_DIR}/shaders/fragment.glsl
    COPYONLY
)