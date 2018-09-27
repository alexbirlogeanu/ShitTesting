#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(std140, set = 0, binding = 0) uniform in_params
{
	mat4 ViewProjMatrix;
	mat4 worldMatrix;
	vec4 materialProp; //x = roughness, y = k, z = F0
	vec4 viewPos;
};

layout(location=0) in vec4 te_material[];
layout(location=1) in vec2 te_uv[];

layout(location=0) out vec4 fg_normal;
layout(location=1) out vec4 fg_worldPos;
layout(location=2) out vec4 fg_material;
layout(location=3) out vec2 fg_uv;

vec3 ComputeNormal(vec3 o, vec3 p1, vec3 p2)
{
	vec3 e1 = p1 - o;
	vec3 e2 = p2 - o;
	vec3 normal = vec3(e1.y * e2.z, e1.z * e2.x, e1.x * e2.y) - vec3(e1.z * e2.y, e1.x * e2.z, e1.y * e2.x);
	
	return normalize(normal);

}

void main()
{
	vec4 normals[3];
	normals[0] = vec4(ComputeNormal(gl_in[0].gl_Position.xyz, gl_in[2].gl_Position.xyz,gl_in[1].gl_Position.xyz), 0.0f);
	normals[1] = vec4(ComputeNormal(gl_in[1].gl_Position.xyz, gl_in[0].gl_Position.xyz,gl_in[2].gl_Position.xyz), 0.0f);
	normals[2] = vec4(ComputeNormal(gl_in[2].gl_Position.xyz, gl_in[1].gl_Position.xyz,gl_in[0].gl_Position.xyz), 0.0f);
	
	for (uint i = 0; i < 3; ++i)
	{
		fg_normal = normals[i];
		fg_worldPos = gl_in[i].gl_Position;
		fg_material = te_material[i];
		fg_uv = te_uv[i];
		
		gl_Position = ViewProjMatrix * fg_worldPos;
		EmitVertex();
	}
	EndPrimitive();
}