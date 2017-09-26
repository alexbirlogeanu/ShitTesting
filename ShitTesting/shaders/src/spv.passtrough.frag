#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) out vec4 out_color;

layout(location=0) in vec4 uv;

layout(set=0, binding=0) uniform sampler2D Image;

void main()
{	
	vec4 color  = texture(Image, uv.st);
	out_color = color;
}