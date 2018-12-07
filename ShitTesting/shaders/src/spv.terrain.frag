#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) out vec4 albedo;
layout(location=1) out vec4 out_specular;
layout(location=2) out vec4 out_normal;
layout(location=3) out vec4 out_position;

layout(std140, set = 0, binding = 0) uniform TerrainParams
{
	mat4 ViewProjMatrix;
	mat4 WorldMatrix;
	vec4 MaterialProp; //x = roughness, y = k, z = F0
	vec4 TesselationParams; //x - outter, y - inner tessellation, z - tessellation factor
	vec4 PatchParams; //xy - number of cells that are in terrain texture patch, zw - total number of cells in a terrain grid
};

layout(set = 0, binding = 1) uniform sampler2D SplatterTexture;
layout(set = 0, binding = 2) uniform sampler2D Textures[3];


layout(location=0) in vec4 normal;
layout(location=1) in vec4 material;
layout(location=2) in vec2 uv;
layout(location=3) in vec4 worldPos;
layout(location=4) in vec4 debug;

float Depth()
{
	float z = gl_FragCoord.z;
	float near = 0.1;
	float far = 75.0f;
	return (2 * near) / (far + near - z * (far - near));
}

vec2 GetPatchCoordinates()
{
	vec2 uvPatchRange = PatchParams.xy / PatchParams.zw; // from global uv mapping 
	ivec2 patchNumber = ivec2(uv / uvPatchRange);
	vec2 patchMinRange = patchNumber * uvPatchRange;
	vec2 patchMaxRange = (patchNumber + 1) * uvPatchRange;
	
	return (uv - patchMinRange) / (patchMaxRange - patchMinRange);
	
}

vec4 SplatTextures()
{
	vec2 patchUV = GetPatchCoordinates();
	vec2 lod[3];
	for (int i = 0; i < 3; ++i)
		lod[i] = vec2(0.0f);//textureQueryLod(Textures[0], patchUV); //keep it at lod 0
		
	vec3 coef = texture(SplatterTexture, uv).rgb;
	vec3 finalColor = coef.r * textureLod(Textures[0], patchUV, lod[0].x).rgb + coef.g * texture(Textures[1], patchUV, lod[0].x).rgb + coef.b * texture(Textures[2], patchUV, lod[0].x).rgb;
	
	return vec4(finalColor, 1.0f);
}

void main()
{  
	albedo = SplatTextures();
	//albedo = debug;
	out_normal = normal;
	out_position = worldPos;
	out_specular = material;
	
	gl_FragDepth = Depth();
}
