#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(vertices = 3) out;

layout(location=0) in vec3 vs_normal[];
layout(location=1) in vec4 vs_material[];
layout(location=2) in vec2 vs_uv[];

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

layout(location=0) out vec3 te_normal[];
layout(location=1) out vec4 te_material[];
layout(location=2) out vec2 te_uv[];
layout(location=3) out OutputPatch oPatch[];

float wij(int i, int j)
{
	return dot(gl_in[j].gl_Position.xyz - gl_in[i].gl_Position.xyz, vs_normal[i]);
}

vec3 ComputeMidNormal(int i, int j)
{
	vec3 Pi = vec3(gl_in[i].gl_Position);
	vec3 Pj = vec3(gl_in[j].gl_Position);
	vec3 Ni = vs_normal[i];
	vec3 Nj = vs_normal[j];
	vec3 Pij = Pj - Pi;
	return Ni + Nj - 2 * ((Pij * (Ni + Nj)) / (Pij * Pij)) * Pij;
}

void ComputeControlPoints()
{
	float N0 = vs_normal[0][gl_InvocationID];
	float N1 = vs_normal[1][gl_InvocationID];
	float N2 = vs_normal[2][gl_InvocationID];
	
	float P0 = gl_in[0].gl_Position[gl_InvocationID];
	float P1 = gl_in[1].gl_Position[gl_InvocationID];
	float P2 = gl_in[2].gl_Position[gl_InvocationID];
	
	oPatch[gl_InvocationID].b300 = P0;
	oPatch[gl_InvocationID].b030 = P1;
	oPatch[gl_InvocationID].b003 = P2;
	
	oPatch[gl_InvocationID].b210 = (2.0f * P0 + P1 - wij(0, 1) * N0) / 3.0f;
	oPatch[gl_InvocationID].b120 = (2.0f * P1 + P0 - wij(1, 0) * N1) / 3.0f;
	oPatch[gl_InvocationID].b021 = (2.0f * P1 + P2 - wij(1, 2) * N1) / 3.0f;
	oPatch[gl_InvocationID].b012 = (2.0f * P2 + P1 - wij(2, 1) * N2) / 3.0f;
	oPatch[gl_InvocationID].b102 = (2.0f * P2 + P0 - wij(2, 0) * N2) / 3.0f;
	oPatch[gl_InvocationID].b201 = (2.0f * P0 + P2 - wij(0, 2) * N0) / 3.0f;
		
	
	float V = (P0 + P1 + P2) / 3.0f;
	float E = (oPatch[gl_InvocationID].b210 + oPatch[gl_InvocationID].b120 + oPatch[gl_InvocationID].b021 + oPatch[gl_InvocationID].b012 + oPatch[gl_InvocationID].b102 + oPatch[gl_InvocationID].b201) / 6.0;
	oPatch[gl_InvocationID].b111 = E + (E - V) / 2.0f;
}

void main()
{
	te_material[gl_InvocationID] = vs_material[gl_InvocationID];
	te_uv[gl_InvocationID] = vs_uv[gl_InvocationID];
	te_normal[gl_InvocationID] = vs_normal[gl_InvocationID];
	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position; //we pass the world position that comes from VS
	
	ComputeControlPoints();
	
	int outerTessLevel = 8;
	int innerTessLevel = 1;
	gl_TessLevelOuter[0] = outerTessLevel;
	gl_TessLevelOuter[1] = outerTessLevel;
	gl_TessLevelOuter[2] = outerTessLevel;
	gl_TessLevelInner[0] = innerTessLevel;
	
	
}