#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) in vec3 position;
layout(location=5) in vec4 in_color; //not used

layout(set=0, binding=0) uniform CommonParams
{
	vec4 ScreenSize; //in pixels
	mat4 ProjViewMatrix;
}commons;

layout(location=0) out vec4 color;

void main()
{
	gl_Position = commons.ProjViewMatrix * vec4(position, 1.0f);
	color = in_color;
}