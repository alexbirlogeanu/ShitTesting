#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (set=0, binding=0) uniform sampler2D albedoText;
layout (set=0, binding=1) uniform sampler2D specularText; //x = roughness, y = metalness, z = F0
layout (set=0, binding=2) uniform sampler2D normalText;
layout (set=0, binding=3) uniform sampler2D worldPosText;
layout (set=0, binding=5) uniform sampler2D shadowMap;
layout (set=0, binding=6) uniform sampler2D aoMap;

layout (set=0, binding=4) uniform params
{
	mat4	shadowProjView;	
	vec4 	dirLight;
	vec4 	cameraPos;
	vec4    lightIradiance;
	mat4    pointVPMatrix;
	mat4	pointModelMatrix;
    vec4    pointCenter;
    vec4    pointColor;
};

layout(location=0) out vec4 out_color;
layout(location=1) out vec4 debug;

layout(location=0) in vec2 uv;

/* float ShadowFactor(vec3 normal)
{
	vec3 worldPos = texture(worldPosText, uv).xyz;
	
	vec4 shadowPos = shadowProjView * vec4(worldPos, 1);
	shadowPos.xyz /= shadowPos.w;
	shadowPos.xy = (shadowPos.xy + vec2(1.0f, 1.0f)) / 2.0f;
	float samplesInShadow = 9.0f;
	vec3 shadowUV = shadowPos.xyz;
	float shadowBias = -0.001f * tan(acos(clamp(dot(normal, -dirLight.xyz), 0.01f, 1.0f)));
	shadowUV.z += shadowBias;
	
	float shadowFactor = texture(shadowMap, shadowUV, 0);
	shadowFactor += textureOffset(shadowMap, shadowUV, ivec2(-1, -1));
	shadowFactor += textureOffset(shadowMap, shadowUV, ivec2(0, -1));
	shadowFactor += textureOffset(shadowMap, shadowUV, ivec2(1, -1));
	shadowFactor += textureOffset(shadowMap, shadowUV, ivec2(-1, 0));
	shadowFactor += textureOffset(shadowMap, shadowUV, ivec2(1, 0));
	shadowFactor += textureOffset(shadowMap, shadowUV, ivec2(-1, 1));
	shadowFactor += textureOffset(shadowMap, shadowUV, ivec2(-1, 0));
	shadowFactor += textureOffset(shadowMap, shadowUV, ivec2(-1, 1));
	
	//return 1.0f;
	return shadowFactor / samplesInShadow;
	//return 1.0f - float(samplesInShadow) * 0.15f;
} */

const float ambientFactor = 0.15f;
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
	
	float NdotL = max(dot(N, L), 0.01f);
	float NdotV = max(dot(N, V), 0.01f);
	
	vec3 nom = NDF * F * G;
	float denom = 4 * NdotL * NdotV;
	vec3 specular = nom / denom;

	vec3 radiance = lightIradiance * NdotL * attenuation;
	color = (kD * albedo / PI + specular) * radiance;
	
	return color;
}

vec3 ComputeAmbientColor(vec3 normal, vec3 albedo)
{
	float ao = texture(aoMap, uv).r;
	//float ao = 1.0f;
	vec3 ambient = albedo * ao * ambientFactor;
	debug.xyz = ambient;
	return ambient;
}

void main()
{
	float lightAttenuation = 1.0f;
	vec4 materialProp = texture(specularText, uv);
	vec3 color = texture(albedoText, uv).rgb;
	vec3 normal = texture(normalText, uv).xyz;
	vec3 worldPos = texture(worldPosText, uv).xyz;
	float shadowFactor = texture(shadowMap, uv).r;
	float roughness = materialProp.x;
	float metalness = materialProp.y;
	vec3 L = -dirLight.xyz;
	vec3 V = normalize(cameraPos.xyz - worldPos);
	
	vec3 lightColor =   ComputeLightColor(normal, L, V,  color, shadowFactor * lightIradiance.rgb, lightAttenuation, roughness, metalness);
	lightColor		+=  ComputeAmbientColor(normal, color);
	
	out_color = vec4(lightColor, 1.0f);
	debug.rgb = gl_FragCoord.xyz;
	//debug.xyz = (skyColor + indirectColor) * ambient;
}