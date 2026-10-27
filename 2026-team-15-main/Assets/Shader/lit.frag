#version 300 es
precision mediump float;
in vec3 vNormal;
uniform vec3 uColor;
uniform vec3 uTint;
uniform float uTintAmount;
out vec4 outColor;
void main()
{
    vec3 n = normalize(vNormal);
    float diffuse = max(dot(n, normalize(vec3(0.4, 1.0, 0.6))), 0.0);
    vec3 lit = uColor * (0.3 + 0.7 * diffuse);
    outColor = vec4(mix(lit, uTint, uTintAmount), 1.0);
}
