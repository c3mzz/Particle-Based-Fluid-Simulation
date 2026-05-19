#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>

#include "VertexArray.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Shader.h"
#include "Renderer.h"
#include "FluidSimulation.h"

static int g_WindowWidth = 1280;
static int g_WindowHeight = 720;
static bool g_WindowSizeChanged = false;
static float g_InteractionRadius = 1.5f;

enum class InteractionMode { ForceField = 1, CreateStatic = 2, CreateDynamic = 3 };
InteractionMode currentMode = InteractionMode::ForceField;

bool isDrawingBlock = false;
glm::vec2 blockStartPoint(0.0f);

bool key1PressedLast = false;
bool key2PressedLast = false;
bool key3PressedLast = false;
bool leftClickLast = false;
bool rightClickLast = false;

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    if (width == 0 || height == 0) return;

    g_WindowWidth = width;
    g_WindowHeight = height;
    g_WindowSizeChanged = true;

    glViewport(0, 0, width, height);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    g_InteractionRadius += static_cast<float>(yoffset) * 0.15f;

    if (g_InteractionRadius < 0.15f) g_InteractionRadius = 0.15f;
    if (g_InteractionRadius > 8.0f)  g_InteractionRadius = 8.0f;
}

int main()
{
    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(g_WindowWidth, g_WindowHeight, "Interactive SPH Fluid Engine", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (glewInit() != GLEW_OK)
        std::cout << "Glew initialization error!" << std::endl;

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetScrollCallback(window, scroll_callback);

    {
        Shader shader("res/shaders/Basic.shader");
        shader.Bind();

        int segments = 16;
        float meshRadius = 1.0f;
        std::vector<float> circleVertices;
        std::vector<unsigned int> circleIndices;

        circleVertices.push_back(0.0f);
        circleVertices.push_back(0.0f);

        const float PI = 3.14159265359f;
        for (int i = 0; i <= segments; i++) {
            float theta = i * (2.0f * PI / segments);
            circleVertices.push_back(meshRadius * std::sin(theta));
            circleVertices.push_back(meshRadius * std::cos(theta));
        }
        for (int i = 1; i <= segments; i++) {
            circleIndices.push_back(0);
            circleIndices.push_back(i);
            circleIndices.push_back(i + 1);
        }

        VertexArray masterVAO;
        VertexBuffer masterVBO(&circleVertices[0], static_cast<unsigned int>(circleVertices.size() * sizeof(float)));
        VertexBufferLayout circleLayout;
        circleLayout.Push<float>(2);
        masterVAO.AddBuffer(masterVBO, circleLayout);
        IndexBuffer masterIBO(&circleIndices[0], static_cast<unsigned int>(circleIndices.size()));

        float boxVertices[] = {
            -0.5f, -0.5f,
             0.5f, -0.5f,
             0.5f,  0.5f,
            -0.5f,  0.5f
        };
        unsigned int boxIndices[] = { 0, 1, 2, 2, 3, 0 };

        VertexArray boxVAO;
        VertexBuffer boxVBO(boxVertices, sizeof(boxVertices));
        VertexBufferLayout boxLayout;
        boxLayout.Push<float>(2);
        boxVAO.AddBuffer(boxVBO, boxLayout);
        IndexBuffer boxIBO(boxIndices, 6);

        int numParticles = 10000;
        std::vector<glm::vec2> initialPositions;

        int cols = 125;
        float spacing = 0.13f;
        for (int i = 0; i < numParticles; i++) {
            float x = (i % cols) * spacing - 8.12f;
            float y = (i / cols) * spacing - 5.5f;

            float jitterX = ((rand() % 100) / 100.0f) * 0.002f;
            float jitterY = ((rand() % 100) / 100.0f) * 0.002f;
            initialPositions.push_back(glm::vec2(x + jitterX, y + jitterY));
        }

        FluidSimulation fluidSim(numParticles, initialPositions);
        Renderer renderer;

        bool isPaused = false;
        bool spacePressedLastFrame = false;
        bool f1PressedLastFrame = false;

        float viewHeight = 15.0f;
        float viewWidth = viewHeight * (static_cast<float>(g_WindowWidth) / static_cast<float>(g_WindowHeight));

        while (!glfwWindowShouldClose(window))
        {
            renderer.Clear();

            if (g_WindowSizeChanged) {
                viewWidth = viewHeight * (static_cast<float>(g_WindowWidth) / static_cast<float>(g_WindowHeight));

                fluidSim.UpdateBounds(glm::vec2(viewWidth, viewHeight));
                g_WindowSizeChanged = false;
            }

            glm::mat4 proj = glm::ortho(-viewWidth / 2.0f, viewWidth / 2.0f, -viewHeight / 2.0f, viewHeight / 2.0f, -1.0f, 1.0f);
            glm::mat4 view = glm::mat4(1.0f);
            glm::mat4 mvp = proj * view;

            // --- PAUSE / RESUME HANDLING ---
            bool spacePressed = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
            if (spacePressed && !spacePressedLastFrame) {
                isPaused = !isPaused;
                std::cout << (isPaused ? "Simulation Paused.\n" : "Simulation Resumed.\n");
            }
            spacePressedLastFrame = spacePressed;

            // --- MOUSE SCREEN-TO-WORLD PROJECTION ---
            double mouseX, mouseY;
            glfwGetCursorPos(window, &mouseX, &mouseY);
            float worldMouseX = ((static_cast<float>(mouseX) / static_cast<float>(g_WindowWidth)) * 2.0f - 1.0f) * (viewWidth / 2.0f);
            float worldMouseY = -((static_cast<float>(mouseY) / static_cast<float>(g_WindowHeight)) * 2.0f - 1.0f) * (viewHeight / 2.0f);
            glm::vec2 currentMousePos(worldMouseX, worldMouseY);

            if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) currentMode = InteractionMode::ForceField;
            if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) currentMode = InteractionMode::CreateStatic;
            if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) currentMode = InteractionMode::CreateDynamic;

            bool leftClicked = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            bool rightClicked = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

            float interactionStrength = 0.0f;
            bool isInteracting = false;

            if (currentMode == InteractionMode::ForceField) {
                isDrawingBlock = false;
                if (leftClicked) {
                    interactionStrength = 180.0f; // Attract
                    isInteracting = true;
                }
                else if (rightClicked) {
                    interactionStrength = -180.0f; // Repel
                    isInteracting = true;
                }
                fluidSim.SetInteraction(currentMousePos, interactionStrength, g_InteractionRadius);
            }
            else if (currentMode == InteractionMode::CreateStatic || currentMode == InteractionMode::CreateDynamic) {
                fluidSim.SetInteraction(currentMousePos, 0.0f, g_InteractionRadius);

                if (leftClicked && !leftClickLast) {
                    isDrawingBlock = true;
                    blockStartPoint = currentMousePos;
                }
                if (!leftClicked && leftClickLast && isDrawingBlock) {
                    isDrawingBlock = false;
                    bool isStatic = (currentMode == InteractionMode::CreateStatic);
                    fluidSim.AddBlock(blockStartPoint, currentMousePos, isStatic);
                }
                if (rightClicked && !rightClickLast) {
                    fluidSim.DeleteBlockAt(currentMousePos);
                }
            }

            leftClickLast = leftClicked;
            rightClickLast = rightClicked;

            // --- CONFIG CONSOLE (F1) ---
            bool f1Pressed = glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS;
            if (f1Pressed && !f1PressedLastFrame) {
                std::cout << "\n==================================================\n";
                std::cout << "          FLUID SIMULATION DEBUG OPTIONS          \n";
                std::cout << "==================================================\n";
                FluidSimulation::Settings& config = fluidSim.GetSettings();
                std::cout << "[1] Gravity: " << config.gravity << "\n";
                std::cout << "[2] Target Density: " << config.targetDensity << "\n";
                std::cout << "[3] Pressure Multiplier: " << config.pressureMultiplier << "\n";
                std::cout << "[4] Near Pressure Multiplier: " << config.nearPressureMultiplier << "\n";
                std::cout << "[5] Viscosity Strength: " << config.viscosityStrength << "\n";
                std::cout << "[6] Smoothing Radius: " << config.smoothingRadius << "\n";
                std::cout << "[7] Collision Damping: " << config.collisionDamping << "\n";
                std::cout << "[8] Spawn Mass for Mode 3 Blocks: " << config.createdBlockMass << "\n";
                std::cout << "[0] Exit and Resume Simulation\n";
                std::cout << "--------------------------------------------------\n";
                std::cout << "Select a setting to modify (0-8): ";

                int selection;
                std::cin >> selection;
                if (selection >= 1 && selection <= 8) {
                    std::cout << "Enter custom value: ";
                    float newSettingValue;
                    std::cin >> newSettingValue;
                    switch (selection) {
                    case 1: config.gravity = newSettingValue; break;
                    case 2: config.targetDensity = newSettingValue; break;
                    case 3: config.pressureMultiplier = newSettingValue; break;
                    case 4: config.nearPressureMultiplier = newSettingValue; break;
                    case 5: config.viscosityStrength = newSettingValue; break;
                    case 6: config.smoothingRadius = newSettingValue; break;
                    case 7: config.collisionDamping = newSettingValue; break;
                    case 8: config.createdBlockMass = newSettingValue; break;
                    }
                    std::cout << "Parameter modified successfully!\n";
                }
                std::cout << "Returning to active simulation viewport...\n";
            }
            f1PressedLastFrame = f1Pressed;

            // --- PHYSICS ---
            if (!isPaused) {
                for (int step = 0; step < 8; step++) { 
                    fluidSim.Step(0.0022f); // 
                }
            }
            else {
                fluidSim.SetInteraction(currentMousePos, 0.0f, g_InteractionRadius);
            }


            // --- RENDER ---
            shader.Bind();
            shader.SetUniformMat4("u_MVP", mvp);

            shader.SetUniform1i("u_VertexMode", 0);
            shader.SetUniform1i("u_OverrideColor", 0);
            shader.SetUniform1f("u_ParticleRadius", fluidSim.GetParticleRadius());

            masterVAO.Bind();
            masterIBO.Bind();

            glBindBuffer(GL_ARRAY_BUFFER, fluidSim.GetPositionBuffer());
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
            glVertexAttribDivisor(1, 1);

            glBindBuffer(GL_ARRAY_BUFFER, fluidSim.GetVelocityBuffer());
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
            glVertexAttribDivisor(2, 1);

            glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(circleIndices.size()), GL_UNSIGNED_INT, nullptr, fluidSim.GetParticleCount());

            if (currentMode == InteractionMode::ForceField && isInteracting) {
                shader.SetUniform1i("u_VertexMode", 1);
                shader.SetUniform1i("u_OverrideColor", 1);
                shader.SetUniform4f("u_CustomColor", 0.0f, 1.0f, 0.2f, 1.0f);
                shader.SetUniform1f("u_ParticleRadius", g_InteractionRadius);

                glUniform2f(glGetUniformLocation(shader.GetRendererID(), "u_MousePos"), worldMouseX, worldMouseY);

                glDrawArrays(GL_LINE_LOOP, 1, segments);
            }

            glVertexAttribDivisor(1, 0);
            glVertexAttribDivisor(2, 0);
            glDisableVertexAttribArray(1);
            glDisableVertexAttribArray(2);

            boxVAO.Bind();
            boxIBO.Bind();

            shader.SetUniform1i("u_VertexMode", 2);
            shader.SetUniform1i("u_OverrideColor", 1);

            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            for (const auto& block : fluidSim.GetBlocks()) {
                if (block.isStatic) {
                    shader.SetUniform4f("u_CustomColor", 0.3f, 0.4f, 0.6f, 1.0f);
                }
                else {
                    shader.SetUniform4f("u_CustomColor", 0.8f, 0.5f, 0.2f, 1.0f);
                }

                glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(block.position, 0.0f));
                model = glm::rotate(model, block.angle, glm::vec3(0.0f, 0.0f, 1.0f));
                model = glm::scale(model, glm::vec3(block.halfSize * 2.0f, 1.0f));

                glm::mat4 blockMVP = mvp * model;
                shader.SetUniformMat4("u_MVP", blockMVP);
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
            }

            if (isDrawingBlock) {
                shader.SetUniform4f("u_CustomColor", 1.0f, 1.0f, 1.0f, 0.8f);

                glm::vec2 center = (blockStartPoint + currentMousePos) * 0.5f;
                glm::vec2 size = glm::vec2(std::abs(currentMousePos.x - blockStartPoint.x), std::abs(currentMousePos.y - blockStartPoint.y));

                glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(center, 0.0f));
                model = glm::scale(model, glm::vec3(size, 1.0f));
                glm::mat4 previewMVP = mvp * model;

                shader.SetUniformMat4("u_MVP", previewMVP);
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            }

            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

    glfwTerminate();
    return 0;
}