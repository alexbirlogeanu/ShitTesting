#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (set=0, binding=0) uniform sampler2D worldPosText;
layout (set=0, binding=1) uniform sampler2D shadowMap;

layout(location=0) out vec4 shadowFactor;


void main()
{
	vec3 worldPos = texture(worldPosText, uv).xyz;
	vec4 shadowPos = shadowProjView * vec4(worldPos, 1);
	vec2 shadowUV = (shadowPos.xy + vec2(1.0f, 1.0f)) / 2.0f;
}
