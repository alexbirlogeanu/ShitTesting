#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) out vec4 color;

layout(set=0, binding=1) uniform sampler2D fontTexture;

layout(location=0) in vec4 uv;

void main()
{
	
	color = texture(fontTexture, uv.st);
	if(color.a < 0.1)
		color = vec4(0.4f, 0.4f, 0.4f, 0.75f);
}