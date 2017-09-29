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
	float s = 1.0 / steps;
	dir /= len; //normalize
	Out = vec4(1.0, 0.0, 0.0, 0.0);
	Debug = vec4(dir, 1.0);
	 for (int i = 0; i < steps; ++i)
	{
		vec3 coords = start + (dir * i * s);
		//coords.xy  = UV.st;
		coords.xy = coords.xy -(-0.5);
		coords.y = 1 - coords.y;
		coords.z = coords.z * 0.5 + 0.5;
		Debug.xyz = coords;
		vec4 color = texture(VolumeTexture, coords);
		if(color.a > 0.0)
		{
			Out = color;
			break;
		}
		
	} 
	/* vec4 color = texture(VolumeTexture, vec3(UV.st, 0.25f));
	if(color.a > 0.0)
		Out = color; */
}