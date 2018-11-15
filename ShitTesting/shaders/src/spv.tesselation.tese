#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (triangles, fractional_odd_spacing, ccw) in;

layout(std140, set = 0, binding = 0) uniform in_params
{
	mat4 ViewProjMatrix;
	mat4 worldMatrix;
	vec4 materialProp; //x = roughness, y = k, z = F0
	vec4 extra;
}param;

struct OutputPatch
{
    vec3 b030;
    vec3 b021;
    vec3 b012;
    vec3 b003;
    vec3 b102;
    vec3 b201;
    vec3 b300;
    vec3 b210;
    vec3 b120;
    vec3 b111;
	vec3 n110;
	vec3 n011;
	vec3 n101;
	vec3 debug1;
	vec3 debug2;
};

layout(location=0) in vec3 te_normal[];
layout(location=1) in vec4 te_material[];
layout(location=2) in vec2 te_uv[];
layout(location=3) in patch OutputPatch oPatch;

layout(location=0) out vec4 fg_normal;
layout(location=1) out vec4 fg_worldPos; 
layout(location=2) out vec4 fg_material; 
layout(location=3) out vec2 fg_uv;
layout(location=4) out vec4 fg_color;


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
	
	//fg_normal = vec4(oPatch.debug1, 0.0f);
	//fg_material = vec4(oPatch.debug2, 0.0f);
	
	float u = gl_TessCoord.x;
	float v = gl_TessCoord.y; 
	float w = gl_TessCoord.z;
	 
	float u3 = pow(u, 3.0f);
	float v3 = pow(v, 3.0f);
	float w3 = pow(w, 3.0f);
	float u2 = pow(u, 2.0f);
	float v2 = pow(v, 2.0f);
	float w2 = pow(w, 2.0f);
	
	fg_normal.xyz = te_normal[0] * u2
		+ te_normal[1] * v2
		+ te_normal[2] * w2 
		+ oPatch.n110 * w * u
		+ oPatch.n011 * u * v
		+ oPatch.n101 * w * v;
	
	fg_normal.w = 0.0f;
	fg_normal = vec4(interpolate3D(te_normal[0], te_normal[1], te_normal[2]), 0.0f);
	fg_normal = normalize(fg_normal);	
	
	//oPatch.b003 = vec3(gl_in[0].gl_Position); //P0
	//oPatch.b300 = vec3(gl_in[1].gl_Position); //P1
	//oPatch.b030 = vec3(gl_in[2].gl_Position); //P2
	
	vec3 worldPos = v3 * oPatch.b300 
		+ w3 * oPatch.b030 
		+ u3 * oPatch.b003 
		+ 3.0f * v2 * w * oPatch.b210 
		+ 3.0f * v * w2 * oPatch.b120 
		+ 3.0f * v2 * u * oPatch.b201
		+ 3.0f * v * u2 * oPatch.b102
		+ 3.0f * w2 * u * oPatch.b021 
		+ 3.0f * u2 * w * oPatch.b012 
		+ 6 * w * u * v * oPatch.b111;
	fg_worldPos = vec4(worldPos, 1.0f);
	//vec4 origPos = vec4(interpolate3D(vec3(gl_in[0].gl_Position), vec3(gl_in[1].gl_Position), vec3(gl_in[2].gl_Position)), u3);
	//fg_normal = fg_worldPos - origPos;
	fg_color = vec4(u, v, w, 1.0f);
	gl_Position = param.ViewProjMatrix * param.worldMatrix * fg_worldPos;
}