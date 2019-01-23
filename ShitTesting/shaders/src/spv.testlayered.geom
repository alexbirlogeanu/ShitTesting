#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(triangles) in;

layout(triangle_strip, max_vertices = 6) out;

layout(location = 0) in int layer[];

layout(push_constant) uniform ShaderParams
{
	vec4 TestVector;
	mat4 Proj;
};

void main()
{
	gl_Layer = 0;
	for (int i = 0; i < 3; ++i)
	{
		gl_Position = Proj * gl_in[i].gl_Position;
		EmitVertex();
	}
	EndPrimitive();
	gl_Layer = 1;
	for (int i = 0; i < 3; ++i)
	{
		gl_Position = Proj * gl_in[i].gl_Position;
		EmitVertex();
	}
	EndPrimitive();
}
