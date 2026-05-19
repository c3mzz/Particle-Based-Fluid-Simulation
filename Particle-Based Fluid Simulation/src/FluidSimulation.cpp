#include "FluidSimulation.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <cmath>

FluidSimulation::FluidSimulation(int numParticles, const std::vector<glm::vec2>& initialPositions)
    : m_NumParticles(numParticles), m_ComputeProgram(0)
{
    std::ifstream stream("res/compute/Fluid.comp");
    if (!stream.is_open()) {
        std::cout << "CRITICAL ERROR: Could not open compute shader file!" << std::endl;
        return;
    }
    std::stringstream buffer;
    buffer << stream.rdbuf();
    CompileComputeShader(buffer.str());

    m_CpuKeys.resize(numParticles);
    m_CpuOffsets.resize(numParticles);
    m_CpuIndices.resize(numParticles);

    glGenBuffers(1, &m_SSBO_Positions);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO_Positions);
    glBufferData(GL_SHADER_STORAGE_BUFFER, numParticles * sizeof(glm::vec2), initialPositions.data(), GL_DYNAMIC_DRAW);

    glGenBuffers(1, &m_SSBO_PredictedPositions);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO_PredictedPositions);
    glBufferData(GL_SHADER_STORAGE_BUFFER, numParticles * sizeof(glm::vec2), nullptr, GL_DYNAMIC_DRAW);

    std::vector<glm::vec2> zeroVelocities(numParticles, glm::vec2(0.0f));
    glGenBuffers(1, &m_SSBO_Velocities);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO_Velocities);
    glBufferData(GL_SHADER_STORAGE_BUFFER, numParticles * sizeof(glm::vec2), zeroVelocities.data(), GL_DYNAMIC_DRAW);

    glGenBuffers(1, &m_SSBO_Densities);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO_Densities);
    glBufferData(GL_SHADER_STORAGE_BUFFER, numParticles * sizeof(glm::vec2), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &m_SSBO_SpatialKeys);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO_SpatialKeys);
    glBufferData(GL_SHADER_STORAGE_BUFFER, numParticles * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &m_SSBO_SpatialOffsets);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO_SpatialOffsets);
    glBufferData(GL_SHADER_STORAGE_BUFFER, numParticles * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &m_SSBO_SortedIndices);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO_SortedIndices);
    glBufferData(GL_SHADER_STORAGE_BUFFER, numParticles * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &m_SSBO_SortTarget_Positions);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO_SortTarget_Positions);
    glBufferData(GL_SHADER_STORAGE_BUFFER, numParticles * sizeof(glm::vec2), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &m_SSBO_SortTarget_PredictedPositions);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO_SortTarget_PredictedPositions);
    glBufferData(GL_SHADER_STORAGE_BUFFER, numParticles * sizeof(glm::vec2), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &m_SSBO_SortTarget_Velocities);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO_SortTarget_Velocities);
    glBufferData(GL_SHADER_STORAGE_BUFFER, numParticles * sizeof(glm::vec2), nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

FluidSimulation::~FluidSimulation() {
    glDeleteProgram(m_ComputeProgram);
    glDeleteBuffers(1, &m_SSBO_Positions);
    glDeleteBuffers(1, &m_SSBO_PredictedPositions);
    glDeleteBuffers(1, &m_SSBO_Velocities);
    glDeleteBuffers(1, &m_SSBO_Densities);
    glDeleteBuffers(1, &m_SSBO_SpatialKeys);
    glDeleteBuffers(1, &m_SSBO_SpatialOffsets);
    glDeleteBuffers(1, &m_SSBO_SortedIndices);
    glDeleteBuffers(1, &m_SSBO_SortTarget_Positions);
    glDeleteBuffers(1, &m_SSBO_SortTarget_PredictedPositions);
    glDeleteBuffers(1, &m_SSBO_SortTarget_Velocities);
}

bool FluidSimulation::CompileComputeShader(const std::string& shaderCode) {
    const char* src = shaderCode.c_str();
    unsigned int shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cout << "COMPUTE SHADER COMPILE FAIL:\n" << infoLog << std::endl;
        return false;
    }

    m_ComputeProgram = glCreateProgram();
    glAttachShader(m_ComputeProgram, shader);
    glLinkProgram(m_ComputeProgram);
    glDeleteShader(shader);
    return true;
}

