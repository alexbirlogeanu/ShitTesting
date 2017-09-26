#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) out vec4 out_color;

layout(location=0) in vec4 color;

float Depth()
{
	float z = gl_FragCoord.z;
	float near = 0.1;
	float far = 75.0f;
	return (2 * near) / (far + near - z * (far - near));
}

void main()
{
	out_color = color;
	gl_FragDepth = Depth();
}