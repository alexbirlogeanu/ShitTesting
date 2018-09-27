#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (triangles, equal_spacing, ccw) in;

layout(location=0) in vec4 te_material[];
layout(location=1) in vec2 te_uv[];

layout(location=0) out vec4 fg_material;
layout(location=1) out vec2 fg_uv;


vec2 interpolate2D(vec2 v0, vec2 v1, vec2 v2)
{
   	return vec2(gl_TessCoord.x) * v0 + vec2(gl_TessCoord.y) * v1 + vec2(gl_TessCoord.z) * v2;
}

vec3 interpolate3D(vec3 v0, vec3 v1, vec3 v2)
{
   	return vec3(gl_TessCoord.x) * v0 + vec3(gl_TessCoord.y) * v1 + vec3(gl_TessCoord.z) * v2;
}
void main()
{
	fg_uv = interpolate2D(te_uv[0], te_uv[1], te_uv[2]);
	fg_material = te_material[0];
	
	vec4 worldPos = vec4(interpolate3D(vec3(gl_in[0].gl_Position), vec3(gl_in[1].gl_Position), vec3(gl_in[2].gl_Position)), 1.0f);
	gl_Position = worldPos;
}