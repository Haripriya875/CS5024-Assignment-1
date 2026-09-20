#version 330 core

in vec3 color;
out vec4 FragColor;

uniform vec3 overrideColor;
uniform bool useOverrideColor;

void main()
{
    FragColor=vec4(useOverrideColor ? overrideColor : color, 1.0);
}