#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

#include "DepthUtils.h.spv"

layout(location=0) out vec4 albedo;
layout(location=1) out vec4 out_specular;
layout(location=2) out vec4 out_normal;
layout(location=3) out vec4 out_position;

layout(set=0, binding=1) uniform sampler2D text;

layout(location=0) in vec4 normal;
layout(location=1) in vec4 worldPos;
layout(location=2) in vec4 material;
layout(location=3) in vec2 uv;

void main()
{  
	albedo = texture(text, uv);
	out_normal = normal;
	out_position = worldPos;
	out_specular = material;
	
	gl_FragDepth = LinearizeDepth(gl_FragCoord.z);
}
