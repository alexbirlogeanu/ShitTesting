#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) out vec4 out_color;

layout(set=1, binding=1) uniform sampler2D Texture;

layout(location=0) in vec4 UV;
layout(location=1) in vec4 LifeSpawnInfo; //x - life spawn, y - current life span, z - fade time

float Depth()
{
	float z = gl_FragCoord.z;
	float near = 0.1;
	float far = 75.0f;
	return (2 * near) / (far + near - z * (far - near));
}

void main()
{
	vec2 lod = textureQueryLod(Texture, UV.st);
	vec4 color = textureLod(Texture, UV.st, lod.x);

	float ct = max(LifeSpawnInfo.y, 0.0f);
	float fade = LifeSpawnInfo.z;
	float tf = LifeSpawnInfo.x;
	float f = 1.0f;
	if(ct > tf - fade)
		f = 1.0f - clamp((ct - tf + fade) / fade, 0.0f, 1.0f);
	
	if(ct <= fade)
		f = ct / fade;
	
	
	out_color = vec4(color.rgb, mix(0.0f, color.a, f));
	gl_FragDepth = Depth();
}