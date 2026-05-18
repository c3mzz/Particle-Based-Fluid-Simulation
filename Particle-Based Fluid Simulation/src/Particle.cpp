#include "Particle.h"
#include <random>
#include <vector>
#include <cmath>

void initializeGas(std::vector<Particle>& particles, int particleCount) {
    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_real_distribution<float> massDist(0.5f, 4.0f);
    std::uniform_real_distribution<float> colorDist(0.4f, 1.0f);

    float baseMass = 1.0f;
    float baseRadius = 0.08f;

    float maxSpeed = 2.0f;
    std::uniform_real_distribution<float> velX(-maxSpeed, maxSpeed);
    std::uniform_real_distribution<float> velY(-maxSpeed, maxSpeed);

    for (int i = 0; i < particleCount; i++) {
        Particle p;

        p.mass = massDist(gen);
        p.radius = baseRadius * std::sqrt(p.mass / baseMass);

        std::uniform_real_distribution<float> posX(-4.0f + p.radius, 4.0f - p.radius);
        std::uniform_real_distribution<float> posY(-3.0f + p.radius, 3.0f - p.radius);

        p.pos = glm::vec2(posX(gen), posY(gen));
        p.vel = glm::vec2(velX(gen), velY(gen));

        float massFactor = (p.mass - 0.5f) / (4.0f - 0.5f);
        p.r = 0.2f + 0.6f * massFactor;
        p.g = colorDist(gen) * (1.0f - massFactor * 0.5f);
        p.b = colorDist(gen);

        particles.push_back(p);
    }
}