#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (triangles, fractional_odd_spacing, cw) in;

layout(std140, set = 0, binding = 0) uniform in_params
{
	mat4 ViewProjMatrix;
	mat4 WorldMatrix;
	vec4 MaterialProp; //x = roughness, y = k, z = F0
	vec4 TesselationParams;  //x - outter, y - inner tessellation, z - tessellation factor
	vec4 PatchParams; //xy - number of cells that are in terrain texture patch, zw - total number of cells in a terrain grid
}param;

struct OutputPatch
{
    float b030;
    float b021;
    float b012;
    float b003;
    float b102;
    float b201;
    float b300;
    float b210;
    float b120;
    float b111;
	float n110;
	float n011;
	float n101;
};

layout(location=0) in vec3 te_normal[];
layout(location=1) in vec4 te_material[];
layout(location=2) in vec2 te_uv[];
layout(location=3) in OutputPatch iPatch[];

layout(location=0) out vec4 fg_normal;
layout(location=1) out vec4 fg_material; 
layout(location=2) out vec2 fg_uv;
layout(location=3) out vec4 fg_worldPos; 
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
	const float tesFactor = param.TesselationParams.z;
	
	fg_uv = interpolate2D(te_uv[1], te_uv[2], te_uv[0]);
	fg_material = te_material[0];
	
	float u = gl_TessCoord.x;
	float v = gl_TessCoord.y; 
	float w = gl_TessCoord.z;
	float u2 = u * u;
	float v2 = v * v;
	float w2 = w * w;
	float u3 = u2 * u;
	float v3 = v2 * v;
	float w3 = w2 * w;
	
	
	vec3 n200 = te_normal[0];
	vec3 n020 = te_normal[1];
	vec3 n002 = te_normal[2];
	vec3 n110 = normalize(vec3(iPatch[0].n110, iPatch[1].n110, iPatch[2].n110));
	vec3 n011 = normalize(vec3(iPatch[0].n011, iPatch[1].n011, iPatch[2].n011));
	vec3 n101 = normalize(vec3(iPatch[0].n101, iPatch[1].n101, iPatch[2].n101));
	
	vec3 pnNormal = n200 * w2 + n020 * u2 + n002 * v2
					+ n110 * w * u + n011 * u * v + n101 * w * v;
	
	vec3 barNormal = interpolate3D(te_normal[1], te_normal[2], te_normal[0]);
	vec3 finalNormal = mix(barNormal, pnNormal, tesFactor);
	vec3 transNormal = inverse(transpose(mat3(param.WorldMatrix))) * finalNormal;
	
	fg_normal = vec4(normalize(transNormal), 0.0f);
	
	vec3 b300 = vec3(iPatch[0].b300, iPatch[1].b300, iPatch[2].b300);
	vec3 b030 = vec3(iPatch[0].b030, iPatch[1].b030, iPatch[2].b030);
	vec3 b003 = vec3(iPatch[0].b003, iPatch[1].b003, iPatch[2].b003);
	vec3 b210 = vec3(iPatch[0].b210, iPatch[1].b210, iPatch[2].b210);
	vec3 b120 = vec3(iPatch[0].b120, iPatch[1].b120, iPatch[2].b120);
	vec3 b201 = vec3(iPatch[0].b201, iPatch[1].b201, iPatch[2].b201);
	vec3 b021 = vec3(iPatch[0].b021, iPatch[1].b021, iPatch[2].b021);
	vec3 b102 = vec3(iPatch[0].b102, iPatch[1].b102, iPatch[2].b102);
	vec3 b012 = vec3(iPatch[0].b012, iPatch[1].b012, iPatch[2].b012);
	vec3 b111 = vec3(iPatch[0].b111, iPatch[1].b111, iPatch[2].b111);
	
	 vec3 worldPos = b300 * w3
		+ b030 * u3 
		+ b003 * v3
		+ b210 * 3.0f * w2 * u
		+ b120 * 3.0f * w * u2
		+ b201 * 3.0f * w2 * v
		+ b021 * 3.0f * u2 * v
		+ b102 * 3.0f * w * v2
		+ b012 * 3.0f * u * v2
		+ b111 * 6.0f * u * v * w;
		
	vec3 origPos = interpolate3D(vec3(gl_in[1].gl_Position), vec3(gl_in[2].gl_Position), vec3(gl_in[0].gl_Position));
	fg_worldPos = param.WorldMatrix * vec4(mix(origPos, worldPos, tesFactor), 1.0f);

	fg_color = vec4(u, v, w, 1.0f);
	gl_Position = param.ViewProjMatrix * fg_worldPos;
}