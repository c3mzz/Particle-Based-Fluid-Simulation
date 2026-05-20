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
    
    // Pre-allocate optimization buffers
    m_KeyCounts.resize(numParticles, 0);
    m_BucketOffsets.resize(numParticles, 0);
    m_SortedKeys.resize(numParticles, 0);

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
    if (m_BlockSSBO != 0) {
        glDeleteBuffers(1, &m_BlockSSBO);
    }
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

    std::fill(m_KeyCounts.begin(), m_KeyCounts.end(), 0);
    for (unsigned int i = 0; i < m_NumParticles; i++) {
        unsigned int key = m_CpuKeys[i] % m_NumParticles;
        m_KeyCounts[key]++;
    }

    unsigned int runningSum = 0;
    for (unsigned int i = 0; i < m_NumParticles; i++) {
        m_BucketOffsets[i] = runningSum;
        runningSum += m_KeyCounts[i];
    }

    for (unsigned int i = 0; i < m_NumParticles; i++) {
        unsigned int key = m_CpuKeys[i] % m_NumParticles;
        unsigned int sortedPos = m_BucketOffsets[key]++;
        m_CpuIndices[sortedPos] = i;
    }

    std::fill(m_CpuOffsets.begin(), m_CpuOffsets.end(), m_NumParticles);

    for (unsigned int i = 0; i < m_NumParticles; i++) {
        m_SortedKeys[i] = m_CpuKeys[m_CpuIndices[i]];
        if (i == 0 || m_SortedKeys[i] != m_SortedKeys[i - 1]) {
            m_CpuOffsets[m_SortedKeys[i]] = i;
        }
    }

    unsigned int lastValid = m_NumParticles;
    for (int i = (int)m_NumParticles - 1; i >= 0; i--) {
        if (m_CpuOffsets[i] == m_NumParticles) {
            m_CpuOffsets[i] = lastValid;
        } else {
            lastValid = m_CpuOffsets[i];
        }
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO_SpatialOffsets);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_NumParticles * sizeof(GLuint), m_CpuOffsets.data());

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO_SpatialKeys);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_NumParticles * sizeof(GLuint), m_SortedKeys.data());

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO_SortedIndices);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_NumParticles * sizeof(GLuint), m_CpuIndices.data());
}

static glm::vec2 GetContactPoint(const PhysicalBlock& A, const PhysicalBlock& B) {
    auto getCorners = [](const PhysicalBlock& blk, glm::vec2* corners) {
        float c = std::cos(blk.angle), s = std::sin(blk.angle);
        glm::vec2 x(c, s), y(-s, c);
        corners[0] = blk.position + x * blk.halfSize.x + y * blk.halfSize.y;
        corners[1] = blk.position + x * blk.halfSize.x - y * blk.halfSize.y;
        corners[2] = blk.position - x * blk.halfSize.x + y * blk.halfSize.y;
        corners[3] = blk.position - x * blk.halfSize.x - y * blk.halfSize.y;
    };
    
    glm::vec2 cA[4], cB[4];
    getCorners(A, cA);
    getCorners(B, cB);
    
    glm::vec2 manifoldSum(0.0f);
    int count = 0;

    auto evalCorners = [&](glm::vec2* corners, const PhysicalBlock& target) {
        float c = std::cos(target.angle), s = std::sin(target.angle);
        glm::vec2 targetX(c, s), targetY(-s, c);
        for (int i = 0; i < 4; i++) {
            glm::vec2 localP = corners[i] - target.position;
            float dx = std::abs(glm::dot(localP, targetX)) - target.halfSize.x;
            float dy = std::abs(glm::dot(localP, targetY)) - target.halfSize.y;
            
            if (dx <= 0.01f && dy <= 0.01f) {
                manifoldSum += corners[i];
                count++;
            }
        }
    };
    
    evalCorners(cA, B);
    evalCorners(cB, A);

    if (count > 0) {
        return manifoldSum / static_cast<float>(count);
    }
    
    glm::vec2 p = A.position - B.position;
    float c = std::cos(-B.angle), s = std::sin(-B.angle);
    glm::vec2 pRot(p.x * c - p.y * s, p.x * s + p.y * c);
    pRot.x = std::max(-B.halfSize.x, std::min(B.halfSize.x, pRot.x));
    pRot.y = std::max(-B.halfSize.y, std::min(B.halfSize.y, pRot.y));
    float cRotB = std::cos(B.angle), sRotB = std::sin(B.angle);
    return glm::vec2(pRot.x * cRotB - pRot.y * sRotB, pRot.x * sRotB + pRot.y * cRotB) + B.position;
}

