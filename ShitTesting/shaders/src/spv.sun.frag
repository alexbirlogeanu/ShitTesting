#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) out vec4 color;

layout(set=0,binding=0) uniform SunParams
{
	vec4 LightDir;
	vec4 LightColor;
	mat4 ViewMatrix;
	mat4 ProjMatrix;
	vec4 CameraRight;
	vec4 CameraUp;
	vec4 Scale; //zw frambuffer size
};

layout(set=0, binding=1) uniform sampler2D SunFlare;
layout(set=0, binding=2) uniform sampler2D DepthText;

layout(location=0) in vec4 uv;
layout(location=1) in vec4 debug;

void main()
{
	color = texture(SunFlare, uv.st);
	color.rgb *= LightColor.rgb * color.a;
	float d = texture(DepthText, gl_FragCoord.xy / Scale.zw).r;
	
	if(!any(bvec3(color.rgb)) || (d < 1.0f))
		color.rgb = vec3(0.0f);
}