#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (set=0, binding=0) uniform sampler2D albedoText;
layout (set=0, binding=1) uniform sampler2D specularText; //x = roughness, y = k, z = F0
layout (set=0, binding=2) uniform sampler2D normalText;
layout (set=0, binding=3) uniform sampler2D worldPosText;
//layout (set=0, binding=4) uniform sampler2D lightMap;

layout (set=0, binding=5) uniform Common
{
	vec4 CameraPosition;
    mat4 ProjViewMatrix;
};

layout(set=1, binding=0) uniform Specifics
{
	vec4 Attenuation;
	mat4 ModelMatrix;
    vec4 LightRadiance;
};

layout(location=0) out vec4 out_color;
layout(location=1) out vec4 debug;

layout(location=0) flat in vec4 centerWorldPos;

const float PI = 3.1415926535897932384626433832795f;

//normal distribution function TrowbridgeReitzGGX
float NormalDistribution(float roughness, vec3 N, vec3 H)
{
	float rough2 = roughness * roughness;
	rough2 *= rough2;
	float NdotH = max(dot(N, H), 0.0f);
	float denom = (NdotH * NdotH) * (rough2 - 1) + 1;
	denom = PI * denom * denom;
	
	return rough2 / denom;
}

float GeometryShlick(float NdotV, float k)
{
	float nom = NdotV;
	float denom = NdotV * (1 - k) + k;
	
	return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	float k = roughness + 1;
	k = k * k / 8.0f;
	float NdotV = max(dot(N, V), 0.0f);
	float NdotL = max(dot(N, L), 0.0f);
	return GeometryShlick(NdotV, k) * GeometryShlick(NdotL, k);
}

vec3 FresnelSchilick(vec3 N, vec3 H, vec3 F0)
{
	float NdotH = max(dot(N, H), 0.0f);
	return F0 + (vec3(1.0f) - F0) * pow((1 - NdotH), 5.0f);
}

vec3 ComputeLightColor(vec3 N, vec3 L, vec3 V, vec3 albedo, vec3 lightIradiance, float attenuation, float roughness, float metalness)
{
	vec3 color = vec3 (0.0f);
	vec3 H = normalize(V + L);
	vec3 F0 = vec3(0.04f);
	F0 = mix(F0, albedo, metalness);
	
	float NDF = NormalDistribution(roughness, N, H);
	float G = GeometrySmith(N, V, L, roughness);
	vec3 F = FresnelSchilick(H, V, F0);
	
	vec3 kS = F;
	vec3 kD = vec3(1.0f) - kS;
	kD *= 1.0f - metalness;
	kD = max(kD, vec3(0.0f));
	
	float NdotL = max(dot(N, L), 0.01f);
	float NdotV = max(dot(N, V), 0.01f);
	
	vec3 nom = NDF * F * G;
	float denom = 4 * NdotL * NdotV;
	vec3 specular = nom / denom;

	vec3 radiance = lightIradiance * NdotL * attenuation;
	color = (kD * albedo / PI + specular) * radiance;
	debug = vec4(kD, 1.0f);
	
	return color;
}

float Depth()
{
	float z = gl_FragCoord.z;
	float near = 0.1;
	float far = 75.0f;
	return (2 * near) / (far + near - z * (far - near));
}

void main()
{
	ivec2 screenSize = textureSize(albedoText, 0);
	//vec2 uv = gl_FragCoord.xy * 1.0f / vec2(screenSize);
	ivec2 uv = ivec2(gl_FragCoord.xy);
	vec4 materialProp = texelFetch(specularText, uv, 0);
	//vec3 light = texture(lightMap, uv).rgb;
	vec3 color = texelFetch(albedoText, uv, 0).rgb;
	vec3 normal = texelFetch(normalText, uv, 0).xyz;
	vec3 worldPos = texelFetch(worldPosText, uv, 0).xyz;
	float roughness = materialProp.x;
	float metalness = materialProp.y;
	
	vec3 V = normalize(CameraPosition.xyz - worldPos);
	vec3 L = centerWorldPos.xyz - worldPos;
	float distToLight = length(L);
	L /= distToLight;
	float attenuation = 1.0f - (Attenuation.x + Attenuation.y * distToLight + Attenuation.z * pow(distToLight, 2)) / Attenuation.w;
	//float attenuation = 1.f / (1.f + distToLight * distToLight);
	
	vec3 lightColor = ComputeLightColor(normal, L, V, color, LightRadiance.rgb, attenuation, roughness, metalness);
	
	out_color = vec4(lightColor, 1.0f);
	gl_FragDepth = Depth();
	
	
}