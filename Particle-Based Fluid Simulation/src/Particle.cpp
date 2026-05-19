#include "Particle.h"
#include <random>
#include <vector>
#include <cmath>
#include <algorithm>

void initializeGas(std::vector<Particle>& particles, int particleCount) {
    if (particleCount <= 0) return;

    const float mass = 1.0f;
    const float gap = 0.1f;
    const float radius = 0.15f;
    const float aspect_ratio = 4.0f / 3.0f;

    int cols = static_cast<int>(std::round(std::sqrt(particleCount * (3.0f / 4.0f))));
    cols = std::max(1, cols);

    float spacing = (2.0f * radius) + gap;

    for (int i = 0; i < particleCount; i++) {
        Particle p;
        p.mass = mass;
        p.radius = radius;
        p.vel = glm::vec2(0.0f, 0.0f);

        p.r = 1.0f;
        p.g = 0.4f;
        p.b = 0.1f;

        int col = i % cols;
        int row = i / cols;

        p.pos.x = col * spacing;
        p.pos.y = row * spacing;

        particles.push_back(p);
    }
}