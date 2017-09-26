#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) in vec3 in_position;

layout(set=0, binding=0) uniform PickParams
{
	mat4   MVPMatrix;
    uvec4  ID;
    vec4   BBMin;
    vec4   BBMax;
};

layout(location=0) out uvec4 ObjectID;

void main()
{
	gl_Position = MVPMatrix * vec4(in_position, 1.0f);
	ObjectID = ID;
}