void FluidSimulation::UpdateSpatialOffsets() {
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO_SpatialKeys);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_NumParticles * sizeof(GLuint), m_CpuKeys.data());

    std::vector<unsigned int> keyCounts(m_NumParticles, 0);
    for (unsigned int i = 0; i < m_NumParticles; i++) {
        unsigned int key = m_CpuKeys[i] % m_NumParticles;
        keyCounts[key]++;
    }

    std::vector<unsigned int> bucketOffsets(m_NumParticles, 0);
    unsigned int runningSum = 0;
    for (unsigned int i = 0; i < m_NumParticles; i++) {
        bucketOffsets[i] = runningSum;
        runningSum += keyCounts[i];
    }

    for (unsigned int i = 0; i < m_NumParticles; i++) {
        unsigned int key = m_CpuKeys[i] % m_NumParticles;
        unsigned int sortedPos = bucketOffsets[key]++;
        m_CpuIndices[sortedPos] = i;
    }

    std::fill(m_CpuOffsets.begin(), m_CpuOffsets.end(), m_NumParticles);

    std::vector<GLuint> sortedKeys(m_NumParticles);
    for (unsigned int i = 0; i < m_NumParticles; i++) {
        sortedKeys[i] = m_CpuKeys[m_CpuIndices[i]];
        if (i == 0 || sortedKeys[i] != sortedKeys[i - 1]) {
            m_CpuOffsets[sortedKeys[i]] = i;
        }
    }

    unsigned int lastValid = m_NumParticles;
    for (int i = (int)m_NumParticles - 1; i >= 0; i--) {
        if (m_CpuOffsets[i] == m_NumParticles) {
            m_CpuOffsets[i] = lastValid;
        }
        else {
            lastValid = m_CpuOffsets[i];
        }
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO_SpatialOffsets);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_NumParticles * sizeof(GLuint), m_CpuOffsets.data());

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO_SpatialKeys);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_NumParticles * sizeof(GLuint), sortedKeys.data());

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO_SortedIndices);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_NumParticles * sizeof(GLuint), m_CpuIndices.data());
}

