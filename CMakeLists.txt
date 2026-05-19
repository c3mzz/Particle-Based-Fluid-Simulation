cmake_minimum_required(VERSION 3.16)
project(FluidSimulation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCE_FILES
    src/main.cpp
    src/FluidSimulation.cpp
    src/Shader.cpp
    src/Renderer.cpp
    src/VertexArray.cpp
    src/VertexBuffer.cpp
    src/IndexBuffer.cpp
)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/GLFW/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/GLEW/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/GLM
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH_DIR "x64")
else()
    set(ARCH_DIR "Win32")
endif()

target_link_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/GLFW/lib-vc2022
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/GLEW/lib/Release/${ARCH_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE glew32 glfw3 opengl32)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/Particle-Based Fluid Simulation/res"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/res"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/GLEW/bin/Release/${ARCH_DIR}/glew32.dll"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
)