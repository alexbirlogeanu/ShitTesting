#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(set=0, binding=0) uniform PostProcesParams
{
	vec4       screenCoords; //x = width, y = height, z = 1/width, w = 1/height
} params;

layout(set=0, binding=1) uniform sampler2D color;

layout(location=0) out vec4 out_color;

layout(location=0) in vec4 uv;

const float FXAA_REDUCE_MIN  = (1.0f/128.0f);
const float FXAA_SPAN_MAX = 8.0;
const float FXAA_REDUCE_MUL =0;//1.0/16.0;

vec3 Fxaa()
{
	vec2 ruv = uv.xy;// - 0.5 * params.screenCoords.zw;
	vec3 rgbNW = texture(color, ruv).rgb;
	vec3 rgbNE = textureOffset(color, ruv, ivec2(1, 0)).rgb;
	vec3 rgbSW = textureOffset(color, ruv, ivec2(0, 1)).rgb;
	vec3 rgbSE = textureOffset(color, ruv, ivec2(1, 1)).rgb;
	vec3 rgbM = texture(color, uv.st).rgb;
	
	vec3 luma = vec3(0.299, 0.587, 0.114);
	float lumaNW = dot(rgbNW, luma);
	float lumaNE = dot(rgbNE, luma);
	float lumaSW = dot(rgbSW, luma);
	float lumaSE = dot(rgbSE, luma);
	float lumaM = dot(rgbM, luma);
	
	float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
	float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
	
	vec2 dir;
	dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
	dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));
	
	float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
	float rcpDirMin = 1.0f / (min(abs(dir.x), abs(dir.y)) + dirReduce);
	dir = min(vec2(FXAA_SPAN_MAX, FXAA_SPAN_MAX), max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX), dir * rcpDirMin)) * params.screenCoords.zw;
	
	vec3 rgbA = 0.5f * ( texture(color, uv.st  + dir * (1/3.0 - 0.5)).rgb + texture(color, uv.st + dir * (2.03 / 3.0 - 0.5f)).rgb);
	vec3 rgbB = rgbA * 0.5f + 0.25f * (texture(color, uv.st + dir * (0.0f / 3.0f - 0.5f)).rgb + texture(color, uv.st + dir * (3.0f / 3.0f - 0.5f)).rgb);
	float lumaB = dot(rgbB, luma);
	if((lumaB < lumaMin) || (lumaB > lumaMax))
		return rgbA;
		
	return rgbB;
}

void main()
{
	//out_color = texture(color, uv.st) + 0.5f;
	out_color = vec4(Fxaa(), 1.0f);
}