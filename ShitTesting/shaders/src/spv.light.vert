#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

vec2 positions[] = {vec2(-1, -1),
	vec2(-1, 1),
	vec2(1, 1),
	vec2(1, -1),

};

vec2 uvs[] = {
	vec2(0.0f, 1.0f),
	vec2(0.0f, 0.0f),
	vec2(1.0f, 0.0f),
	vec2(1.0f, 1.0f),
};

layout(location=0) out vec2 uv;

void main()
{
	uv = uvs[gl_VertexIndex];
	gl_Position = vec4(positions[gl_VertexIndex], .0f, 1.0f) * vec4(1, -1, 1, 1);
}