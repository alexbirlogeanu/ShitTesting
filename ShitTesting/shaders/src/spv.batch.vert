#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) in vec3 position;
layout(location=1) in vec2 in_uv;
layout(location=2) in vec3 in_normal;
layout(location=3) in vec3 in_bitangent;
layout(location=4) in vec3 in_tangent;

struct BatchCommons
{
	mat4 ModelMatrix;
};

layout(set=0, binding=0) buffer BatchParams
{
	BatchCommons commonData[];
};

layout(push_constant) uniform PushConstants
{
	mat4 ProjViewMatrix;
	mat4 ShadowProjViewMatrix;
	vec4 ViewPos;
};


//outs
layout(location=0) out vec4 normal;
layout(location=1) out vec4 worldPos;
layout(location=2) out vec2 uv;
layout(location=3) out uint BatchIndex;

void main()
{
	uv = in_uv;
	BatchIndex = gl_InstanceIndex;
	BatchCommons currentNode = commonData[BatchIndex];
	
	vec3 transNormal = inverse(transpose(mat3(currentNode.ModelMatrix))) * in_normal;
	normal = vec4(normalize(transNormal), 0.0f);
	worldPos = (currentNode.ModelMatrix * vec4(position, 1));
	gl_Position = ProjViewMatrix * worldPos;
}