#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) in vec3 position;
layout(location=1) in vec2 in_uv;
layout(set=0,binding=0) uniform Params
{
    mat4 ModelMatrix;
    mat4 ViewMatrix;
    mat4 ProjMatrix;
};

layout(location = 0) out vec4 ViewPosition;
layout(location = 1) out mat4 InvModelMatrix;
layout(location = 5) out vec4 uv;
void main()
{
	ViewPosition =  ModelMatrix * vec4(position, 1.0);
	InvModelMatrix = inverse(ModelMatrix);
	gl_Position = ProjMatrix * ViewMatrix * ViewPosition;
	uv = vec4(in_uv, 0.0f, 0.0f);
	uv.t = 1.0 - uv.t;
}