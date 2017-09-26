#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) in vec3 position;

layout (set=0, binding=5) uniform Common
{
	vec4 CameraPosition;
    mat4 ProjViewMatrix;
};

layout(set=1, binding=0) uniform Specifics
{
	mat4 ModelMatrix;
    vec4 LightRadiance;
};
layout(location=0) flat out vec4 centerWorldPos;

void main()
{
	gl_Position = ProjViewMatrix * ModelMatrix * vec4(position, 1.0f);
	centerWorldPos = ModelMatrix * vec4(0.0f, 0.0f, 0.0f, 1.0f);
}