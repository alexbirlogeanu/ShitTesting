#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(early_fragment_tests) in;
layout(location=0) out vec4 color;

layout(set=0, binding=1) uniform sampler2D cubeMap;

layout(location=0) in vec4 ViewPosition;
layout(location=1) in vec4 LightColor;

vec4 GetSkyColor()
{
	vec3 coords = normalize(ViewPosition.xyz);
	coords.y = (coords.y + 1.0f) / 2.0f;
	return texture(cubeMap, coords.xy);
}

vec4 GetSkyColor2()
{
	float mPoint = 0.75f;
	vec3 V = normalize(ViewPosition.xyz);
	//vec3 ca = vec3(0.047, 0.361f, 0.573f);
	vec3 ca = vec3(0.0f);
	vec3 cb = vec3(1.0f);
	ca = pow(ca, vec3(2.2f));
	cb = pow(cb, vec3(2.2f));
	
	float y = 1.0f - (V.y + 1.0f) / 2.0f;
	vec3 cm = mix(ca, cb, 0.5f);

	float t = 0.0f;
	vec3 start = ca;
	vec3 stop = cb;
	
	if (y < mPoint)
	{
		t = y * mPoint;
		stop = cm;
		start = ca;
	}
	else
	{
		t = (y - mPoint) * (1.0f - mPoint) + mPoint;
		start = cm;
		stop = cb;
	}
	
	//return vec4(mix(start, stop, t), 1.0f);
	return vec4(V, 1.0f);
}

void main()
{
	float lightFactor = 0.5f;
	vec4 sColor = GetSkyColor();
	//vec4 sColor = GetSkyColor2();
	
	color = sColor * LightColor * lightFactor;
	color = max(color, sColor / 2.0f);
	
	color.a = 1.0f;
}