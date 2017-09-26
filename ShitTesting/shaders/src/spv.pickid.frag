#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) out uint idTexture;

layout(location=0) flat in uvec4 id;

void main()
{
	idTexture = id.x;
}