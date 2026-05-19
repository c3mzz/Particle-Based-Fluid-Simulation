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

static float g_InteractionRadius = 1.5f;

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

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Interactive SPH Fluid Engine", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (glewInit() != GLEW_OK)
        std::cout << "Glew initialization error!" << std::endl;

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

        float viewWidth = 26.66f;
        float viewHeight = 15.0f;

        while (!glfwWindowShouldClose(window))
        {
            renderer.Clear();

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
            float worldMouseX = ((static_cast<float>(mouseX) / 1280.0f) * 2.0f - 1.0f) * (viewWidth / 2.0f);
            float worldMouseY = -((static_cast<float>(mouseY) / 720.0f) * 2.0f - 1.0f) * (viewHeight / 2.0f);

            float interactionStrength = 0.0f;
            bool isInteracting = false;

            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
                interactionStrength = 180.0f; // Attract
                isInteracting = true;
            }
            else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
                interactionStrength = -180.0f; // Repel
                isInteracting = true;
            }
            fluidSim.SetInteraction(glm::vec2(worldMouseX, worldMouseY), interactionStrength, g_InteractionRadius);

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
                std::cout << "[0] Exit and Resume Simulation\n";
                std::cout << "--------------------------------------------------\n";
                std::cout << "Select a setting to modify (0-5): ";

                int selection;
                std::cin >> selection;
                if (selection >= 1 && selection <= 5) {
                    std::cout << "Enter custom value: ";
                    float newSettingValue;
                    std::cin >> newSettingValue;
                    switch (selection) {
                    case 1: config.gravity = newSettingValue; break;
                    case 2: config.targetDensity = newSettingValue; break;
                    case 3: config.pressureMultiplier = newSettingValue; break;
                    case 4: config.nearPressureMultiplier = newSettingValue; break;
                    case 5: config.viscosityStrength = newSettingValue; break;
                    }
                    std::cout << "Parameter updated.\n";
                }
            }
            f1PressedLastFrame = f1Pressed;

            // --- PHYSICS ---
            if (!isPaused) {
                for (int step = 0; step < 3; step++) {
                    fluidSim.Step(0.0022f);
                }
            }
            else {
                fluidSim.SetInteraction(glm::vec2(worldMouseX, worldMouseY), 0.0f, g_InteractionRadius);
            }


            // --- RENDER ---
            shader.Bind();
            shader.SetUniformMat4("u_MVP", mvp);
            shader.SetUniform1i("u_OverrideColor", 0);
            shader.SetUniform1f("u_ParticleRadius", fluidSim.GetParticleRadius());

            masterVAO.Bind();
            masterIBO.Bind();

            // Position Attributes
            glBindBuffer(GL_ARRAY_BUFFER, fluidSim.GetPositionBuffer());
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
            glVertexAttribDivisor(1, 1);

            // Velocity Attributes
            glBindBuffer(GL_ARRAY_BUFFER, fluidSim.GetVelocityBuffer());
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
            glVertexAttribDivisor(2, 1);

            // Draw all
            glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(circleIndices.size()), GL_UNSIGNED_INT, 0, fluidSim.GetParticleCount());

            glVertexAttribDivisor(1, 0);
            glVertexAttribDivisor(2, 0);
            glDisableVertexAttribArray(1);
            glDisableVertexAttribArray(2);

            // --- RENDER MOUSE ---
            if (isInteracting) {
                shader.SetUniform1i("u_OverrideColor", 1);
                shader.SetUniform4f("u_CustomColor", 0.0f, 1.0f, 0.2f, 1.0f);
                shader.SetUniform1f("u_ParticleRadius", g_InteractionRadius);

                glVertexAttrib2f(1, worldMouseX, worldMouseY);
                glVertexAttrib2f(2, 0.0f, 0.0f);

                glDrawArrays(GL_LINE_LOOP, 1, segments);
            }

            glDisableVertexAttribArray(1);
            glDisableVertexAttribArray(2);

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

    glfwTerminate();
    return 0;
}