#include "PhysicsEngine.h"
#include <cmath>

std::vector<glm::vec2> PhysicsEngine::GetAccelerations(const std::vector<glm::vec2>& positions, const std::vector<Particle>& Particles)
{
    int n = Particles.size();
    std::vector<glm::vec2> acc(n, glm::vec2(0.0f, -0.000981f));

    return acc;
}

void PhysicsEngine::UpdateRK4(std::vector<Particle>& Particles, float dt)
{
    int n = Particles.size();

    std::vector<glm::vec2> pos_k(n), vel_k(n);

    // --- K1 ---
    for (int i = 0; i < n; ++i) {
        pos_k[i] = Particles[i].pos;
        vel_k[i] = Particles[i].vel;
    }
    std::vector<glm::vec2> acc_k1 = GetAccelerations(pos_k, Particles);

    // --- K2 ---
    for (int i = 0; i < n; ++i) {
        pos_k[i] = Particles[i].pos + vel_k[i] * (dt * 0.5f);
        vel_k[i] = Particles[i].vel + acc_k1[i] * (dt * 0.5f);
    }
    std::vector<glm::vec2> acc_k2 = GetAccelerations(pos_k, Particles);

    // --- K3 ---
    for (int i = 0; i < n; ++i) {
        pos_k[i] = Particles[i].pos + vel_k[i] * (dt * 0.5f);
        vel_k[i] = Particles[i].vel + acc_k2[i] * (dt * 0.5f);
    }
    std::vector<glm::vec2> acc_k3 = GetAccelerations(pos_k, Particles);

    // --- K4 ---
    for (int i = 0; i < n; ++i) {
        pos_k[i] = Particles[i].pos + vel_k[i] * dt;
        vel_k[i] = Particles[i].vel + acc_k3[i] * dt;
    }
    std::vector<glm::vec2> acc_k4 = GetAccelerations(pos_k, Particles);

    // ----------

    for (int i = 0; i < n; ++i) {
        glm::vec2 vk1 = Particles[i].vel;
        glm::vec2 vk2 = Particles[i].vel + acc_k1[i] * (dt * 0.5f);
        glm::vec2 vk3 = Particles[i].vel + acc_k2[i] * (dt * 0.5f);
        glm::vec2 vk4 = Particles[i].vel + acc_k3[i] * dt;

        Particles[i].pos += (dt / 6.0f) * (vk1 + 2.0f * vk2 + 2.0f * vk3 + vk4);
        Particles[i].vel += (dt / 6.0f) * (acc_k1[i] + 2.0f * acc_k2[i] + 2.0f * acc_k3[i] + acc_k4[i]);
    }
}