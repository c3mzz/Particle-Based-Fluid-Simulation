#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>

struct PhysicalBlock {
    glm::vec2 position;       
    glm::vec2 halfSize;       
    glm::vec2 velocity;       
    float angle;              
    float angularVelocity;    
    float mass;               
    float momentOfInertia;    
    int isStatic;             
    int forceX;               
    int forceY;               
    int torqueAcc;            
    glm::vec2 padding;        
};

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
        float createdBlockMass = 800.0f;
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

    std::vector<PhysicalBlock> m_Blocks;

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
    void UpdateBounds(const glm::vec2& newBounds) { m_Settings.boundsSize = newBounds; }
    Settings& GetSettings() { return m_Settings; }

    void AddBlock(const glm::vec2& p1, const glm::vec2& p2, bool isStatic) {
        PhysicalBlock block;
        glm::vec2 minB = glm::vec2(std::min(p1.x, p2.x), std::min(p1.y, p2.y));
        glm::vec2 maxB = glm::vec2(std::max(p1.x, p2.x), std::max(p1.y, p2.y));

        block.position = (minB + maxB) * 0.5f;
        block.halfSize = (maxB - minB) * 0.5f;
        block.velocity = glm::vec2(0.0f);
        block.angle = 0.0f;
        block.angularVelocity = 0.0f;

        block.mass = m_Settings.createdBlockMass;

        float width = block.halfSize.x * 2.0f;
        float height = block.halfSize.y * 2.0f;
        block.momentOfInertia = block.mass * (width * width + height * height) / 12.0f;

        block.isStatic = isStatic ? 1 : 0;
        block.forceX = 0;
        block.forceY = 0;
        block.torqueAcc = 0;
        block.padding = glm::vec2(0.0f);
        m_Blocks.push_back(block);
    }

    void DeleteBlockAt(const glm::vec2& mousePos) {
        for (auto it = m_Blocks.begin(); it != m_Blocks.end(); ++it) {

            glm::vec2 localPos = mousePos - it->position;

            float cosA = std::cos(-it->angle);
            float sinA = std::sin(-it->angle);
            float rotX = localPos.x * cosA - localPos.y * sinA;
            float rotY = localPos.x * sinA + localPos.y * cosA;

            if (std::abs(rotX) <= it->halfSize.x && std::abs(rotY) <= it->halfSize.y) {
                m_Blocks.erase(it);
                break;
            }
        }
    }

    const std::vector<PhysicalBlock>& GetBlocks() const { return m_Blocks; }
    std::vector<PhysicalBlock>& GetModifiableBlocks() { return m_Blocks; }
};