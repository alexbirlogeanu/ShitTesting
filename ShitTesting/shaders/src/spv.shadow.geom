/*
	WARNING! This shader is almost identical with the spv.shadow_terrain.geom.
	In the FUTURE, try to merge the tw0 shaders into one
*/
#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#extension GL_GOOGLE_include_directive : enable

#include "DepthUtils.h.spv"

layout(triangles) in;
layout(triangle_strip, max_vertices = 9) out;

layout(push_constant) uniform PushConstants
{
	mat4 ProjViewMatrix;
	mat4 ShadowProjViewMatrix;
	vec4 ViewPos;
};

struct ShadowSplit
{
	mat4 ProjViewMatrix;
	vec4 NearFar; //x - near, y - far, (z, w) - unused
};

layout(set = 1, binding = 0) uniform ShadowParams
{
	ivec4			NSplits;
	ShadowSplit		Splits[3]; //max 3 splits
};

float GetDepth(int vertexIndex)
{
	vec4 hPoint = ProjViewMatrix * gl_in[vertexIndex].gl_Position;
	return LinearizeDepth(hPoint.z / hPoint.w);
}

int GetSplitIndex(float depth)
{
	for (int i = 0; i < NSplits.x; ++i)
		if (depth < Splits[i].NearFar.y)
			return i;
	
	return max(NSplits.x - 1, 0); //last split
}

void TryAddNewSplit(in int newSplit, inout int splitList[3], inout int nSplits)
{
	for (int i = 0; i < nSplits; ++i)
	{
		if (newSplit == splitList[i])
			return;
	}
	
	if (nSplits >= 3)
		return;
		
	splitList[nSplits++] = newSplit;
}

void main()
{
	int splitIndexes[3];
	int numIndexes = 0;
	float bias = 0.25f;
	
	for (int i = 0; i < 3; ++i)
	{
		float d = GetDepth(i);
		TryAddNewSplit(GetSplitIndex(d), splitIndexes, numIndexes);
		//TryAddNewSplit(GetSplitIndex(d - bias), splitIndexes, numIndexes);
		TryAddNewSplit(GetSplitIndex(d + bias), splitIndexes, numIndexes);
	}
	
	for (int s = 0; s < 3; ++s)
	{
		gl_Layer = splitIndexes[s];
		mat4 PV = Splits[gl_Layer].ProjViewMatrix;
		for (int i = 0; i < 3; ++i)
		{
			gl_Position = PV * gl_in[i].gl_Position;
			EmitVertex();
		}
		EndPrimitive();
	}
}