#version 450

layout (location = 0) out vec2 vertOut;
layout (location = 1) out float vertScale;

struct Particle {
	vec3 position;
	float mass; /* UNUSED */
	vec3 velocity; /* UNUSED */
};

layout (binding = 0, std140) readonly buffer instanceSSBO {
	Particle particles[];
};

layout (binding = 0, std140) uniform camInfo {
	mat4 combined;
};

void main()
{
	vec2 vert;
	const float scale = 100;
	switch (gl_VertexIndex) {
		case 0: vert = vec2(0, 2) / scale; break;
		case 1: vert = vec2(1.7321, -1) / scale; break;
		case 2: vert = vec2(-1.7321, -1) / scale; break;
	}

	gl_Position = combined * vec4(vec3(vert, 0.0) + particles[gl_InstanceIndex].position, 1.0);
	vertOut = vert;
	vertScale = scale;
}
