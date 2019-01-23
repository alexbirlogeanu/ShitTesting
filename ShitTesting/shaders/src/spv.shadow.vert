#version 450
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


void main()
{
    gl_Position = commons[gl_InstanceIndex].ModelMatrix * vec4(position, 1.0f);
}