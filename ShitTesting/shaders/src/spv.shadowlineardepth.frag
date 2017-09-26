#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

void main()
{
	float z = gl_FragCoord.z;
	float near = 0.1;
	float far = 4.0f;
	gl_FragDepth = (2 * near) / (far + near - z * (far - near));
	//gl_FragDepth = z;
}