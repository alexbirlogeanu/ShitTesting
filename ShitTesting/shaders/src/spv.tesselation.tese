#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (triangles, equal_spacing, ccw) in;

layout(location=0) in vec4 te_normal[];
layout(location=1) in vec4 te_material[];
layout(location=2) in vec2 te_uv[];

layout(location=0) out vec4 fg_normal;
layout(location=1) out vec4 fg_worldPos;
layout(location=2) out vec4 fg_material;
layout(location=3) out vec2 fg_uv;

layout(std140, set = 0, binding = 0) uniform in_params
{
	mat4 ViewProjMatrix;
	mat4 worldMatrix;
	vec4 materialProp; //x = roughness, y = k, z = F0
	vec4 viewPos;
}param;

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
	vec3 normal = interpolate3D(te_normal[0].xyz, te_normal[1].xyz, te_normal[2].xyz);
	fg_normal = vec4(normalize(normal), 1.0f);
	fg_uv = interpolate2D(te_uv[0], te_uv[1], te_uv[2]);
	fg_worldPos = vec4(interpolate3D(vec3(gl_in[0].gl_Position), vec3(gl_in[1].gl_Position), vec3(gl_in[2].gl_Position)), 1.0f);
	fg_material = te_material[0];
	
	gl_Position = param.ViewProjMatrix * fg_worldPos;
}