void FluidSimulation::UpdateBlocks(float dt) {
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
            } else if (m_Blocks[i].position.y + extentY > halfBoundY) {
                m_Blocks[i].position.y = halfBoundY - extentY;
                m_Blocks[i].velocity.y *= -0.3f;
            }

            if (m_Blocks[i].position.x - extentX < -halfBoundX) {
                m_Blocks[i].position.x = -halfBoundX + extentX;
                m_Blocks[i].velocity.x *= -0.3f;
                m_Blocks[i].velocity.y *= 0.95f;
                m_Blocks[i].angularVelocity *= 0.95f;
            } else if (m_Blocks[i].position.x + extentX > halfBoundX) {
                m_Blocks[i].position.x = halfBoundX - extentX;
                m_Blocks[i].velocity.x *= -0.3f;
                m_Blocks[i].velocity.y *= 0.95f;
                m_Blocks[i].angularVelocity *= 0.95f;
            }
        }
    }
}

void FluidSimulation::HandleBlockCollisions() {
    for (size_t i = 0; i < m_Blocks.size(); ++i) {
        for (size_t j = 0; j < m_Blocks.size(); ++j) {
            if (i == j) continue;
            if (m_Blocks[j].isStatic == 0 && j < i) continue;

            PhysicalBlock& A = m_Blocks[i];
            PhysicalBlock& B = m_Blocks[j];

            if (A.isStatic && B.isStatic) continue;

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
                
                // Bias vertical axes to prevent "edge catching" (adhesion) at corner slides
                float bias = (axis == Ay || axis == By) ? 0.95f : 1.0f;
                if (overlap * bias < minOverlap) {
                    minOverlap = overlap;
                    minAxis = axis;
                }
            }

            if (isColliding) {
                if (glm::dot(T, minAxis) < 0.0f) {
                    minAxis = -minAxis; 
                }

                glm::vec2 contactP = GetContactPoint(A, B);
                glm::vec2 rA = contactP - A.position;
                glm::vec2 rB = contactP - B.position;

                auto cross2D = [](glm::vec2 v) { return glm::vec2(-v.y, v.x); };
                auto cross2DScalar = [](glm::vec2 a, glm::vec2 b) { return a.x * b.y - a.y * b.x; };

                const float slop = 0.02f;
                const float percent = 0.12f; 
                float correctionMagnitude = std::max(minOverlap - slop, 0.0f) * percent;
                glm::vec2 correction = minAxis * correctionMagnitude;

                if (B.isStatic == 1) {
                    A.position -= correction;
                } else if (A.isStatic == 1) {
                    B.position += correction;
                } else {
                    A.position -= correction * 0.5f;
                    B.position += correction * 0.5f;
                }

                glm::vec2 vA = A.velocity + A.angularVelocity * cross2D(rA);
                glm::vec2 vB = B.velocity + B.angularVelocity * cross2D(rB);
                glm::vec2 vRel = vA - vB;

                float velAlongNormal = glm::dot(vRel, minAxis);
                if (velAlongNormal < 0.0f) continue;

                float e = 0.05f; // Very low restitution for stacking stability
                float jImpulse = -(1.0f + e) * velAlongNormal;

                float invMassA = A.isStatic ? 0.0f : (1.0f / A.mass);
                float invMassB = B.isStatic ? 0.0f : (1.0f / B.mass);
                float invInertiaA = A.isStatic ? 0.0f : (1.0f / A.momentOfInertia);
                float invInertiaB = B.isStatic ? 0.0f : (1.0f / B.momentOfInertia);

                float rACrossN = cross2DScalar(rA, minAxis);
                float rBCrossN = cross2DScalar(rB, minAxis);

                float denom = invMassA + invMassB + 
                              (rACrossN * rACrossN) * invInertiaA + 
                              (rBCrossN * rBCrossN) * invInertiaB;

                if (denom > 0.0001f) {
                    jImpulse /= denom;
                    glm::vec2 impulse = jImpulse * minAxis;

                    // Friction Impulse (Coulomb Friction Model)
                    glm::vec2 tangent = cross2D(minAxis);
                    float velAlongTangent = glm::dot(vRel, tangent);
                    float jTangent = -velAlongTangent / denom;
                    float maxFriction = std::abs(jImpulse) * m_Settings.blockFriction;
                    jTangent = std::max(-maxFriction, std::min(maxFriction, jTangent));
                    glm::vec2 frictionImpulse = tangent * jTangent;

                    glm::vec2 totalImpulse = impulse + frictionImpulse;

                    if (!A.isStatic) {
                        A.velocity += totalImpulse * invMassA;
                        A.angularVelocity += cross2DScalar(rA, totalImpulse) * invInertiaA; 
                    }
                    if (!B.isStatic) {
                        B.velocity -= totalImpulse * invMassB;
                        B.angularVelocity -= cross2DScalar(rB, totalImpulse) * invInertiaB;
                    }
                }
            }
        }
    }
}

