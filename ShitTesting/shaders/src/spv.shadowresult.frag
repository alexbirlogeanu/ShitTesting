#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

#include "DepthUtils.h.spv"

struct ShadowSplit
{
	mat4 ProjViewMatrix;
	vec4 NearFar; //x - near, y - far, (z, w) - unused
};

layout(set=0,binding=0) uniform Constants
{
	vec4   	LightDirection;
	vec4	CameraPosition;
    mat4   	ShadowProjMatrix;
	ivec4	NSplits;
	ShadowSplit Splits[5];
};

//layout(set = 0, binding = 1) uniform sampler2DShadow ShadowMap;
layout(set = 0, binding = 1) uniform sampler2DArrayShadow ShadowMap;
layout(set = 0, binding = 2) uniform sampler2D WorldPosMap;
layout(set = 0, binding = 3) uniform sampler2D NormalMap;
layout(set = 0, binding = 4) uniform sampler2D ShadowText;
layout(set = 0, binding = 5) uniform sampler1D BlockerDist;
layout(set = 0, binding = 6) uniform sampler1D PCFDist;
layout(set = 0, binding = 7) uniform sampler2D DepthBuffer;

layout(location = 0) in vec4 uv;

layout(location = 0) out float ShadowResult;
layout(location = 1) out vec4 Debug;


/* float ShadowFactor()
{
	vec3 worldPos = texture(WorldPosMap, uv.st).xyz;
	vec3 normal = texture(NormalMap, uv.st).xyz;
	
	vec4 shadowPos = ShadowProjMatrix * vec4(worldPos, 1);
	shadowPos.xyz /= shadowPos.w;
	shadowPos.xy = (shadowPos.xy + vec2(1.0f, 1.0f)) / 2.0f;
	float samplesInShadow = 9.0f;
	vec3 shadowUV = shadowPos.xyz;
	float shadowBias = -0.01f;// * tan(acos(clamp(dot(normal, -LightDirection.xyz), 0.01f, 1.0f)));
	shadowUV.z += shadowBias;
	
	float shadowFactor = texture(ShadowMap, shadowUV, 0);
	shadowFactor += textureOffset(ShadowMap, shadowUV, ivec2(-1, -1));
	shadowFactor += textureOffset(ShadowMap, shadowUV, ivec2(0, -1));
	shadowFactor += textureOffset(ShadowMap, shadowUV, ivec2(1, -1));
	shadowFactor += textureOffset(ShadowMap, shadowUV, ivec2(-1, 0));
	shadowFactor += textureOffset(ShadowMap, shadowUV, ivec2(1, 0));
	shadowFactor += textureOffset(ShadowMap, shadowUV, ivec2(-1, 1));
	shadowFactor += textureOffset(ShadowMap, shadowUV, ivec2(-1, 0));
	shadowFactor += textureOffset(ShadowMap, shadowUV, ivec2(-1, 1));
	
	return shadowFactor / samplesInShadow;
} */

int GetShadowSplit()
{
	float d = texture(DepthBuffer, uv.st).x;
	//d = InverseLinearDepth(d);
	
	for (int i = 0; i < NSplits.x; ++i)
		if (d < Splits[i].NearFar.y)
			return i;
			
	return max(NSplits.x - 1, 0);
}


