#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(std140, set = 0, binding = 0) uniform in_params
{
	mat4 mvp;
	mat4 worldMatrix;
	vec4 materialProp; //x = roughness, y = k, z = F0
	vec4 viewPos;
}param;

layout(location=0) in vec3 position;
layout(location=1) in vec2 in_uv;
layout(location=2) in vec3 in_normal;
layout(location=3) in vec3 in_bitangent;
layout(location=4) in vec3 in_tangent;

layout(location=0) out vec4 normal;
layout(location=1) out vec4 worldPos;
layout(location=2) out vec4 material;
layout(location=3) out vec2 uv;
//layout(location=4) out mat4 TBN; //try to send it as mat3
layout(location=4) out mat3 TBN;
layout(location=7) out vec4 tsViewPos; //view in tangentspace
layout(location=8) out vec4 tsFragPos;
void main()
{
	uv = in_uv;
	material = param.materialProp;
	mat3 transWM = inverse(transpose(mat3(param.worldMatrix)));
	vec3 transNormal = transWM * in_normal;
	normal = vec4(normalize(transNormal), 0.0f);
	worldPos = (param.worldMatrix * vec4(position, 1));
	
	
	//mat3 WM3 = mat3(param.worldMatrix);
	vec3 T = normalize( transWM * in_tangent.xyz);
	vec3 B = normalize(transWM * in_bitangent.xyz);
	vec3 N = normalize(transNormal);
	
	TBN = mat3(T, B, N);
	mat3 tTBN = transpose(TBN);
	tsViewPos = vec4(tTBN * vec3(param.viewPos), 1.0);
	tsFragPos = vec4(tTBN * vec3(worldPos), 1.0);
    gl_Position = param.mvp * vec4(position, 1.0f);
}