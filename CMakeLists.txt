cmake_minimum_required(VERSION 3.16)
project(FluidSimulation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCE_FILES
    "Particle-Based Fluid Simulation/src/main.cpp"
    "Particle-Based Fluid Simulation/src/FluidSimulation.cpp"
    "Particle-Based Fluid Simulation/src/Shader.cpp"
    "Particle-Based Fluid Simulation/src/Renderer.cpp"
    "Particle-Based Fluid Simulation/src/VertexArray.cpp"
    "Particle-Based Fluid Simulation/src/VertexBuffer.cpp"
    "Particle-Based Fluid Simulation/src/IndexBuffer.cpp"
)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/GLFW/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/GLEW/glew-2.3.1/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/GLM
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH_DIR "x64")
else()
    set(ARCH_DIR "Win32")
endif()

target_link_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/GLFW/lib-vc2022
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/GLEW/glew-2.3.1/lib/Release/${ARCH_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE glew32 glfw3 opengl32)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Particle-Based Fluid Simulation/res")
    set(RES_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Particle-Based Fluid Simulation/res")
else()
    set(RES_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/res")
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${RES_SRC_DIR}"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/res"
    COMMENT "Copying assets simulator folder to the build target directory..."
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/GLEW/glew-2.3.1/bin/Release/${ARCH_DIR}/glew32.dll"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
)
