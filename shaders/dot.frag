#version 450

layout(location = 0) out vec4 color;
layout(location = 0) in vec2 vertIn;

void main()
{
	if (dot(vertIn, vertIn) <= 1)
		color = vec4(1.0, 0.0, 1.0, 0.0);
	else
		color = vec4(0,0,0,0);
}
