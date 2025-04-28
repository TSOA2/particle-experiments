#version 450

layout (location = 0) out vec2 vertOut;

layout (binding = 0, std140) uniform camInfo {
	mat4 combined;
};

void main()
{
	vec2 vert;
	switch (gl_VertexIndex) {
		case 0: vert = vec2(0, 2); break;
		case 1: vert = vec2(1.7321, -1); break;
		case 2: vert = vec2(-1.7321, -1); break;
	}

	gl_Position = combined * vec4(vert, 0.0, 1.0);
	vertOut = vert;
}
