#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec4 WorldPosition;
layout(location = 1) in mat4 InvModelMatrix;
layout(location = 5) in vec4 UV;

layout(set=0, binding=1) uniform sampler3D VolumeTexture;
layout(set=0, binding=2) uniform sampler2D FrontTexture;
layout(set=0, binding=3) uniform sampler2D BackTexture;

layout(location = 0) out vec4 Out;
layout(location = 1) out vec4 Debug;
void main()
{
	vec2 size = vec2(textureSize(FrontTexture, 0));
	vec2 uv = vec2(gl_FragCoord.xy) / size;
	
	vec3 start = vec3(InvModelMatrix * vec4(texture(BackTexture, uv).xyz, 0.0f));
	vec3 stop = vec3(InvModelMatrix * vec4(texture(FrontTexture, uv).xyz, 0.0f));
	
	float steps = 32.0;
	vec3 dir = stop - start;
	float len = length(dir);
	float s = len / steps;
	dir /= len; //normalize
	Out = vec4(1.0, 0.0, 0.0, 0.0);
	Debug = vec4(dir, 1.0);
	for (int i = 0; i < steps; ++i)
	{
		vec3 coords = (dir * i * s);
		coords.xy  = UV.st;
		coords.z = -coords.z;
		vec4 color = texture(VolumeTexture, coords);
		if(color.a > 0.0)
		{
			Out = color;
			break;
		}
	}
}