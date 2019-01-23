#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) in vec3 in_position;

layout(push_constant) uniform ShaderParams
{
	vec4 TestVector;
	mat4 Proj;
};

layout(location=0) out int layer;

void main()
{
	vec4 viewPos = vec4(in_position, 1.0f);
	viewPos.z = -(3.5f + gl_InstanceIndex * 2);
	gl_Position = viewPos;
	layer = gl_InstanceIndex;
}