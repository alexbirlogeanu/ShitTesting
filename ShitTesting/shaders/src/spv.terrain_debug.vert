#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(std140, set = 0, binding = 0) uniform in_params
{
	mat4 ViewProjMatrix;
	mat4 WorldMatrix;
	vec4 MaterialProp; //x = roughness, y = k, z = F0
	vec4 TesselationParams;
	vec4 PatchParams; //xy - number of cells that are in terrain texture patch, zw - total number of cells in a terrain grid
}param;

layout(location=0) in vec3 position;
layout(location=1) in vec2 in_uv;
layout(location=2) in vec3 in_normal;

layout(location=0) out vec4 normal;
layout(location=1) out vec4 material;
layout(location=2) out vec2 uv;
layout(location=3) out vec4 outWorldPos;
layout(location=4) out vec4 debug;

void main()
{
	uv = in_uv;
	material = vec4(param.MaterialProp.xyz, 0.0f);
	vec3 transNormal = inverse(transpose(mat3(param.WorldMatrix))) * in_normal;
	normal = vec4(normalize(transNormal), 0.0f);
	debug = vec4(0.0f);
	
	outWorldPos = param.WorldMatrix * vec4(position, 1.0f);
	gl_Position = param.ViewProjMatrix * outWorldPos;
}