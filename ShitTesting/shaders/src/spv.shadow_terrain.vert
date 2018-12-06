#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) in vec3 position;

layout(push_constant) uniform PushConstants
{
	mat4 ShadowProjViewMatrix;
	mat4 WorldMatrix;
};

void main()
{
    gl_Position = ShadowProjViewMatrix * WorldMatrix * vec4(position, 1.0f);
}
