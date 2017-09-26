#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) out vec4 blur;

layout(location=0) in vec4 uv;

layout(set=0, binding=0) uniform RadialParams
{
	vec4 ProjSunPos;
	vec4 LightDensity;
	vec4 LightDecay;
	vec4 SampleWeight;
	vec4 LightExposure;
	vec4 ShaftSamples;
};

layout(set=0, binding=1) uniform sampler2D Image;

vec4 RadialBlur(vec2 sunCoords)
{
	vec3 color  = vec3(0.0f);
	float blurSamples = 12.0f;
	vec2 blurVec = (sunCoords - uv.st) / blurSamples;
	vec2 textCoords = uv.st;
	
	for(int i = 0; i < int(blurSamples); ++i)
	{
		color += texture(Image, textCoords).rgb;
		textCoords += blurVec;
	}
	
	return vec4(color / blurSamples, 1.0f);
}

vec4 LightShafts(vec2 sunCoords)
{
	float samples = ShaftSamples.x;
	float Density = LightDensity.x;
	float Decay = LightDecay.x;
	float Weight = SampleWeight.x;
	float Exposure = LightExposure.x;
	vec2 blurVec = (sunCoords - uv.st) / samples * Density;
	vec2 textCoords = uv.st;
	vec3 color  = texture(Image, textCoords).rgb;
	float lightDecay = 1.0f;
	for(int i = 0; i < int(samples); ++i)
	{
		textCoords += blurVec;
		color += texture(Image, textCoords).rgb * lightDecay * Weight;
		lightDecay *= Decay;
	}
	
	return vec4(color * Exposure, 1.0f);
}

void main()
{
	vec2 sunCoords = (ProjSunPos.xy + 1.0f) / 2.0f;
	//blur = RadialBlur(sunCoords);
	blur = LightShafts(sunCoords);
}