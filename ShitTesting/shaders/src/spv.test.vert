#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(std140, set = 0, binding = 0) uniform in_params
{
	mat4 ViewProjMatrix;
	mat4 worldMatrix;
	vec4 materialProp; //x = roughness, y = k, z = F0
	vec4 extra;
}param;

layout(set = 0, binding = 2) uniform sampler2D HeightMap;

layout(location=0) in vec3 position;
layout(location=1) in vec2 in_uv;
layout(location=2) in vec3 in_normal;

layout(location=0) out vec4 normal;
layout(location=1) out vec4 worldPos;
layout(location=2) out vec4 material;
layout(location=3) out vec2 uv;

vec3 ComputeNormal(vec3 o, vec3 p1, vec3 p2)
{
	vec3 e1 = p1 - o;
	vec3 e2 = p2 - o;
	vec3 normal = vec3(e1.y * e2.z, e1.z * e2.x, e1.x * e2.y) - vec3(e1.z * e2.y, e1.x * e2.z, e1.y * e2.x);
	
	return normalize(normal);

}

/*
	To get the normal of a vertex, first have to compute the normal of the adjancent triangles
	
	the neighbours vertexes are numbered as follows:
		0
		|
	  3-P-1
		|
		2
		
	We will get the normals of the triangles that contains pair of vertexes : P-0, P-1, P-2, P-3
*/

vec3 ComputeVertexNormal(vec3 P)
{
	vec3 modelSpaceDisplacement = vec3(param.extra.x, 1.0f, param.extra.y);// how far away on the grid the vertexes are;
	vec2 delta = vec2(param.extra.zw); //derivatives of the height image that corespond with the model space displacement
	
	float h0 = texture(HeightMap, in_uv + vec2(0.0f, -delta.y)).r;
	float h1 = texture(HeightMap, in_uv + vec2(delta.x, 0.0f)).r;
	float h2 = texture(HeightMap, in_uv + vec2(0.0f, delta.y)).r;
	float h3 = texture(HeightMap, in_uv + vec2(-delta.x, 0.0f)).r;
	
	vec3 P0 = position + vec3(0.0f, h0, -1.0f) * modelSpaceDisplacement;
	vec3 P1 = position + vec3(1.0f, h1, 0.0f) * modelSpaceDisplacement;
	vec3 P2 = position + vec3(0.0f, h2, 1.0f) * modelSpaceDisplacement;
	vec3 P3 = position + vec3(-1.0f, h3, 0.0f) * modelSpaceDisplacement;
	
	vec3 normal = ComputeNormal(P, P1, P0);
	normal += ComputeNormal(P, P2, P1);
	normal += ComputeNormal(P, P3, P2);
	normal += ComputeNormal(P, P0, P3);
	
	return normalize(normal / 4.0f); //maybe dont use normalize here
}

void main()
{
	uv = in_uv;
	float maxHeight = 2.f;
	float height = texture(HeightMap, uv).r * maxHeight;
	vec3 newPos = position + vec3(0, height, 0);
	
	material = vec4(param.materialProp.xyz, height);
	vec3 transNormal = inverse(transpose(mat3(param.worldMatrix))) * ComputeVertexNormal(newPos);
	normal = vec4(normalize(transNormal), 0.0f);
	worldPos = (param.worldMatrix * vec4(newPos, 1));

    gl_Position = param.ViewProjMatrix * worldPos;
}
