#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) out vec4 light;

layout(location=0) in vec4 uv;

layout(set=0, binding=0) uniform sampler2D Image;

float treshold = 1.5f;

void main()
{
	vec3 c = texture(Image, uv.st).rgb;
	float bright = dot(c, vec3(0.2126, 0.7152, 0.0722));
	
	if(bright > treshold)
		light.rgb = c;
	else
		light.rgb = vec3(0.0f);
	light.w = bright;
}