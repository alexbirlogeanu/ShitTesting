#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) in vec3 position;
layout(location=1) in vec2 in_uv;
layout(location=2) in vec3 in_normal;
layout(location=3) in vec3 in_bitangent;
layout(location=4) in vec3 in_tangent;

struct NodeParams
{
	mat4 ModelMatrix;
	vec4 MaterialProperties; //x = roughness, y = k, z = F0
};

layout(set=0, binding=0) buffer BatchParams
{
	NodeParams data[];
};

layout(push_constant) uniform PushConstants
{
	mat4 ProjViewMatrix;
	vec4 ViewPos;
};

struct VertexOut
{
	vec4 MaterialProps;
	uvec4 TextureIndexes;
};

//outs
layout(location=0) out vec4 normal;
layout(location=1) out vec4 worldPos;
layout(location=2) out vec2 uv;
layout(location=3) out VertexOut Out;

void main()
{
	uv = in_uv;
	NodeParams currentNode = data[gl_DrawID];
	
	vec3 transNormal = inverse(transpose(mat3(currentNode.ModelMatrix))) * in_normal;
	normal = vec4(normalize(transNormal), 0.0f);
	worldPos = (currentNode.ModelMatrix * vec4(position, 1));
	gl_Position = ProjViewMatrix * worldPos;
	
	Out.MaterialProps = currentNode.MaterialProperties;
	Out.TextureIndexes = uvec4(gl_DrawID);
}