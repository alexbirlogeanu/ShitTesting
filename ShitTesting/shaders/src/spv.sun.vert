#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) in vec3 in_position;
layout(location=1) in vec2 in_uv;

layout(set=0,binding=0) uniform SunParams
{
	vec4 LightDir;
	vec4 LightColor;
	mat4 ViewMatrix;
	mat4 ProjMatrix;
	vec4 CameraRight;
	vec4 CameraUp;
	vec4 Scale;
};

layout(location=0) out vec4 uv;
layout(location=1) out vec4 debug;

void main()
{
	float far = 20.0f;
	uv.st = in_uv;
	vec3 vsL = normalize(vec3(ViewMatrix * LightDir));
	vec3 pos = LightDir.xyz * far; 
	pos.xyz -= Scale.x * in_position.x * CameraRight.xyz;
	pos.xyz += Scale.x * in_position.y * CameraUp.xyz;
	gl_Position = ProjMatrix * ViewMatrix * vec4(pos, 1.0f);
	gl_Position.z = gl_Position.w;
	debug = gl_Position;
}
