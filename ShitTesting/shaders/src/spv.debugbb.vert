#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) in vec3 in_position;

struct BoundingBox
{
    vec4  	BBMin;
    vec4   	BBMax;
	vec4 	Color;
};

layout(set = 0, binding = 0) buffer BoxesBuffer
{
	BoundingBox Boxes[];
};

layout(push_constant) uniform Constants
{
	mat4 ProjViewMatrix;
};

layout(location=0) out vec4 color;

void main()
{
	BoundingBox bb = Boxes[gl_InstanceIndex];
	color = bb.Color;
	
	vec3 wp = bb.BBMax.xyz;
	if(in_position.x < 0)
		wp.x = bb.BBMin.x;
	if(in_position.y < 0)
		wp.y = bb.BBMin.y;
	if(in_position.z < 0)
		wp.z = bb.BBMin.z;
		
	gl_Position = ProjViewMatrix * vec4(wp, 1.0f);
}