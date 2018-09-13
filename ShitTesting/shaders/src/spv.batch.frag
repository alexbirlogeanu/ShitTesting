#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) out vec4 albedo;
layout(location=1) out vec4 out_specular;
layout(location=2) out vec4 out_normal;
layout(location=3) out vec4 out_position;

layout(set=0, binding=1) uniform sampler2D BatchTextures[12];

struct MaterialPropertis
{
	float		Roughness;
	float		K;
	float		F0;
	uint		AlbedoTexture;
};

layout(location=0) in vec4 normal;
layout(location=1) in vec4 worldPos;
layout(location=2) in vec2 uv;
layout(location=3) flat in MaterialPropertis Properties;

float Depth()
{
	float z = gl_FragCoord.z;
	float near = 0.1;
	float far = 75.0f;
	return (2 * near) / (far + near - z * (far - near));
}

void main()
{
	const uint index = Properties.AlbedoTexture;

	albedo = texture(BatchTextures[index], uv);
	out_specular = vec4(Properties.Roughness, Properties.K, Properties.F0, 0.0f);
	out_normal = normal;
	out_position = worldPos;
	
	gl_FragDepth = Depth();
}