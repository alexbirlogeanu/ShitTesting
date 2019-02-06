#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

#include "DepthUtils.h.spv"

layout(location=0) out vec4 out_color;

layout(location=0) in vec4 color;

void main()
{
	out_color = color;
	gl_FragDepth = LinearizeDepth(gl_FragCoord.z);
}