#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) out vec4 albedo;
layout(location=1) out vec4 out_specular;
layout(location=2) out vec4 out_normal;
layout(location=3) out vec4 out_position;

layout(set=0, binding=1) uniform sampler2D text;
layout(set=0, binding=2) uniform sampler2D normalMap;
layout(set=0, binding=3) uniform sampler2D depthMap; //or displacement map

layout(location=0) in vec4 normal;
layout(location=1) in vec4 worldPos;
layout(location=2) in vec4 material;
layout(location=3) in vec2 uv;
layout(location=4) in mat3 TBN;
layout(location=7) in vec4 tsViewPos;
layout(location=8) in vec4 tsFragPos;

vec2 GetParalaxTextCoords()
{
	vec3 viewDir = normalize(tsViewPos.xyz - tsFragPos.xyz);
	float depth = texture(depthMap, uv).r;
	vec2 dispUV = viewDir.xy / viewDir.z * (depth * 0.1f);
	dispUV = uv - dispUV;
	return dispUV;
}

vec2 GetStepParalaxTextCoords()
{
	float numLayers = 5.0f;
	float layerStepDepth = 1.0f / numLayers;
	float currLayerDepth = 0.0f;
	vec3 viewDir = normalize(tsViewPos.xyz - tsFragPos.xyz);
	vec2 P = viewDir.xy * 0.1f;
	vec2 deltaCoords = P / numLayers;
	
	vec2 prevCoords = uv;
	float prevDepthLayer = 0.0f;

	vec2 textCoords = uv;
	float curDepthValue = texture(depthMap, uv).r;
	float prevDepthValue = curDepthValue;
	while(curDepthValue > currLayerDepth)
	{
		prevCoords = textCoords;
		
		textCoords -= deltaCoords;
		prevDepthLayer = currLayerDepth;
		prevDepthValue = curDepthValue;
		
		curDepthValue = texture(depthMap, textCoords).r;
		currLayerDepth += layerStepDepth;
		
	}
	
	float afterDepth = curDepthValue - currLayerDepth;
	float beforeDepth = prevDepthValue - prevDepthLayer;
	
	float weight = afterDepth / (afterDepth - beforeDepth );
	textCoords = prevCoords * weight + (1.0f - weight) * textCoords;
	
	return textCoords;
}
float Depth()
{
	float z = gl_FragCoord.z;
	float near = 0.01;
	float far = 75.0f;
	return (2 * near) / (far + near - z * (far - near));
}
vec2 GetTextCoords()
{
	return uv;
}

void main()
{  
	vec2 textCoords = GetTextCoords();
	vec2 lod = textureQueryLod(text, textCoords);
	albedo = texture(text, textCoords, lod.x);
	vec3 sNormal = texture(normalMap, textCoords).rgb;
	sNormal = normalize(sNormal * 2.0f - 1.0f); 
	out_normal = vec4(normalize(TBN * sNormal), 0.0f);
	out_position = worldPos;
	out_specular = material;
	
	gl_FragDepth = Depth();
}