void FluidSimulation::DispatchComputeShaders(float dt) {
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

    if (m_BlockSSBO == 0) {
        glGenBuffers(1, &m_BlockSSBO);
    }
    
    if (!m_Blocks.empty()) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BlockSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, m_Blocks.size() * sizeof(PhysicalBlock), m_Blocks.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, m_BlockSSBO);
    }

    int numGroups = (m_NumParticles + 63) / 64;
    GLint indexLocation = glGetUniformLocation(m_ComputeProgram, "u_KernelIndex");

    glUniform2f(glGetUniformLocation(m_ComputeProgram, "u_InteractionPoint"), m_InteractionPoint.x, m_InteractionPoint.y);
    glUniform1f(glGetUniformLocation(m_ComputeProgram, "u_InteractionStrength"), m_InteractionStrength);
    glUniform1f(glGetUniformLocation(m_ComputeProgram, "u_InteractionRadius"), m_InteractionRadius);

    auto dispatch = [&](int pass) {
        glUniform1i(indexLocation, pass);
        glDispatchCompute(numGroups, 1, 1);
    };

    dispatch(0); 
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    dispatch(1); 
    UpdateSpatialOffsets();
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    dispatch(2); 
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    dispatch(3); 
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    dispatch(4); 
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    dispatch(5); 
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    dispatch(6); 
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    dispatch(7); 
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
}

void FluidSimulation::ReadbackBlockForces() {
    if (m_Blocks.empty()) return;
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BlockSSBO);
    PhysicalBlock* gpuBlocks = (PhysicalBlock*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);

    if (gpuBlocks) {
        for (size_t i = 0; i < m_Blocks.size(); i++) {
            if (m_Blocks[i].isStatic == 0) {
                float forceX = (float)gpuBlocks[i].forceX / 10000.0f;
                float forceY = (float)gpuBlocks[i].forceY / 10000.0f;
                float torque = (float)gpuBlocks[i].torqueAcc / 10000.0f;

                float inertia = std::max(m_Blocks[i].momentOfInertia, 0.001f);
                float mass = std::max(m_Blocks[i].mass, 0.001f);

                m_Blocks[i].velocity.x += (forceX / mass) * m_Settings.particleToBlockImpulse;
                m_Blocks[i].velocity.y += (forceY / mass) * m_Settings.particleToBlockImpulse;

                m_Blocks[i].angularVelocity += (torque / inertia) * m_Settings.particleToBlockImpulse;

                m_Blocks[i].velocity *= 0.99f;
                m_Blocks[i].angularVelocity *= 0.985f;
            }

            m_Blocks[i].forceX = 0;
            m_Blocks[i].forceY = 0;
            m_Blocks[i].torqueAcc = 0;
        }
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
}

void FluidSimulation::Step(float dt) {
    if (m_ComputeProgram == 0) return;
    
    UpdateBlocks(dt);
    HandleBlockCollisions();
    DispatchComputeShaders(dt);
    ReadbackBlockForces();
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}