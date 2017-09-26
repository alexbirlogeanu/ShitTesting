#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) in vec3 position;
layout(location=1) in vec2 in_uv;
layout(location=2) in vec3 in_normal; //not used

layout(location=0) out vec4 uv;

void main()
{
	uv = vec4(in_uv, 0.0f, 0.0f);
	gl_Position = vec4(position.x, -position.y, 1.0f, 1.0f);
}