#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) out float ao;
layout(location=1) out vec4 debug;

layout(location=0) in vec4 uv;

layout(set=0, binding=0) uniform sampler2D Normals;
layout(set=0, binding=1) uniform sampler2D Positions;
layout(set=0, binding=2) uniform sampler2D DepthMap;

layout(set=0, binding=3) uniform ConstParams
{
	vec4 Samples[64];
    vec4 Noise[16];
};

layout(set=1, binding=0) uniform VarParams
{
	mat4 ProjMatrix;
    mat4 ViewMatrix;
};

vec4 GetNoise()
{
	ivec2 screenCoords = ivec2(gl_FragCoord.xy);
	int x = screenCoords.x % 4;
	int y = screenCoords.y % 4;
	
	return Noise[ y * 4 + x];
}

void main()
{
	vec3 normal = (ViewMatrix * texture(Normals, uv.st)).xyz;
	vec3 position = (ViewMatrix * texture(Positions, uv.st)).xyz;
	vec3 randomVec = GetNoise().xyz;
	vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
	vec3 bitangent = cross(normal, tangent);
	mat3 TBN = mat3(tangent, bitangent, normal);
	
	float radius = 0.25f;
	ao = 0.0f;
	float bias = 0.015f;
	float maxRange = -1.0f;
	float kernelSize = 64.0f;
	for(int i = 0; i < int(kernelSize); ++i)
	{
		vec3 s = TBN * Samples[i].xyz;
		s = position + s * radius;
		vec4 offset = ProjMatrix * vec4(s, 1.0f);
		offset.xyz /= offset.w;
		offset.xyz = (  offset.xyz + 1.0f ) / 2.0f;
		float depthSample = (ViewMatrix * texture(Positions, offset.xy)).z;
		debug = vec4(depthSample);
		float rangeCheck = smoothstep(0.0f, 1.0f, radius / abs(position.z - depthSample));
		maxRange = max(maxRange, rangeCheck);
		ao += ((depthSample >= s.z + bias)? 1.0f : 0.0f)  * rangeCheck;
	}
	
	ao = 1.0f - ao / kernelSize;
	
}