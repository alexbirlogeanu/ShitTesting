#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) out vec4 blur;

layout(location=0) in vec4 uv;

layout(set=0, binding=0) uniform sampler2D Image;
layout(push_constant) uniform PushConstants
{
	bvec4 BlurParams;
};

void main()
{
	float weight[5] = float[] (0.227027f, 0.1945946f, 0.1216216, 0.054054f, 0.016216f);
	vec2 delta = 1.0f / textureSize(Image, 0);
	vec2 offset;
	if (BlurParams.x) //vertical blur
		offset = vec2(0.0f, delta.y);
	else
		offset = vec2(delta.x, 0.0f);
	
	vec4 color  = texture(Image, uv.st) * weight[0];
	
	for(int i = 1; i < 5; ++i)
	{
		color += texture(Image, uv.st + i * offset) * weight[i];
		color += texture(Image, uv.st - i * offset) * weight[i];
	}
	
	blur = color;
}