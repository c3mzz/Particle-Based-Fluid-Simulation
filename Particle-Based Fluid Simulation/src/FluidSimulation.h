#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>

class FluidSimulation {
public:
    struct Settings {
        float gravity = -13.0f;
        float smoothingRadius = 0.45f;
        float targetDensity = 55.0f;
        float pressureMultiplier = 500.0f;
        float nearPressureMultiplier = 18.0f;
        float viscosityStrength = 0.06f;
        float collisionDamping = 0.5f;
        glm::vec2 boundsSize = glm::vec2(26.66f, 15.0f);
    };

private:
    unsigned int m_NumParticles;
    unsigned int m_ComputeProgram = 0;

    GLuint m_SSBO_Positions = 0;
    GLuint m_SSBO_PredictedPositions = 0;
    GLuint m_SSBO_Velocities = 0;
    GLuint m_SSBO_Densities = 0;
    GLuint m_SSBO_SpatialKeys = 0;
    GLuint m_SSBO_SpatialOffsets = 0;
    GLuint m_SSBO_SortedIndices = 0;

    GLuint m_SSBO_SortTarget_Positions = 0;
    GLuint m_SSBO_SortTarget_PredictedPositions = 0;
    GLuint m_SSBO_SortTarget_Velocities = 0;

    Settings m_Settings;

    std::vector<GLuint> m_CpuKeys;
    std::vector<GLuint> m_CpuOffsets;
    std::vector<GLuint> m_CpuIndices;

    glm::vec2 m_InteractionPoint = glm::vec2(0.0f);
    float m_InteractionStrength = 0.0f;
    float m_InteractionRadius = 3.5f;

    bool CompileComputeShader(const std::string& shaderCode);
    void UpdateSpatialOffsets();

public:
    FluidSimulation(int numParticles, const std::vector<glm::vec2>& initialPositions);
    ~FluidSimulation();

    void Step(float dt);

    void SetInteraction(glm::vec2 point, float strength, float radius) {
        m_InteractionPoint = point;
        m_InteractionStrength = strength;
        m_InteractionRadius = radius;
    }

    GLuint GetPositionBuffer() const { return m_SSBO_Positions; }
    GLuint GetVelocityBuffer() const { return m_SSBO_Velocities; }
    unsigned int GetParticleCount() const { return m_NumParticles; }
    float GetParticleRadius() const { return 0.048f; }

    Settings& GetSettings() { return m_Settings; }
};