float CascadeShadowFactor()
{
	vec3 worldPos = texture(WorldPosMap, uv.st).xyz;
	vec3 normal = texture(NormalMap, uv.st).xyz;
	int splitIndex = GetShadowSplit();
	
	vec4 shadowPos = Splits[splitIndex].ProjViewMatrix * vec4(worldPos, 1);
	shadowPos.xyz /= shadowPos.w;
	shadowPos.xy = (shadowPos.xy + vec2(1.0f, 1.0f)) / 2.0f;
	float samplesInShadow = 9.0f;
	vec4 shadowUV;
	shadowUV.xyw = shadowPos.xyz;
	float shadowBias = -0.01f;// * tan(acos(clamp(dot(normal, -LightDirection.xyz), 0.01f, 1.0f)));
	shadowUV.w += shadowBias;
	shadowUV.w = LinearizeDepth(shadowUV.w);
	
	shadowUV.z = float(splitIndex);
	
	Debug.xyz = shadowUV.xyz;
	float shadowFactor = texture(ShadowMap, shadowUV);
	shadowFactor += textureOffset(ShadowMap, shadowUV, ivec2(-1, -1));
	shadowFactor += textureOffset(ShadowMap, shadowUV, ivec2(0, -1));
	shadowFactor += textureOffset(ShadowMap, shadowUV, ivec2(1, -1));
	shadowFactor += textureOffset(ShadowMap, shadowUV, ivec2(-1, 0));
	shadowFactor += textureOffset(ShadowMap, shadowUV, ivec2(1, 0));
	shadowFactor += textureOffset(ShadowMap, shadowUV, ivec2(-1, 1));
	shadowFactor += textureOffset(ShadowMap, shadowUV, ivec2(-1, 0));
	shadowFactor += textureOffset(ShadowMap, shadowUV, ivec2(-1, 1));
	
	return shadowFactor / samplesInShadow;
}

/* float PCSSBias = -0.001f;
float lightSize = 0.001f;
int blockerSearchSamples = 3;
int pcfSamples = 5;
float GetSearchRadius(float receiverDist)
{
	vec4 eyePos = ShadowProjMatrix * CameraPosition;
	//eyePos.xyz /= eyePos.w;
	eyePos.z = Depth(eyePos.z);
	
	return lightSize * receiverDist / eyePos.z;
} 

float FindBlockerDistance(vec3 shadowCoords)
{
	float searchRadius = GetSearchRadius(shadowCoords.z);
	int blockers = 0;
	float avgBlockDist = 0.0f;
	vec2 deltaUV = vec2(1.0f) / vec2(textureSize(ShadowText, 0));
	Debug = searchRadius;
	for(int i = 0; i < blockerSearchSamples; ++i)
	{
		float z = texture(ShadowText, shadowCoords.xy + texture(BlockerDist, float(i) / float(blockerSearchSamples)).xy * searchRadius).r;
		if(z < shadowCoords.z)
		{
			++blockers;
			avgBlockDist += z;
		}
	}
		
	if(blockers == 0)
		return 0;
	
	return avgBlockDist / float(blockers);
}

float PCSSShadowFactor(float penumbraRadius, vec3 shadowCoords)
{
	vec2 deltaUV = vec2(1.0f) / vec2(textureSize(ShadowMap, 0));
	float factor = 0.0f;
	for(int i = 0; i < pcfSamples; ++i)
	{
		vec3 pcfCoords = shadowCoords;
		shadowCoords.xy += texture(PCFDist, float(i) / float(pcfSamples)).xy * penumbraRadius;
		factor += texture(ShadowMap, shadowCoords);
	}
		
		float samples = float(2 * pcfSamples + 1);
		return factor / (samples * samples);
}

float PCSS()
{
	vec3 worldPos = texture(WorldPosMap, uv.st).xyz;
	vec3 normal = texture(NormalMap, uv.st).xyz;
	
	vec4 shadowPos = ShadowProjMatrix * vec4(worldPos, 1);
	shadowPos.xyz /= shadowPos.w;
	shadowPos.xy = (shadowPos.xy + vec2(1.0f)) / 2.0f;
	vec3 shadowCoords = shadowPos.xyz;
	shadowCoords.z = Depth(shadowCoords.z) + PCSSBias;
	
	float blockerDist = FindBlockerDistance(shadowCoords);
	Debug = blockerDist;
	if(blockerDist == 0)
		return 1.0f;
	
	float penumbraWidth = (shadowCoords.z - blockerDist) / blockerDist;
	float penumbraRadius = penumbraWidth * lightSize / shadowCoords.z;
	
	return PCSSShadowFactor(penumbraRadius, shadowCoords);
} */

void main()
{
	ShadowResult = CascadeShadowFactor();
	//ShadowResult = PCSS();
}