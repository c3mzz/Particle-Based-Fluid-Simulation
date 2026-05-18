#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>

#include "VertexArray.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Shader.h"
#include "Renderer.h"
#include "Particle.h"
#include "PhysicsEngine.h"

int main()
{
    GLFWwindow* window;
    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);


    window = glfwCreateWindow(640, 480, "Hello World", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    glfwSwapInterval(1);

    if (glewInit() != GLEW_OK)
        std::cout << "Error!" << std::endl;

    std::cout << glGetString(GL_VERSION) << std::endl;
    {
        Shader shader("res/shaders/Basic.shader");
        shader.Bind();


        float camX = 0.0f;
        float camY = 0.0f;
        float camZoom = 1.0f;
        float camSpeed = 0.05f;


        // --- Gen Circle ---
        int segments = 36;
        float radius = 1.0f;
        std::vector<float> circleVertices;
        std::vector<unsigned int> circleIndices;

        circleVertices.push_back(0.0f);
        circleVertices.push_back(0.0f);

        const float PI = 3.14159265359f;
        for (int i = 0; i <= segments; i++) {
            float theta = i * (2 * PI / segments);
            circleVertices.push_back(radius * std::sin(theta));
            circleVertices.push_back(radius * std::cos(theta));
        }

        for (int i = 1; i <= segments; i++) {
            circleIndices.push_back(0);
            circleIndices.push_back(i);
            circleIndices.push_back(i+1);
        }

        VertexArray masterVAO;
        VertexBuffer masterVBO(&circleVertices[0], circleVertices.size() * sizeof(float));
        VertexBufferLayout circleLayout;
        circleLayout.Push<float>(2);
        masterVAO.AddBuffer(masterVBO, circleLayout);
        IndexBuffer masterIBO(&circleIndices[0], circleIndices.size());
        // ------------------


        glm::mat4 proj = glm::ortho(-4.0f, 4.0f, -3.0f, 3.0f, -1.0f, 1.0f);
        glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));

        std::vector<Particle> particles;
        initializeGas(particles, 100);

        PhysicsEngine physics;
        Renderer renderer;

        while (!glfwWindowShouldClose(window))
        {
            renderer.Clear();

            // --- Camera Input ---
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camY += camSpeed / camZoom;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camY -= camSpeed / camZoom;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camX -= camSpeed / camZoom;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camX += camSpeed / camZoom;

            // --- Zooming ---
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) camZoom *= 1.01f;
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) camZoom *= 0.99f;

            glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(-camX, -camY, 0.0f));

            float aspect = 4.0f / 3.0f;
            float width = 4.0f / camZoom;
            float height = 3.0f / camZoom;
            glm::mat4 proj = glm::ortho(-width, width, -height, height, -1.0f, 1.0f);

            physics.UpdateRK4(particles, 0.5f);
            
            float e = 0.8f;

            for (int i = 0; i < particles.size(); i++) {
                float r = particles[i].radius;

                if (particles[i].pos.x < -4.0f + r) {
                    particles[i].pos.x = -4.0f + r;
                    particles[i].vel.x = -particles[i].vel.x;
                }
                if (particles[i].pos.x > 4.0f - r) {
                    particles[i].pos.x = 4.0f - r;
                    particles[i].vel.x = -particles[i].vel.x;
                }
                if (particles[i].pos.y < -3.0f + r) {
                    particles[i].pos.y = -3.0f + r;
                    particles[i].vel.y = -particles[i].vel.y;
                }
                if (particles[i].pos.y > 3.0f - r) {
                    particles[i].pos.y = 3.0f - r;
                    particles[i].vel.y = -particles[i].vel.y;
                }
            }

            for (int i = 0; i < particles.size(); i++) {
                for (int j = i + 1; j < particles.size(); j++) {
                    glm::vec2 delta = particles[j].pos - particles[i].pos;
                    float distance = glm::length(delta);
                    float minDistance = particles[i].radius + particles[j].radius;

                    if (distance < minDistance) {
                        if (distance == 0.0f) continue;

                        glm::vec2 normal = delta / distance;

                        float v1n = glm::dot(particles[i].vel, normal);
                        float v2n = glm::dot(particles[j].vel, normal);

                        if (v1n - v2n > 0.0f) {
                            float m1 = particles[i].mass;
                            float m2 = particles[j].mass;

                            float v1n_new = (m1 * v1n + m2 * v2n - m2 * e * (v1n - v2n)) / (m1 + m2);
                            float v2n_new = (m1 * v1n + m2 * v2n + m1 * e * (v1n - v2n)) / (m1 + m2);

                            particles[i].vel += normal * (v1n_new - v1n);
                            particles[j].vel += normal * (v2n_new - v2n);

                            float overlap = minDistance - distance;
                            float totalMass = m1 + m2;

                            particles[i].pos -= normal * (overlap * (m2 / totalMass));
                            particles[j].pos += normal * (overlap * (m1 / totalMass));
                        }
                    }
                }
            }

            shader.Bind();
            for (const Particle& p : particles)
            {
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(p.pos.x, p.pos.y, 0.0f));
                model = glm::scale(model, glm::vec3(p.radius, p.radius, 1.0f));
                glm::mat4 mvp = proj * view * model;

                shader.SetUniformMat4("u_MVP", mvp);
                shader.SetUniform4f("u_Color", p.r, p.g, p.b, 1.0f);

                renderer.Draw(masterVAO, masterIBO, shader);
            }

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

    }
    glfwTerminate();
    return 0;
}
