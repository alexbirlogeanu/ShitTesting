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

float Depth()
{
	float z = gl_FragCoord.z;
	float near = 0.1;
	float far = 75.0f;
	return (2 * near) / (far + near - z * (far - near));
}

void main()
{
	vec2 size = vec2(textureSize(FrontTexture, 0));
	vec2 uv = vec2(gl_FragCoord.xy) / size;
	
	vec3 start = vec3(InvModelMatrix * vec4(texture(BackTexture, uv).xyz, 1.0f));
	vec3 stop = vec3(InvModelMatrix * vec4(texture(FrontTexture, uv).xyz, 1.0f));
	
	//vec3 start = texture(BackTexture, uv).xyz;
	//vec3 stop =  texture(FrontTexture, uv).xyz;
	
	float steps = 128.0;
	vec3 dir = stop - start;
	float len = length(dir);
	
	if(len < 0.01f) //?
		return;
		
	dir /= len; //normalize
	Out = vec4(1.0, 0.0, 0.0, 0.0);
	vec3 s = 1.0 / steps * dir;
	
	vec4 acumColor = vec4(0.0f);
	for (int i = 0; i < steps; ++i)
	{
		vec3 coords = start + (i * s);
		coords.xyz = coords.xyz + 0.5f;
		
		vec4 color = texture(VolumeTexture, coords);
		
		color.rgb *= color.a;
		color.a *= 0.1f;
		
		acumColor += (1.0f - acumColor.a) * color;
		Debug = vec4(acumColor.rgb, 1.0);
		Debug.w = len;
		
		if (acumColor.a > 0.45) //this should be parametrize
			break;
			
	}
	//Debug = acumColor;
	Out = acumColor;
	gl_FragDepth = Depth();
}