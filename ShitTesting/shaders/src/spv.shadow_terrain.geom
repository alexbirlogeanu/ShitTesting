#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(triangles) in;
layout(triangle_strip, max_vertices = 9) out;

layout(push_constant) uniform PushConstants
{
	mat4 ShadowProjViewMatrix;
	mat4 ViewMatrix; 
	mat4 WorldMatrix;
};

struct ShadowSplit
{
	mat4 ProjViewMatrix;
	vec4 NearFar; //x - near, y - far, (z, w) - unused
};

layout(set = 0, binding = 0) uniform ShadowParams
{
	ivec4			NSplits;
	ShadowSplit		Splits[3]; //max 3 splits
};

float LiniarizeDepth(float z)
{
	float near = 0.01;
	float far = 75.0f;
	return (2 * near) / (far + near - z * (far - near));
}

float GetDepth(int vertexIndex)
{
	vec4 hPoint = ViewMatrix * gl_in[vertexIndex].gl_Position;
	return LiniarizeDepth(hPoint.z / hPoint.w);
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