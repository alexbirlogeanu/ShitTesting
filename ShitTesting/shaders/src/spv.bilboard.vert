#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

/* layout(std140, set=0, binding=0) uniform params
{
		vec4 bilboardWorldPosition;
        vec4 cameraWorldPosition;
        mat4 projViewMatrix;
        float       size;
}in_param;

vec2 offset[] = {vec2(-1, -1),
	vec2(-1, 1),
	vec2(1, 1),
	vec2(1, -1),

};

const vec3 bilboardUpDir = vec3(.0f, 1.0f, .0f);

void main()
{
	vec3 bilboardFaceDir = normalize((in_param.cameraWorldPosition - in_param.bilboardWorldPosition).xyz);
	vec3 bilboardRightDir = normalize(cross(bilboardUpDir, bilboardFaceDir));	
	vec3 newPos = in_param.bilboardWorldPosition.xyz;
	newPos += offset[gl_VertexIndex].x * in_param.size * bilboardRightDir;
	newPos += offset[gl_VertexIndex].y * in_param.size * bilboardUpDir;
	
	gl_Position = in_param.projViewMatrix * vec4(newPos, 1.0f);
	//gl_Position = vec4(offset[gl_VertexIndex] * in_param.size, .1f, 1.0f) * vec4(1, -1, 1, 1);
} */

layout(location=0) in vec3 in_position;
layout(location=1) in vec2 in_uv;
#define SIZE 100

struct particle_t
{
	vec4   Velocity; //not needed. only in compute shader
    vec4   Position;
    vec4   Color;
	vec4   Properties; //x - life spawn, y - current life span, z - fade time, w - size
};
layout(set=0, binding=0) uniform ParticleParams
{
	vec4 CameraPos;
	vec4 CameraUp;
	vec4 CameraRight;
	mat4 ProjViewMatrix;
};

layout(set=1, binding=0) buffer ParticleBuffer
{
	particle_t particles[SIZE];
};

layout(location=0) out vec4 UV;
layout(location=1) out vec4 LifeSpawnInfo;
void main()
{ 
	vec3 worldPos = particles[gl_InstanceIndex].Position.xyz;
	vec3 bbDir = CameraPos.xyz - worldPos;
	float size = particles[gl_InstanceIndex].Properties.w;
	
	//const value is right in bilboard space
	worldPos += in_position.x * size * -CameraRight.xyz;
	worldPos += in_position.y * size * CameraUp.xyz;
	
	LifeSpawnInfo = vec4(particles[gl_InstanceIndex].Properties.xyz, 0.0f);
	
	gl_Position = ProjViewMatrix * vec4(worldPos, 1.0f);
	UV = vec4(in_uv, 0.0f, 0.0f);
}