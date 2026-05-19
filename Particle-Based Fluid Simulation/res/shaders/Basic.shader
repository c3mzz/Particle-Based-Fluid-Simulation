#shader vertex
#version 430 core

layout(location = 0) in vec2 position; 
layout(location = 1) in vec2 instancePosition;
layout(location = 2) in vec2 instanceVelocity;

uniform mat4 u_MVP;
uniform int u_VertexMode; // 0 = Particles, 1 = Mouse Ring, 2 = UI Blocks
uniform vec2 u_MousePos; 
uniform float u_ParticleRadius;

uniform int u_OverrideColor;
uniform vec4 u_CustomColor;

out vec4 v_Color;

void main() {
    vec2 finalPos;
    
    if (u_VertexMode == 0) {
        // --- STANDARD PARTICLES ---
        finalPos = instancePosition + (position * u_ParticleRadius);
        
        // Dynamic Velocity Color Grading (Blue -> Yellow -> Orange -> Red)
        float speed = length(instanceVelocity);
        float t = clamp(speed / 15.0, 0.0, 1.0);
        
        vec3 color;
        if (t < 0.33) {
            color = mix(vec3(0.0, 0.3, 1.0), vec3(1.0, 1.0, 0.0), t / 0.33);
        } else if (t < 0.66) {
            color = mix(vec3(1.0, 1.0, 0.0), vec3(1.0, 0.5, 0.0), (t - 0.33) / 0.33);
        } else {
            color = mix(vec3(1.0, 0.5, 0.0), vec3(1.0, 0.0, 0.0), (t - 0.66) / 0.34);
        }
        
        // Apply override if selected, otherwise use physical speed color
        v_Color = (u_OverrideColor == 1) ? u_CustomColor : vec4(color, 1.0);
        
    } 
    else if (u_VertexMode == 1) {
        // --- INTERACTIVE MOUSE RING ---
        finalPos = u_MousePos + (position * u_ParticleRadius);
        v_Color = u_CustomColor;
    } 
    else {
        // --- UI BLOCKS ---
        finalPos = position;
        v_Color = u_CustomColor;
    }
    
    gl_Position = u_MVP * vec4(finalPos, 0.0, 1.0);
}

#shader fragment
#version 430 core

in vec4 v_Color;
out vec4 FragColor;

void main() {
    // Colors are entirely calculated on the vertex stage for performance
    FragColor = v_Color;
}