void FluidSimulation::Step(float dt) {
    if (m_ComputeProgram == 0) return;

    for (size_t i = 0; i < m_Blocks.size(); ++i) {
        if (m_Blocks[i].isStatic == 0) {
            m_Blocks[i].velocity.y += m_Settings.gravity * dt;
            m_Blocks[i].position += m_Blocks[i].velocity * dt;
            m_Blocks[i].angle += m_Blocks[i].angularVelocity * dt;

            float cosAngle = std::abs(std::cos(m_Blocks[i].angle));
            float sinAngle = std::abs(std::sin(m_Blocks[i].angle));

            float extentX = cosAngle * m_Blocks[i].halfSize.x + sinAngle * m_Blocks[i].halfSize.y;
            float extentY = sinAngle * m_Blocks[i].halfSize.x + cosAngle * m_Blocks[i].halfSize.y;

            float halfBoundX = m_Settings.boundsSize.x / 2.0f;
            float halfBoundY = m_Settings.boundsSize.y / 2.0f;

            if (m_Blocks[i].position.y - extentY < -halfBoundY) {
                m_Blocks[i].position.y = -halfBoundY + extentY;
                m_Blocks[i].velocity.y *= -0.3f;
                m_Blocks[i].velocity.x *= 0.95f;
                m_Blocks[i].angularVelocity *= 0.95f;
            }
            else if (m_Blocks[i].position.y + extentY > halfBoundY) {
                m_Blocks[i].position.y = halfBoundY - extentY;
                m_Blocks[i].velocity.y *= -0.3f;
            }

            if (m_Blocks[i].position.x - extentX < -halfBoundX) {
                m_Blocks[i].position.x = -halfBoundX + extentX;
                m_Blocks[i].velocity.x *= -0.3f;
                m_Blocks[i].velocity.y *= 0.95f;
                m_Blocks[i].angularVelocity *= 0.95f;
            }
            else if (m_Blocks[i].position.x + extentX > halfBoundX) {
                m_Blocks[i].position.x = halfBoundX - extentX;
                m_Blocks[i].velocity.x *= -0.3f;
                m_Blocks[i].velocity.y *= 0.95f;
                m_Blocks[i].angularVelocity *= 0.95f;
            }

            for (size_t j = 0; j < m_Blocks.size(); ++j) {
                if (i == j) continue;

                if (m_Blocks[j].isStatic == 0 && j < i) continue;

                PhysicalBlock& A = m_Blocks[i];
                PhysicalBlock& B = m_Blocks[j];

                glm::vec2 T = B.position - A.position;

                float cA = std::cos(A.angle), sA = std::sin(A.angle);
                glm::vec2 Ax(cA, sA), Ay(-sA, cA);

                float cB = std::cos(B.angle), sB = std::sin(B.angle);
                glm::vec2 Bx(cB, sB), By(-sB, cB);

                glm::vec2 axes[4] = { Ax, Ay, Bx, By };
                float minOverlap = 999999.0f;
                glm::vec2 minAxis(0.0f);
                bool isColliding = true;

                for (int ax = 0; ax < 4; ++ax) {
                    glm::vec2 axis = axes[ax];

                    float rA = A.halfSize.x * std::abs(glm::dot(Ax, axis)) + A.halfSize.y * std::abs(glm::dot(Ay, axis));
                    float rB = B.halfSize.x * std::abs(glm::dot(Bx, axis)) + B.halfSize.y * std::abs(glm::dot(By, axis));

                    float dist = std::abs(glm::dot(T, axis));
                    float overlap = rA + rB - dist;

                    if (overlap <= 0.0f) {
                        isColliding = false;
                        break;
                    }

                    if (overlap < minOverlap) {
                        minOverlap = overlap;
                        minAxis = axis;
                    }
                }

                if (isColliding) {
                    if (glm::dot(T, minAxis) < 0.0f) {
                        minAxis = -minAxis;
                    }

                    if (B.isStatic == 1) {
                        A.position -= minAxis * minOverlap;

                        float relativeVel = glm::dot(A.velocity, minAxis);
                        if (relativeVel > 0.0f) {
                            A.velocity -= minAxis * relativeVel * 1.3f;
                        }
                    }
                    else {
                        A.position -= minAxis * (minOverlap * 0.5f);
                        B.position += minAxis * (minOverlap * 0.5f);

                        glm::vec2 relVel = A.velocity - B.velocity;
                        float velAlongNormal = glm::dot(relVel, minAxis);
                        if (velAlongNormal > 0.0f) {
                            float bounce = velAlongNormal * 0.65f;
                            A.velocity -= minAxis * bounce;
                            B.velocity += minAxis * bounce;
                        }
                    }
                }
            }
        }
    }

    glUseProgram(m_ComputeProgram);

    glUniform1ui(glGetUniformLocation(m_ComputeProgram, "numParticles"), m_NumParticles);
    glUniform1f(glGetUniformLocation(m_ComputeProgram, "gravity"), m_Settings.gravity);
    glUniform1f(glGetUniformLocation(m_ComputeProgram, "deltaTime"), dt);
    glUniform1f(glGetUniformLocation(m_ComputeProgram, "collisionDamping"), m_Settings.collisionDamping);
    glUniform1f(glGetUniformLocation(m_ComputeProgram, "smoothingRadius"), m_Settings.smoothingRadius);
    glUniform1f(glGetUniformLocation(m_ComputeProgram, "targetDensity"), m_Settings.targetDensity);
    glUniform1f(glGetUniformLocation(m_ComputeProgram, "pressureMultiplier"), m_Settings.pressureMultiplier);
    glUniform1f(glGetUniformLocation(m_ComputeProgram, "nearPressureMultiplier"), m_Settings.nearPressureMultiplier);
    glUniform1f(glGetUniformLocation(m_ComputeProgram, "viscosityStrength"), m_Settings.viscosityStrength);
    glUniform2f(glGetUniformLocation(m_ComputeProgram, "boundsSize"), m_Settings.boundsSize.x, m_Settings.boundsSize.y);

    float r = m_Settings.smoothingRadius;
    glUniform1f(glGetUniformLocation(m_ComputeProgram, "Poly6ScalingFactor"), 4.0f / (3.14159265f * pow(r, 8.0f)));
    glUniform1f(glGetUniformLocation(m_ComputeProgram, "SpikyPow3ScalingFactor"), 10.0f / (3.14159265f * pow(r, 5.0f)));
    glUniform1f(glGetUniformLocation(m_ComputeProgram, "SpikyPow2ScalingFactor"), 6.0f / (3.14159265f * pow(r, 4.0f)));
    glUniform1f(glGetUniformLocation(m_ComputeProgram, "SpikyPow3DerivativeScalingFactor"), 30.0f / (3.14159265f * pow(r, 5.0f)));
    glUniform1f(glGetUniformLocation(m_ComputeProgram, "SpikyPow2DerivativeScalingFactor"), 12.0f / (3.14159265f * pow(r, 4.0f)));

    glUniform1i(glGetUniformLocation(m_ComputeProgram, "u_BlockCount"), static_cast<int>(m_Blocks.size()));

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_SSBO_Positions);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_SSBO_PredictedPositions);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_SSBO_Velocities);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_SSBO_Densities);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_SSBO_SpatialKeys);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, m_SSBO_SpatialOffsets);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, m_SSBO_SortedIndices);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, m_SSBO_SortTarget_Positions);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, m_SSBO_SortTarget_PredictedPositions);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, m_SSBO_SortTarget_Velocities);

    GLuint blockSSBO = 0;
    if (!m_Blocks.empty()) {
        glGenBuffers(1, &blockSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, blockSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, m_Blocks.size() * sizeof(PhysicalBlock), m_Blocks.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, blockSSBO);
    }

    int numGroups = (m_NumParticles + 63) / 64;
    GLint indexLocation = glGetUniformLocation(m_ComputeProgram, "u_KernelIndex");

    glUniform2f(glGetUniformLocation(m_ComputeProgram, "u_InteractionPoint"), m_InteractionPoint.x, m_InteractionPoint.y);
    glUniform1f(glGetUniformLocation(m_ComputeProgram, "u_InteractionStrength"), m_InteractionStrength);
    glUniform1f(glGetUniformLocation(m_ComputeProgram, "u_InteractionRadius"), m_InteractionRadius);


    // Pass 0: External Forces
    glUniform1i(indexLocation, 0);
    glDispatchCompute(numGroups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Pass 1: Hash Extraction
    glUniform1i(indexLocation, 1);
    glDispatchCompute(numGroups, 1, 1);

    UpdateSpatialOffsets();

    // Pass 2: Reorder Arrays
    glUniform1i(indexLocation, 2);
    glDispatchCompute(numGroups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Pass 3: Reorder Copyback
    glUniform1i(indexLocation, 3);
    glDispatchCompute(numGroups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Pass 4: Densities
    glUniform1i(indexLocation, 4);
    glDispatchCompute(numGroups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Pass 5: Pressures
    glUniform1i(indexLocation, 5);
    glDispatchCompute(numGroups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Pass 6: Viscosity
    glUniform1i(indexLocation, 6);
    glDispatchCompute(numGroups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Pass 7: Final Position Update
    glUniform1i(indexLocation, 7);
    glDispatchCompute(numGroups, 1, 1);


    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

    if (blockSSBO != 0) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, blockSSBO);
        PhysicalBlock* gpuBlocks = (PhysicalBlock*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);

        if (gpuBlocks) {
            for (size_t i = 0; i < m_Blocks.size(); i++) {
                if (m_Blocks[i].isStatic == 0) {
                    float forceX = (float)gpuBlocks[i].forceX / 10000.0f;
                    float forceY = (float)gpuBlocks[i].forceY / 10000.0f;
                    float torque = (float)gpuBlocks[i].torqueAcc / 10000.0f;

                    m_Blocks[i].velocity.x += (forceX / m_Blocks[i].mass) * 20.0f;
                    m_Blocks[i].velocity.y += (forceY / m_Blocks[i].mass) * 20.0f;

                    m_Blocks[i].angularVelocity += (torque / m_Blocks[i].momentOfInertia) * 20.0f;

                    m_Blocks[i].velocity *= 0.99f;
                    m_Blocks[i].angularVelocity *= 0.98f;
                }

                m_Blocks[i].forceX = 0;
                m_Blocks[i].forceY = 0;
                m_Blocks[i].torqueAcc = 0;
            }
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }

        glDeleteBuffers(1, &blockSSBO);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}