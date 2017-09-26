#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) in vec3 position;
layout(location=1) in vec2 in_uv;
layout(location=2) in vec3 in_normal; //not used

layout(set=0, binding=0) uniform CommonParams
{
	vec4 ScreenSize; //in pixels
	mat4 ProjViewMatrix;
}commons;

layout(set=1, binding=0) uniform SpecificParams
{
	vec4 ScreenPosition; //in pixels
}specifics;

layout(location=0) out vec4 uv;

void main()
{
	vec2 normalizedPos =  (specifics.ScreenPosition.xy + position.xy) / commons.ScreenSize.xy; //in 0 - 1
	//now we need to transform from 0-1 to -1 - 1
	vec2 ndcPos = normalizedPos * 2 - vec2(1.0, 1.0);
	uv = vec4(in_uv, 0.0f, 0.0f);
	gl_Position = vec4(ndcPos, 0.0f, 1.0f);
}