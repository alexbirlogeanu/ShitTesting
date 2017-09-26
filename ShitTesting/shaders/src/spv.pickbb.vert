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

void main()
{
	vec3 wp = BBMax.xyz;
	if(in_position.x < 0)
		wp.x = BBMin.x;
	if(in_position.y < 0)
		wp.y = BBMin.y;
	if(in_position.z < 0)
		wp.z = BBMin.z;
		
	gl_Position = MVPMatrix * vec4(wp, 1.0f);
}