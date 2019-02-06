#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

#include "DepthUtils.h.spv"

#define TEXTURE_LIMIT 4

layout(location=0) out vec4 out_albedo;
layout(location=1) out vec4 out_specular;
layout(location=2) out vec4 out_normal;
layout(location=3) out vec4 out_position;

layout(location = 0) in vec2 uv;
layout(location = 1) flat in vec4 materialProps;
layout(location = 2) in vec4 worldPos;
layout(location = 3) in vec4 normal;

layout (set = 0, binding = 1) uniform sampler2D Albedo[TEXTURE_LIMIT]; 

float Depth()
{
	float z = gl_FragCoord.z;
	float near = 0.01;
	float far = 75.0f;
	return (2 * near) / (far + near - z * (far - near));
}

void main()
{
	uint index = clamp(uint(materialProps.w), 0, TEXTURE_LIMIT - 1);
	
	vec2 lod = textureQueryLod(Albedo[index], uv);
	vec4 color = textureLod(Albedo[index], uv, lod.x);
	
	if (color.a < 0.1f)
		discard;
	
	out_albedo = color;
	out_specular = materialProps;
	out_normal = normal;
	out_position = worldPos;
	
	gl_FragDepth = LinearizeDepth(gl_FragCoord.z);
}
