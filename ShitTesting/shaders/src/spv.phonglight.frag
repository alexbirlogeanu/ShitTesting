#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (set=0, binding=0) uniform sampler2D albedoText;
layout (set=0, binding=1) uniform sampler2D specularText; //x = roughness, y = k, z = F0
layout (set=0, binding=2) uniform sampler2D normalText;
layout (set=0, binding=3) uniform sampler2D worldPosText;
layout (set=0, binding=5) uniform sampler2DShadow shadowMap;

layout (set=0, binding=4) uniform params
{
	vec4 	dirLight;
	vec4 	cameraPos;
	vec4    screenCoords; //x = width, y = height, z = 1/width, w = 1/height
	mat4    pointMVPMatrix;
    vec4    pointCenter;
    vec4    pointColor;
};

layout(location=0) out vec4 out_color;

layout(location=0) in vec2 uv;

vec4 DiffuseLight(vec3 ambColor, vec3 normal)
{
	float lambertian = max(dot(-dirLight.xyz, normal), 0.0f);
	vec3 diffuseColor = ambColor * lambertian;
	return vec4(diffuseColor, 1.0f);
}

//int SampleShadowCompare(vec2 uv, float compvalue)
//{
	//float depth = texture(shadowMap, uv).r;
	//return clamp(int(depth < compvalue), 0, 1);
//}

float ShadowFactor(vec3 normal)
{
	vec3 worldPos = texture(worldPosText, uv).xyz;
	
	vec4 shadowPos = shadowProjView * vec4(worldPos, 1);
	shadowPos.xyz /= shadowPos.w;
	shadowPos.xy = (shadowPos.xy + vec2(1.0f, 1.0f)) / 2.0f;
	
	//vec2 shadowUV = shadowPos.xy + vec2(1.0f, 1.0f)) / 2.0f;
	vec3 shadowUV = shadowPos.xyz;
	
	int samplesInShadow = 0;
	float shadowBias = 0.005f * tan(acos(clamp(dot(normal, -dirLight.xyz), 0.01f, 1.0f)));
	
	//float dist = 3500.0f;
	//samplesInShadow += SampleShadowCompare(shadowUV + poissonDisk[0]/dist, shadowPos.z - shadowBias);
	//samplesInShadow += SampleShadowCompare(shadowUV + poissonDisk[1]/dist, shadowPos.z - shadowBias);
	//samplesInShadow += SampleShadowCompare(shadowUV + poissonDisk[2]/dist, shadowPos.z - shadowBias);
	//samplesInShadow += SampleShadowCompare(shadowUV + poissonDisk[3]/dist, shadowPos.z - shadowBias);
	
	float shadowFactor = texture(shadowMap, shadowUV, shadowBias);
	shadowFactor += textureOffset(shadowMap, shadowUV, ivec2(-1, -1), shadowBias); //wtf
	shadowFactor += textureOffset(shadowMap, shadowUV, ivec2(0, -1), shadowBias);
	shadowFactor += textureOffset(shadowMap, shadowUV, ivec2(1, -1), shadowBias);
	shadowFactor += textureOffset(shadowMap, shadowUV, ivec2(-1, 0), shadowBias);
	shadowFactor += textureOffset(shadowMap, shadowUV, ivec2(1, 0), shadowBias);
	shadowFactor += textureOffset(shadowMap, shadowUV, ivec2(-1, 1), shadowBias);
	shadowFactor += textureOffset(shadowMap, shadowUV, ivec2(-1, 0), shadowBias);
	shadowFactor += textureOffset(shadowMap, shadowUV, ivec2(-1, 1), shadowBias);
	
	return shadowFactor / 9.0f;
	
	return shadowFactor;
	//return 1.0f - float(samplesInShadow) * 0.15f;
}

vec4 SpecularPhong(vec3 color,vec3 normal)
{
	float specular = 0.0f;
	vec3 worldPos = texture(worldPosText, uv).xyz;
	float NdotL = max(dot(normal, -dirLight.xyz), 0.0f);
	vec3 viewDir = normalize(cameraPos.xyz - worldPos) ;
	
	vec3 H = normalize(viewDir - dirLight.xyz);
	float NdotH = max(dot( normal, H ), 0.0f);
	specular = pow(NdotH, 8.0f);
	return vec4(specular, specular, specular, 0.0f);
}
float ambientFactor = 0.15f;

void main()
{
	vec4 color = texture(albedoText, uv);
	vec3 normal = texture(normalText, uv).xyz;
	float shadowFactor = ShadowFactor(normal);
	
	float lightFactor = clamp(float(shadowFactor < 1.0f), 0.0f, 1.0f);
	vec4 ambient = vec4(color.rgb * ambientFactor, 1);
	vec4 diffuse = DiffuseLight(color.rgb, normal);
	diffuse = (shadowFactor + 0.15f) * diffuse;
	
	vec4 specular = lightFactor * SpecularPhong(color.rgb, normal);
	out_color = ambient + diffuse + specular;
	//out_color = vec4(0.0f, 1.0f, 0.0f, 1.0f);
}