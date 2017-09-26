#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(set=0, binding=0) uniform PostProcesParams
{
	vec4       screenCoords; //x = width, y = height, z = 1/width, w = 1/height
} params;

layout(set=0, binding=1) uniform sampler2D color;
layout(set=0, binding=2) uniform sampler3D LUT;
//layout(set=0, binding=2) uniform sampler2D bloom;
layout(location=0) out vec4 out_color;

layout(location=0) in vec4 uv;

void main()
{
	vec3 hdr = texture(color, uv.st).rgb;
	//hdr += texture(bloom, uv.st).rgb;
	
	
	float exposure = 0.95f;
	hdr = vec3(1.0f) - exp(-hdr * exposure);
	//hdr = hdr / (hdr + vec3(1.0f));
	vec3 lutCoords = vec3(1.0f - hdr.rg, hdr.b);
	/* vec3 lutColor = texture(LUT, lutCoords).rgb;
	hdr = mix(hdr, lutColor, 0.25f); */
	
	hdr = pow(hdr, vec3(1.0f / 2.2f));
	out_color = vec4(hdr, 1.0f);
}