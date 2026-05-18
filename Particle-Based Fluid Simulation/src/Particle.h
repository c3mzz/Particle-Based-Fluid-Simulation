#pragma once

#include <glm/glm.hpp>
#include <deque>
#include <vector>

struct Particle {
    glm::vec2 pos;
    glm::vec2 vel;
    glm::vec2 force;
    float mass;
    float radius;
    float density;
    float pressure;
    float r, g, b;
};

void initializeGas(std::vector<Particle>& particles, int particleCount);