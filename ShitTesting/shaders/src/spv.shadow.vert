#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

struct BatchCommons
{
	mat4 ModelMatrix;
};


layout(std140, set = 0, binding = 0) buffer in_params
{
	BatchCommons commons[];
};

layout(location=0) in vec3 position;
layout(location=1) in vec2 in_uv;
layout(location=2) in vec3 in_normal;

layout(push_constant) uniform PushConstants
{
	mat4 ProjViewMatrix;
	mat4 ShadowProjViewMatrix;
	vec4 ViewPos;
};


void main()
{
    gl_Position = ShadowProjViewMatrix * commons[gl_DrawID].ModelMatrix * vec4(position, 1.0f);
}