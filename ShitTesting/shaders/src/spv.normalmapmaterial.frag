#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) out vec4 albedo;
layout(location=1) out vec4 out_specular;
layout(location=2) out vec4 out_normal;
layout(location=3) out vec4 out_position;

struct MaterialPropertis
{
	float		Roughness;
	float		K;
	float		F0;
	uint		AlbedoTexture;
	uint		NormalMapTexture;
	uint		Padding[3]; //not used
};

layout(set=1, binding=0) buffer MaterialParams
{
	MaterialPropertis materials[];
};

layout(set=1, binding=1) uniform sampler2D BatchTextures[12];

layout(location=0) in vec4 normal;
layout(location=1) in vec4 worldPos;
layout(location=2) in vec2 uv;
layout(location=3) flat in uint BatchIndex;
layout(location=4) in mat3 TBN;
float Depth()
{
	float z = gl_FragCoord.z;
	float near = 0.1;
	float far = 75.0f;
	return (2 * near) / (far + near - z * (far - near));
}

void main()
{
	MaterialPropertis properties = materials[BatchIndex];
	uint albedoIndex = properties.AlbedoTexture;
	uint normalMapIndex = properties.NormalMapTexture;
	
	vec2 lod = textureQueryLod(BatchTextures[albedoIndex], uv);
	albedo = texture(BatchTextures[albedoIndex], uv, lod.x);
	vec3 sNormal = texture(BatchTextures[normalMapIndex], uv).rgb;
	sNormal = normalize(sNormal * 2.0f - 1.0f); 
	out_normal = vec4(normalize(TBN * sNormal), 0.0f);
	out_position = worldPos;
	out_specular =  vec4(properties.Roughness, properties.K, properties.F0, 0.0f);
	
	gl_FragDepth = Depth();
	
}