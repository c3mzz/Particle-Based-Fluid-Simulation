#shader vertex
#version 430 core

layout(location = 0) in vec2 a_GeometryVertex;  
layout(location = 1) in vec2 a_InstancePosition;
layout(location = 2) in vec2 a_InstanceVelocity;

uniform mat4 u_MVP;
uniform float u_ParticleRadius;
uniform bool u_OverrideColor;
uniform vec4 u_CustomColor;

out vec4 v_Color;

vec4 getSpeedColor(float speed) {
    float maxSpeed = 24.0;
    float t = clamp(speed / maxSpeed, 0.0, 1.0);
    
    vec4 c_blue   = vec4(0.0, 0.45, 1.0, 1.0);  
    vec4 c_yellow = vec4(1.0, 0.92, 0.1, 1.0);  
    vec4 c_orange = vec4(1.0, 0.45, 0.0, 1.0);  
    vec4 c_red    = vec4(0.95, 0.0, 0.0, 1.0);  
    
    if (t < 0.33) {
        return mix(c_blue, c_yellow, t / 0.33);
    } else if (t < 0.66) {
        return mix(c_yellow, c_orange, (t - 0.33) / 0.33);
    } else {
        return mix(c_orange, c_red, (t - 0.66) / 0.34);
    }
}

void main() {
    vec2 localSpacePos = a_GeometryVertex * u_ParticleRadius + a_InstancePosition;
    gl_Position = u_MVP * vec4(localSpacePos, 0.0, 1.0);

    if (u_OverrideColor) {
        v_Color = u_CustomColor;
    } else {
        float speed = length(a_InstanceVelocity);
        v_Color = getSpeedColor(speed);
    }
}

#shader fragment
#version 430 core

layout(location = 0) out vec4 color;
in vec4 v_Color;

void main() {
    color = v_Color;
}