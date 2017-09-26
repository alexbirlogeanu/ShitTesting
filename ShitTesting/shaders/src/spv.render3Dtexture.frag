#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) out vec4 out_color;

layout(location=0) in vec4 uv;

const float PI = 3.1415926535897;
float F(float x, float freq)
{
	const float A = 1.0f;
	float w = PI / 3.0;
	float y = A * sin(2 * PI * (freq + 0.1) * x + w);
	return y;
}

void main()
{	
	float layer = uv.z;
	float y = F(uv.s, layer / 5.0);
	float minRange = 0.45f;
	float maxRange = 0.75f;
	float localRange = minRange + (1.0 - layer / 32.0) * (maxRange - minRange) - 0.05;
	y = localRange + ((y + 1.0f) / 2.0f) * (maxRange - localRange);
	
	//vec3 c =  (uv.t >= minRange && uv.t <= maxRange)? vec3(y) : vec3(0.05f);
	vec4 c =  (uv.t >= y)? vec4(y, y, y, layer) : vec4(0.0f);
	out_color = c;
}