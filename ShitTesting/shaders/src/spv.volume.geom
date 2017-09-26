#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

layout(location=0) in vec4 in_uv[3];
layout(location=1) in ivec4 in_instance[3];

layout(location=0) out vec4 uv;
void main()
{
	for(int i = 0; i < 3; ++i)
	{
		gl_Position = gl_in[i].gl_Position;
		uv = vec4(in_uv[i].st, in_instance[i].x, 0.0f);
		gl_Layer = in_instance[i].x;
		EmitVertex();
	}
}