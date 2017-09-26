#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (set=0, binding=0) uniform sampler2D colorAttach;
layout (set=0, binding=3) uniform sampler2D pickTexture;
layout (set=0, binding=1) uniform params
{
	float   width;
    float   height;
    float   samples;
    int     blur;
	int 	fxaa;
};

layout(location=0) out vec4 out_color;

layout(location=0) in vec2 uv;

void main()
{
	vec3 color;
	int useBlur = blur;
	color = texture(colorAttach, uv).rgb;
	if(fxaa != 0)
	{
		useBlur = 0;
		float deltaPixel = 1.0f / (width * height);
		vec3 nw = texture(colorAttach, uv + vec2(-deltaPixel, -deltaPixel)).rgb;
		vec3 ne = texture(colorAttach, uv + vec2(deltaPixel, -deltaPixel)).rgb;
		vec3 sw = texture(colorAttach, uv + vec2(deltaPixel, deltaPixel)).rgb;
		vec3 se = texture(colorAttach, uv + vec2(deltaPixel, deltaPixel)).rgb;
		vec3 m = texture(colorAttach, uv).rgb;
		
		vec3 luma = vec3(0.299, 0.587, 0.114);
		float lumaNW = dot(nw, luma);
		float lumaNE = dot(ne, luma);
		float lumaSE = dot(se, luma);
		float lumaSW = dot(sw, luma);
		float lumaM = dot(m, luma);
		
		vec2 dir;
		dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
		dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
		
		float reduceMul = 1.0f / 8.0f;
		float reduceMin = 1.0f / 128.0f;
		
		float lumaMin = min(lumaM, min(min(lumaSE, lumaSW), min(lumaNE, lumaNW)));
		float lumaMax = max(lumaM, max(max(lumaSE, lumaSW), max(lumaNE, lumaNW)));
		float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * reduceMul), reduceMin);
		float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
		
		float spanMax = 8.0f;
		dir = min(vec2( spanMax,  spanMax),
          max(vec2(-spanMax, -spanMax),
              dir * rcpDirMin));

		vec3 rgbA = (1.0/2.0) * (
                         texture(colorAttach, uv + dir * (1.0/3.0 - 0.5)).rgb +
                         texture(colorAttach, uv + dir * (2.0/3.0 - 0.5)).rgb);
		vec3 rgbB = rgbA * (1.0/2.0) + (1.0/4.0) * (
                                            texture(colorAttach, uv + dir * (0.0/3.0 - 0.5)).rgb +
                                            texture(colorAttach, uv + dir * (3.0/3.0 - 0.5)).rgb);
		float lumaB = dot(rgbB, luma);

		if((lumaB < lumaMin) || (lumaB > lumaMax)) 
		{
		
			color = rgbA;
		} else 
		{
			color = rgbB;
		}
		color = vec3(abs(dir.x),abs(dir.y), 0.0f);
	}
	if(useBlur != 0)
	{

		float blurSamples = 2 * samples + 1;
		blurSamples*=blurSamples;
		float dx = 1.0f/width;
		float dy = 1.0f/height;
		color = vec3(0.0f);
		for(float i = -samples; i <= samples; i+=1.0f)
		{
			for(float j = -samples; j <= samples; j+=1.0f)
			{
				color += texture(colorAttach, uv + vec2(i*dx, j*dy)).rgb;
			}
		}
		color/=blurSamples;
	}
	
	out_color = vec4(color, 1.0f);
}