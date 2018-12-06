#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) out vec4 out_albedo;
layout(location=1) out vec4 out_specular;
layout(location=2) out vec4 out_normal;
layout(location=3) out vec4 out_position;

layout(location = 0) in vec2 uv;
layout(location = 1) in vec4 materialProps;
layout(location = 2) in vec4 worldPos;
layout(location = 3) in vec4 normal;

layout (set = 0, binding = 1) uniform sampler2D Albedo[4]; 

float Depth()
{
	float z = gl_FragCoord.z;
	float near = 0.1;
	float far = 75.0f;
	return (2 * near) / (far + near - z * (far - near));
}

void main()
{
	uint index = clamp(uint(materialProps.w), 0, 4);
	
	vec4 color = texture(Albedo[index], uv);
	
	if (color.a < 0.1f)
		discard;
	
	out_albedo = color;
	out_specular = materialProps;
	out_normal = normal;
	out_position = worldPos;
	
	gl_FragDepth = Depth();
}
