#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(vertices = 3) out;

layout(location=0) in vec3 vs_normal[];
layout(location=1) in vec4 vs_material[];
layout(location=2) in vec2 vs_uv[];

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

layout(location=0) out vec3 te_normal[];
layout(location=1) out vec4 te_material[];
layout(location=2) out vec2 te_uv[];
layout(location=3) out patch OutputPatch oPatch;

vec3 ProjectToPlane(vec3 Point, vec3 PlanePoint, vec3 PlaneNormal)
{
    vec3 v = Point - PlanePoint;
    float Len = dot(v, PlaneNormal);
    vec3 d = Len * PlaneNormal;
    return (Point - d);
}

vec3 ComputeMidPoint(vec3 P0, vec3 P1, vec3 N0)
{
	return (2.0f * P0 + P1 - dot(N0, P1 - P0) * N0) / 3.0f;
}

vec3 ComputeMidNormal(int i, int j)
{
	vec3 Pi = vec3(gl_in[i].gl_Position);
	vec3 Pj = vec3(gl_in[j].gl_Position);
	vec3 Ni = vs_normal[i];
	vec3 Nj = vs_normal[j];
	vec3 Pij = Pj - Pi;
	return normalize(Ni + Nj - 2 * ((Pij * (Ni + Nj)) / (Pij * Pij)) * Pij);
}

void ComputeControlPoints()
{
	oPatch.b003 = vec3(gl_in[0].gl_Position); //P0
	oPatch.b300 = vec3(gl_in[1].gl_Position); //P1
	oPatch.b030 = vec3(gl_in[2].gl_Position); //P2
	
	// Edges are names according to the opposing vertex 
   /*  vec3 EdgeB300 = oPatch.WorldPos_B003 - oPatch.WorldPos_B030; 
    vec3 EdgeB030 = oPatch.WorldPos_B300 - oPatch.WorldPos_B003;
    vec3 EdgeB003 = oPatch.WorldPos_B030 - oPatch.WorldPos_B300;
	
	oPatch.WorldPos_B201 = oPatch.WorldPos_B003 + EdgeB030 * 2.0f / 3.0f;
	oPatch.WorldPos_B102 = oPatch.WorldPos_B003 + EdgeB030 / 3.0f;
	oPatch.WorldPos_B012 = oPatch.WorldPos_B030 + EdgeB300 * 2.0f / 3.0f;
	oPatch.WorldPos_B021 = oPatch.WorldPos_B030 + EdgeB300 / 3.0f;
	oPatch.WorldPos_B120 = oPatch.WorldPos_B300 + EdgeB003 * 2.0f / 3.0f;
	oPatch.WorldPos_B210 = oPatch.WorldPos_B300 + EdgeB003 / 3.0f;
	oPatch.debug1 = oPatch.WorldPos_B201;
	
	oPatch.WorldPos_B201 = ProjectToPlane(oPatch.WorldPos_B201, oPatch.WorldPos_B300, vs_normal[2]);
	oPatch.WorldPos_B102 = ProjectToPlane(oPatch.WorldPos_B102, oPatch.WorldPos_B003, vs_normal[1]);
	oPatch.WorldPos_B012 = ProjectToPlane(oPatch.WorldPos_B012, oPatch.WorldPos_B003, vs_normal[1]);
	oPatch.WorldPos_B021 = ProjectToPlane(oPatch.WorldPos_B021, oPatch.WorldPos_B030, vs_normal[0]);
	oPatch.WorldPos_B120 = ProjectToPlane(oPatch.WorldPos_B120, oPatch.WorldPos_B030, vs_normal[0]);
	oPatch.WorldPos_B210 = ProjectToPlane(oPatch.WorldPos_B210, oPatch.WorldPos_B300, vs_normal[2]);
	oPatch.debug2 = oPatch.WorldPos_B201; */
	
	oPatch.debug1.x = dot(vs_normal[0], oPatch.b300 - oPatch.b003);
	oPatch.b102 = ComputeMidPoint(oPatch.b003, oPatch.b300, vs_normal[0]);
	oPatch.debug1.y = dot(vs_normal[1], oPatch.b003 - oPatch.b300);
	oPatch.b201 = ComputeMidPoint(oPatch.b300, oPatch.b003, vs_normal[1]);
	oPatch.debug1.z = dot(vs_normal[1], oPatch.b030 - oPatch.b300);
	oPatch.b210 = ComputeMidPoint(oPatch.b300, oPatch.b030, vs_normal[1]);
	oPatch.debug2.x = dot(vs_normal[2], oPatch.b300 - oPatch.b030);
	oPatch.b120 = ComputeMidPoint(oPatch.b030, oPatch.b300, vs_normal[2]);
	oPatch.debug2.y = dot(vs_normal[2], oPatch.b003 - oPatch.b030);
	oPatch.b021 = ComputeMidPoint(oPatch.b030, oPatch.b003, vs_normal[2]);
	oPatch.debug2.z = dot(vs_normal[2], oPatch.b030 - oPatch.b003);
	oPatch.b012 = ComputeMidPoint(oPatch.b003, oPatch.b030, vs_normal[2]);
	
	oPatch.n101 = ComputeMidNormal(1, 0);
	oPatch.n110 = ComputeMidNormal(2, 1);
	oPatch.n011 = ComputeMidNormal(0, 2);
	
	vec3 center = (oPatch.b030 + oPatch.b003 + oPatch.b300) / 3.0f;
	oPatch.b111 = (oPatch.b021 + oPatch.b012 + oPatch.b102 + oPatch.b201 + oPatch.b210 + oPatch.b120) / 6.0;
	oPatch.b111 += (oPatch.b111 - center) / 2.0f;
}

void main()
{
	te_material[gl_InvocationID] = vs_material[gl_InvocationID];
	te_uv[gl_InvocationID] = vs_uv[gl_InvocationID];
	te_normal[gl_InvocationID] = vs_normal[gl_InvocationID];
	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position; //we pass the world position that comes from VS
	
	//if (gl_InvocationID == 0)
		ComputeControlPoints();
	
	int outerTessLevel = 10;
	int innerTessLevel = 10;
	gl_TessLevelOuter[0] = outerTessLevel;
	gl_TessLevelOuter[1] = outerTessLevel;
	gl_TessLevelOuter[2] = outerTessLevel;
	gl_TessLevelInner[0] = innerTessLevel;
	
	
}