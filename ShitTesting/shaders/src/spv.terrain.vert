#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(std140, set = 0, binding = 0) uniform in_params
{
	mat4 ViewProjMatrix;
	mat4 worldMatrix;
	vec4 materialProp; //x = roughness, y = k, z = F0
	vec4 viewPos;
}param;

layout(set = 0, binding = 2) uniform sampler2D HeightMap;

layout(location=0) in vec3 position;
layout(location=1) in vec2 in_uv;
layout(location=2) in vec3 in_normal;

layout(location=0) out vec4 material;
layout(location=1) out vec2 uv;

void main()
{	
	uv = in_uv;
	material = param.materialProp;
	vec3 transNormal = inverse(transpose(mat3(param.worldMatrix))) * in_normal;
    gl_Position = (param.worldMatrix * vec4(position, 1));
	//gl_Position.y = 0.0f;
}
