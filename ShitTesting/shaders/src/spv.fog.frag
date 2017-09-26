#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) out vec4 fogColor;
layout(location=1) out vec4 debug;

layout(set=0,binding=0) uniform sampler2D Positions;
layout(set=0,binding=1) uniform FogParams
{
	mat4 ViewMatrix;
};

layout(location=0) in vec4 uv;

void main()
{
	
	vec4 position = texture(Positions, uv.st);
	vec3 viewPos = (ViewMatrix * position).xyz;
	float cameraDistance = length(viewPos);
	float fogDensity = 0.035f;
	float fogIntensity = 1.0f - exp( - pow(cameraDistance * fogDensity, 2.0f));
	fogColor = vec4(0.5f, 0.6f, 0.7f, fogIntensity);
	
	debug = vec4(viewPos, 1.0f);
}