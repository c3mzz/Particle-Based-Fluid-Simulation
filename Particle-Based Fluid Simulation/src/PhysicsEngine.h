#pragma once

#include <vector>
#include "Particle.h"

class PhysicsEngine {
public:
    float G = 0.0001f;
    void UpdateRK4(std::vector<Particle>& Particles, float dt);

private:
    std::vector<glm::vec2> GetAccelerations(const std::vector<glm::vec2>& positions, const std::vector<Particle>& Particles);
};