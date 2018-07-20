#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) out vec4 out_color;
layout(location=1) out vec4 debug;


layout(set=0, binding=0) uniform sampler2D SSRayTracing_T;
layout(set=0, binding=1) uniform sampler2D Light_T;
layout(set=0, binding=2) uniform sampler2D Normal_T;
layout(set=0, binding=3) uniform sampler2D Specular_T;
layout(set=0, binding=4) uniform sampler2D Position_T;

layout(location=0) in vec4 uv;

layout(push_constant) uniform SSResolveConts
{
	mat4 ViewMatrix;
};


void main()
{
	ivec2 fragCoords = ivec2(gl_FragCoord.xy);
	vec3 normalWS = texelFetch(Normal_T, fragCoords, 0).xyz;
	
	
	if (all(equal(normalWS, vec3(0.0f))))
		discard;
	
	vec4 ray = texture(SSRayTracing_T, uv.st);
	if (ray.w <= 0.0f) //falback color
	{
		out_color = vec4(0.0f, 0.0f, 0.6f, 0.0f);
		return;
	}
	
	float roughness = texelFetch(Specular_T, fragCoords, 0).x;
	vec3 positionVS = vec3(ViewMatrix * texelFetch(Position_T, fragCoords, 0));
	vec3 lightColor = vec3(texelFetch(Light_T, ivec2(ray.st), 0));
	vec3 viewDir = normalize(positionVS);
	
	float maxDistance = 7.5f;
	float fadeOnDistance = 1.0f - clamp(abs(ray.z - positionVS.z) / maxDistance, 0.0f, 1.0f);
	float fadeOnRoughness = 1.0f - roughness;
	float fadeOnPerpendicular = clamp(ray.w * 4.0f, 0.0f, 1.0f);
	float totalFade = /* fadeOnDistance * */ fadeOnRoughness /* * fadeOnPerpendicular */;
	
	out_color =  vec4(lightColor, totalFade);
	debug = vec4(positionVS, fadeOnRoughness);
}